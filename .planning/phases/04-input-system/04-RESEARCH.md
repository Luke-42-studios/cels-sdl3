# Phase 4: Input System - Research

**Researched:** 2026-03-20
**Domain:** SDL3 event polling, per-window event queuing as ECS components, window event routing
**Confidence:** HIGH

## Summary

Phase 4 replaces the existing `SDL3_EventPumpSystem` with a unified system that polls SDL events, routes window-specific events to the correct window entity's state machine, and buffers all other events into per-window queues as ECS components. The SDL3 event API is well-documented and stable -- `SDL_PollEvent` destructively drains events, making a single-pass system the only viable approach. Every SDL3 input event (keyboard, mouse, etc.) carries a `windowID` field identifying which window had focus, enabling direct per-window routing without needing `SDL_GetKeyboardFocus()` or `SDL_GetMouseFocus()`.

The core challenge is designing the per-window event queue component. SDL_Event is a 128-byte union, so a queue of 64 events consumes 8 KiB per window. A fixed-capacity array is appropriate for single-frame event buffers (events are cleared each frame, and a typical frame generates fewer than 64 events). The queue component attaches to the same entity as `SDL3_WindowComponent`, making it queryable via standard CELS `cel_query`/`cel_each` patterns.

**Primary recommendation:** Implement a new `SDL3_EventQueue` component with a fixed-size `SDL_Event events[64]` array and `int count` field, attached to each window entity. A single `SDL3_InputSystem` (OnLoad phase) replaces `SDL3_EventPumpSystem` -- it clears all queues, polls all events, routes window events to `sdl3_window_handle_event`, routes `SDL_EVENT_QUIT` to `cel_quit()`, and pushes everything else into the matching window's queue by `windowID`.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| SDL3 | 3.4.2 | Event polling, event types | Already in project, provides SDL_PollEvent/SDL_WaitEvent/SDL_Event |

No additional libraries needed. All event handling is built into SDL3 core.

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| (none) | - | - | Event system is pure SDL3 + CELS ECS |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Fixed array (64 events) | Dynamic realloc array | Fixed is simpler, 8 KiB per window is negligible, avoids malloc per frame |
| Fixed array (64 events) | Ring buffer | Ring buffer adds complexity for no benefit when queue is cleared every frame |
| Per-window queues | Single global queue | Per-window aligns with ECS entity model and context decision |

## Architecture Patterns

### Recommended Project Structure
```
src/
  input/
    sdl3_input.c          # NEW: event drain, queue management, window routing
  window/
    sdl3_window.c         # EXISTING: unchanged (sdl3_window_handle_event stays here)
  loop/
    sdl3_loop.c           # EXISTING: unchanged
  sdl3_module.c           # MODIFIED: replace EventPumpSystem, add InputSystem + EventQueue component
  sdl3_internal.h         # MODIFIED: add input function declarations
include/
  cels_sdl3.h            # MODIFIED: add SDL3_EventQueue component type
examples/
  input/
    main.c                # NEW: input system example
```

### Pattern 1: Per-Window Event Queue as ECS Component
**What:** Each window entity gets an `SDL3_EventQueue` component containing a fixed array of raw `SDL_Event` structs and a count. The component is attached during window creation and cleared at the start of each frame.
**When to use:** Always -- this is the only event consumption model for Phase 4.
**Example:**
```c
// In cels_sdl3.h (public header)
#define SDL3_EVENT_QUEUE_CAPACITY 64

CEL_Component(SDL3_EventQueue) {
    SDL_Event events[SDL3_EVENT_QUEUE_CAPACITY];
    int       count;
};
```

### Pattern 2: Single-Pass Event Drain with Inline Routing
**What:** One system polls all SDL events in a single `while (SDL_PollEvent)` loop. Each event is classified: window events route to `sdl3_window_handle_event`, quit triggers `cel_quit()`, all others push into the per-window queue matched by `windowID`.
**When to use:** Every frame. Replaces Phase 3's `SDL3_EventPumpSystem`.
**Example:**
```c
// In sdl3_input.c -- called from the system in sdl3_module.c
void sdl3_input_drain_events(cels_entity_t wc_id, cels_entity_t eq_id) {
    // Phase 1: Clear all queues
    // Phase 2: Check minimized-pause (SDL_WaitEvent if all minimized)
    // Phase 3: while (SDL_PollEvent(&event)) { classify and route }
}
```

### Pattern 3: windowID-Based Event Routing
**What:** SDL3 keyboard, mouse, and other input events carry a `windowID` field identifying which window had focus when the event occurred. The drain system iterates window entities to find the matching `SDL3_WindowComponent->window_id` and pushes the event into that entity's `SDL3_EventQueue`.
**When to use:** For all non-window, non-quit events.
**Key insight:** No need to call `SDL_GetKeyboardFocus()` or `SDL_GetMouseFocus()` -- the event itself carries the target window ID.

### Pattern 4: Window Event Classification
**What:** Window events that affect the state machine are intercepted and routed to `sdl3_window_handle_event` -- they never enter the raw queue. This includes close, resize, minimize, restore, and now also focus gain/loss (per CONTEXT.md).
**When to use:** During the drain loop, before queue insertion.
**Key insight:** The existing `sdl3_window_handle_event` function in `sdl3_window.c` needs to be extended to handle focus events (SDL_EVENT_WINDOW_FOCUS_GAINED, SDL_EVENT_WINDOW_FOCUS_LOST) if those should affect the state machine. Per CONTEXT.md, these are intercepted but the current state machine enum does not include a focused/unfocused state -- they may simply be routed without state change.

### Anti-Patterns to Avoid
- **Two-system event pipeline:** SDL_PollEvent is destructive -- you cannot have one system poll and another system consume from a separate queue. All polling and routing must happen in a single system.
- **Abstracting SDL_Event:** Do not create wrapper types for keyboard/mouse events. The CONTEXT explicitly requires raw `SDL_Event` structs with zero abstraction.
- **Convenience macros for queue iteration:** CONTEXT explicitly says "no convenience macros." Consumer systems write their own for-loop + switch.
- **malloc per frame:** The queue is fixed-size, cleared every frame. Dynamic allocation adds complexity and fragmentation for no benefit.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Event polling | Custom event loop | SDL_PollEvent / SDL_WaitEvent | SDL handles platform-specific event sources |
| Window ID lookup | HashMap of window IDs | Linear scan of cel_each(SDL3_WindowComponent) | Few windows (typically 1-3), linear scan is simpler and faster |
| Event type classification | Bitmask/range check | switch on event.type | SDL3 event types are sparse enums, not contiguous ranges |
| Keyboard key codes | Custom key enum | SDL_Keycode / SDL_Scancode | SDL3 provides complete, platform-aware key mapping |

**Key insight:** The entire input system is a thin routing layer between SDL_PollEvent and ECS components. There is no domain logic to implement -- just plumbing.

## Common Pitfalls

### Pitfall 1: Forgetting to Clear Queues Before Polling
**What goes wrong:** Events from the previous frame accumulate, causing duplicate processing or stale data.
**Why it happens:** The clear step must happen before the poll loop, but it's easy to forget when refactoring.
**How to avoid:** The drain function's first action is always clearing all queues. Make this a clearly documented phase in the function.
**Warning signs:** Events appear to fire twice, or stale keyboard state persists after key release.

### Pitfall 2: Queue Overflow on High-Event Frames
**What goes wrong:** More than 64 events arrive in a single frame (e.g., rapid mouse movement at high polling rates), and events are silently dropped.
**Why it happens:** Fixed-capacity array with no overflow handling.
**How to avoid:** Add a bounds check: `if (queue->count < SDL3_EVENT_QUEUE_CAPACITY)`. Log a warning on overflow in debug builds. 64 events per window per frame is generous -- typical games see 5-20 events per frame.
**Warning signs:** Missed input events during fast mouse movement or rapid key presses.

### Pitfall 3: Events with windowID == 0
**What goes wrong:** Some SDL3 events (e.g., SDL_EVENT_QUIT, app lifecycle events, device add/remove) have windowID == 0 because they are not associated with any window.
**Why it happens:** Not all events originate from a window context.
**How to avoid:** SDL_EVENT_QUIT is handled specially (calls cel_quit). Other windowID==0 events can be dropped or routed to a designated "global" queue. For Phase 4, dropping them is acceptable since only keyboard and mouse events matter.
**Warning signs:** Events silently disappear; no window entity matches windowID 0.

### Pitfall 4: Per-TU Static ID Constraint
**What goes wrong:** Using CEL_Component or CEL_System macros in sdl3_input.c instead of sdl3_module.c causes component IDs to be uninitialized (0) at runtime.
**Why it happens:** CELS generates static per-TU `_id` variables. Only the TU that calls `cels_register` gets its IDs initialized.
**How to avoid:** All CELS declarations (CEL_System, CEL_Component) stay in sdl3_module.c. sdl3_input.c receives component IDs as function parameters (same pattern as sdl3_window.c).
**Warning signs:** Segfault or assertion failure when accessing component data; component_id is 0.

### Pitfall 5: Minimized-Pause Logic Must Move to New System
**What goes wrong:** The minimized-pause behavior (SDL_WaitEvent when all windows minimized) from Phase 3's EventPumpSystem is lost during the replacement.
**Why it happens:** The new InputSystem replaces EventPumpSystem entirely. If the WaitEvent logic is not migrated, the app will busy-loop when minimized.
**How to avoid:** Copy the minimized-pause check from the existing EventPumpSystem into the new system. It runs before the normal poll loop.
**Warning signs:** CPU usage stays at 100% when all windows are minimized.

### Pitfall 6: SDL3_EventQueue Component Must Be Registered Before Window Creation
**What goes wrong:** If the EventQueue component is not registered when a window entity is created, the queue cannot be attached to the window entity.
**Why it happens:** Component registration order matters in CELS.
**How to avoid:** Register SDL3_EventQueue in the module init alongside SDL3_WindowComponent. The queue is attached to the window entity during window creation (in sdl3_window_create or via composition).
**Warning signs:** Window entity exists but has no EventQueue component.

## Code Examples

### Event Queue Component Definition
```c
// Source: cels_sdl3.h (public header)
#define SDL3_EVENT_QUEUE_CAPACITY 64

CEL_Component(SDL3_EventQueue) {
    SDL_Event events[SDL3_EVENT_QUEUE_CAPACITY];
    int       count;
};
```

### Event Drain Function Signature
```c
// Source: sdl3_internal.h
// Called from SDL3_InputSystem in sdl3_module.c
// Receives component IDs because sdl3_input.c cannot use CELS macros (per-TU constraint)
extern void sdl3_input_clear_queues(cels_entity_t eq_id);
extern void sdl3_input_drain_events(cels_entity_t wc_id, cels_entity_t eq_id);
```

### System Definition in sdl3_module.c
```c
// Replaces SDL3_EventPumpSystem
CEL_System(SDL3_InputSystem, .phase = OnLoad) {
    cel_run {
        sdl3_input_drain_events(SDL3_WindowComponent_id, SDL3_EventQueue_id);
    }
}
```

### Event Queue Push (in sdl3_input.c)
```c
// Push an event into a window's queue (bounds-checked)
static void push_event(SDL3_EventQueue* queue, const SDL_Event* event) {
    if (queue->count < SDL3_EVENT_QUEUE_CAPACITY) {
        queue->events[queue->count++] = *event;
    }
    // else: overflow -- silently drop (or log in debug)
}
```

### Consumer System Pattern (example app)
```c
// How a consumer reads the event queue -- no convenience macros
CEL_System(KeyboardHandler, .phase = OnUpdate) {
    cel_query(SDL3_EventQueue);
    cel_each(SDL3_EventQueue) {
        for (int i = 0; i < SDL3_EventQueue->count; i++) {
            const SDL_Event* e = &SDL3_EventQueue->events[i];
            switch (e->type) {
            case SDL_EVENT_KEY_DOWN:
                if (e->key.key == SDLK_ESCAPE) {
                    cel_quit();
                }
                break;
            default:
                break;
            }
        }
    }
}
```

### Minimized-Pause Logic (migrated from Phase 3)
```c
// Check if all windows are minimized -- block with SDL_WaitEvent if so
static bool check_minimized_pause(cels_entity_t wc_id) {
    // Uses cels_entity_get_component to iterate windows
    // Returns true if SDL_WaitEvent was called (skip normal poll loop)
    // Same logic as existing EventPumpSystem lines 73-117
}
```

### Attaching EventQueue During Window Creation
```c
// In sdl3_window_create (or in the composition)
// After creating SDL3_WindowComponent, also attach empty SDL3_EventQueue
SDL3_EventQueue empty_queue = { .count = 0 };
cels_entity_set_component(entity, eq_id, &empty_queue, sizeof(empty_queue));
```

## State of the Art

| Old Approach (Phase 3) | Current Approach (Phase 4) | Impact |
|------------------------|---------------------------|--------|
| SDL3_EventPumpSystem ignores non-window events | SDL3_InputSystem buffers all non-window events into per-window queues | Consumer systems can now read keyboard/mouse input |
| No event queue component | SDL3_EventQueue component on each window entity | Events are ECS-native, queryable by any system |
| Window events hardcoded in event pump | Window events routed through sdl3_window_handle_event (same function, expanded event set) | Focus events now also routed |

**SDL3 vs SDL2 differences relevant to this phase:**
- SDL3 window events are top-level event types (SDL_EVENT_WINDOW_CLOSE_REQUESTED), not subtypes of a single SDL_WINDOWEVENT -- no need to check event.window.event subtype
- SDL3 keyboard events use `event.key.key` (SDL_Keycode) and `event.key.scancode` (SDL_Scancode), plus `event.key.down` bool and `event.key.repeat` bool
- SDL3 mouse events use float coordinates (`event.motion.x`, `event.button.x`) not integer
- SDL_Event is 128 bytes (padded for ABI compatibility)

## Open Questions

1. **Focus events and window state machine**
   - What we know: CONTEXT says focus gain/loss should be intercepted and routed to window entity's state machine. The current `SDL3_WindowState` enum has no FOCUSED/UNFOCUSED states.
   - What's unclear: Should focus events trigger a state transition, or just be intercepted and handled without changing the state enum?
   - Recommendation: Route focus events to `sdl3_window_handle_event` but have them be no-ops for state transitions (the default branch already handles this). Do NOT add them to the raw queue. This matches CONTEXT.md's "never enter raw queue" requirement.

2. **Events without a matching window entity**
   - What we know: Some events (device add/remove, app lifecycle) have windowID == 0. Keyboard/mouse events from a window that has already been destroyed could reference a stale windowID.
   - What's unclear: Should these be dropped silently or routed somewhere?
   - Recommendation: Drop silently for Phase 4. No global queue needed. This can be revisited in Phase 9 (module integration) if needed.

3. **SDL3_EventQueue component size**
   - What we know: 64 events * 128 bytes = 8,192 bytes per component. This is stored in the ECS archetype storage.
   - What's unclear: Whether the underlying ECS (Flecs) handles components of this size efficiently.
   - Recommendation: Flecs stores components in contiguous arrays per archetype. 8 KiB per entity is fine for 1-3 windows. If profiling shows issues, capacity can be reduced to 32 (4 KiB). 64 is a safe starting point.

## Sources

### Primary (HIGH confidence)
- SDL3 Official Wiki: [SDL_PollEvent](https://wiki.libsdl.org/SDL3/SDL_PollEvent) - Event polling API
- SDL3 Official Wiki: [SDL_Event](https://wiki.libsdl.org/SDL3/SDL_Event) - Union structure (128 bytes, all member types)
- SDL3 Official Wiki: [SDL_EventType](https://wiki.libsdl.org/SDL3/SDL_EventType) - Complete event type enumeration
- SDL3 Official Wiki: [SDL_KeyboardEvent](https://wiki.libsdl.org/SDL3/SDL_KeyboardEvent) - Keyboard event fields (windowID, key, scancode, down, repeat)
- SDL3 Official Wiki: [SDL_WindowEvent](https://wiki.libsdl.org/SDL3/SDL_WindowEvent) - Window event fields (windowID, data1, data2)
- SDL3 Official Wiki: [SDL_MouseButtonEvent](https://wiki.libsdl.org/SDL3/SDL_MouseButtonEvent) - Mouse button fields (windowID, button, down, clicks, x, y)
- SDL3 Official Wiki: [SDL_MouseMotionEvent](https://wiki.libsdl.org/SDL3/SDL_MouseMotionEvent) - Mouse motion fields (windowID, state, x, y, xrel, yrel)
- SDL3 Official Wiki: [SDL_GetWindowFromID](https://wiki.libsdl.org/SDL3/SDL_GetWindowFromID) - Window lookup by ID
- Existing codebase: `src/sdl3_module.c` lines 71-150 - Current EventPumpSystem implementation
- Existing codebase: `src/window/sdl3_window.c` lines 67-105 - Current window event handler
- Existing codebase: `include/cels_sdl3.h` - Component definitions and public API
- CELS framework: `include/cels/cels.h` - ECS component, system, and state macros

### Secondary (MEDIUM confidence)
- SDL3 Official Wiki: [SDL_GetKeyboardFocus](https://wiki.libsdl.org/SDL3/SDL_GetKeyboardFocus) / [SDL_GetMouseFocus](https://wiki.libsdl.org/SDL3/SDL_GetMouseFocus) - Confirmed windowID on events makes these unnecessary for routing

### Tertiary (LOW confidence)
- SDL Forum: [Event queue size/limits](https://discourse.libsdl.org/t/event-queue-size-limits/24041) - SDL internal queue limit discussion (informational, not directly relevant)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - SDL3 event API is stable, documented, and already used in Phase 3
- Architecture: HIGH - Constrained by CONTEXT.md decisions (per-window queues, raw SDL_Event, no wrappers)
- Pitfalls: HIGH - Derived from existing codebase patterns and known SDL3 API behavior
- Code examples: HIGH - Based on existing codebase conventions and verified SDL3 API docs

**Research date:** 2026-03-20
**Valid until:** 2026-04-20 (SDL3 event API is stable)
