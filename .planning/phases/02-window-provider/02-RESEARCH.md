# Phase 2: Window Provider - Research

**Researched:** 2026-03-15
**Domain:** SDL3 window creation/lifecycle, ECS window entity patterns, CELS lifecycle/observer integration
**Confidence:** HIGH

## Summary

Phase 2 creates window entities with SDL3 windows and a full lifecycle state machine. The research investigated five areas: (1) SDL3 3.4.2 window creation and event APIs verified from the locally fetched headers, (2) CELS framework patterns for components, lifecycles, observers, state, compositions, and systems verified from the cels.h source, (3) the cels-ncurses window module as the direct precedent, (4) multi-window event routing via SDL_WindowID, and (5) headless testing with the dummy video driver.

Key findings: SDL3's `SDL_CreateWindow(title, w, h, flags)` takes title/width/height/flags directly (no position parameter -- position is controlled via `SDL_SetWindowPosition()` or `SDL_CreateWindowWithProperties()`). Window events in SDL3 are individual event types (e.g., `SDL_EVENT_WINDOW_CLOSE_REQUESTED`, `SDL_EVENT_WINDOW_RESIZED`, `SDL_EVENT_WINDOW_MINIMIZED`) not subtypes of a single window event. Each `SDL_WindowEvent` carries a `windowID` field for multi-window routing via `SDL_GetWindowFromID()`. The dummy video driver (`SDL_VIDEODRIVER=dummy`) supports `SDL_CreateWindow()` with framebuffer support, and `SDL_PushEvent()` allows injecting synthetic events for testing.

**Primary recommendation:** Follow the cels-ncurses module pattern exactly: `CEL_Component` for WindowConfig and WindowState, `CEL_Lifecycle` + `CEL_Observe(on_create/on_destroy)` for window spawn/destroy, a `CEL_System` at OnLoad phase to poll window state and drive transitions. Store the `SDL_Window*` and `SDL_WindowID` on the component. Multi-window routing will happen in Phase 4 (Input System) using `SDL_WindowID` from events.

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| SDL3 | 3.4.2 | `SDL_CreateWindow`, `SDL_DestroyWindow`, window flags, events | Already fetched in Phase 1 |
| CELS | 0.5.3 | `CEL_Component`, `CEL_Lifecycle`, `CEL_Observe`, `CEL_System`, `CEL_Composition` | Framework -- all ECS patterns |

### Supporting

No additional libraries needed for this phase. Window creation is pure SDL3 + CELS.

### Key SDL3 APIs for This Phase

| API | Purpose | Confidence |
|-----|---------|------------|
| `SDL_CreateWindow(title, w, h, flags)` | Create a window (returns `SDL_Window*` or NULL) | HIGH -- verified from SDL3 3.4.2 header |
| `SDL_DestroyWindow(window)` | Destroy a window | HIGH -- verified from header |
| `SDL_GetWindowID(window)` | Get numeric ID for event routing | HIGH -- verified from header |
| `SDL_GetWindowFromID(id)` | Resolve numeric ID back to window pointer | HIGH -- verified from header |
| `SDL_GetWindowFlags(window)` | Query current window state flags | HIGH -- verified from header |
| `SDL_GetWindowSize(window, &w, &h)` | Query current window dimensions | HIGH -- verified from header |
| `SDL_SetWindowSize(window, w, h)` | Change window dimensions | HIGH -- verified from header |
| `SDL_SetWindowTitle(window, title)` | Change window title | HIGH -- verified from header |
| `SDL_SetWindowPosition(window, x, y)` | Change window position | HIGH -- verified from header |
| `SDL_SetWindowResizable(window, bool)` | Toggle resizable flag | HIGH -- verified from header |
| `SDL_SetWindowFullscreen(window, bool)` | Toggle fullscreen | HIGH -- verified from header |
| `SDL_SetWindowBordered(window, bool)` | Toggle window decoration | HIGH -- verified from header |
| `SDL_SetWindowAlwaysOnTop(window, bool)` | Toggle always-on-top | HIGH -- verified from header |
| `SDL_MinimizeWindow(window)` | Minimize the window | HIGH -- verified from header |
| `SDL_RestoreWindow(window)` | Restore from minimize/maximize | HIGH -- verified from header |
| `SDL_PushEvent(event)` | Inject synthetic events (for testing) | HIGH -- verified from header |

### Key SDL3 Window Events

All are individual `SDL_EventType` values (not subtypes), each carrying `SDL_WindowEvent` struct with `windowID`:

| Event | Maps To State | Notes |
|-------|---------------|-------|
| `SDL_EVENT_WINDOW_SHOWN` | Informational | Window is now visible |
| `SDL_EVENT_WINDOW_RESIZED` | RESIZING | `data1` = new width, `data2` = new height |
| `SDL_EVENT_WINDOW_MINIMIZED` | MINIMIZED | Window was minimized |
| `SDL_EVENT_WINDOW_RESTORED` | Re-enter SURFACE_READY->READY chain | Window restored from minimize/maximize |
| `SDL_EVENT_WINDOW_CLOSE_REQUESTED` | CLOSING | User clicked close button / alt-F4 |
| `SDL_EVENT_WINDOW_DESTROYED` | CLOSED | Window has been destroyed by SDL |
| `SDL_EVENT_WINDOW_EXPOSED` | Informational | Window needs redraw |
| `SDL_EVENT_WINDOW_FOCUS_GAINED` | Informational | Not a state change |
| `SDL_EVENT_WINDOW_FOCUS_LOST` | Informational | Not a state change |

### SDL3 Window Flags (for WindowConfig)

| Flag | Value | Purpose |
|------|-------|---------|
| `SDL_WINDOW_RESIZABLE` | `0x20` | Window can be resized (default per context decisions) |
| `SDL_WINDOW_BORDERLESS` | `0x10` | No window decoration |
| `SDL_WINDOW_FULLSCREEN` | `0x01` | Fullscreen mode |
| `SDL_WINDOW_HIDDEN` | `0x08` | Start hidden |
| `SDL_WINDOW_ALWAYS_ON_TOP` | `0x10000` | Always on top |
| `SDL_WINDOW_MINIMIZED` | `0x40` | Start minimized |
| `SDL_WINDOW_MAXIMIZED` | `0x80` | Start maximized |
| `SDL_WINDOW_HIGH_PIXEL_DENSITY` | `0x2000` | HiDPI support |

**Note:** `SDL_WindowFlags` is `Uint64` in SDL3 (not `Uint32` like SDL2).

## Architecture Patterns

### Recommended File Structure

```
src/
  sdl3_module.c         # All CELS declarations (existing, extend with window)
  sdl3_init.c           # Context init/shutdown (existing)
  sdl3_internal.h       # Internal declarations (existing, extend)
  window/
    sdl3_window.c       # Window creation, destruction, state machine logic
include/
  cels_sdl3.h           # Public API (existing, extend with window types)
```

### Pattern 1: Window as ECS Component (follow cels-ncurses pattern)

**What:** Each window is an ECS entity with a `SDL3_WindowConfig` component (creation config) and a `SDL3_WindowComponent` component (runtime state including SDL_Window*, state machine, dimensions).

**When to use:** Always -- this is the locked decision from CONTEXT.md.

**Example:**
```c
// Source: cels-ncurses pattern + SDL3 API verified from headers

// --- In cels_sdl3.h (public header) ---

// State machine enum
typedef enum SDL3_WindowState {
    SDL3_WINDOW_NONE = 0,
    SDL3_WINDOW_CREATED,
    SDL3_WINDOW_SURFACE_READY,
    SDL3_WINDOW_READY,
    SDL3_WINDOW_RESIZING,
    SDL3_WINDOW_MINIMIZED,
    SDL3_WINDOW_CLOSING,
    SDL3_WINDOW_CLOSED
} SDL3_WindowState;

// Creation config -- developer fills this
CEL_Component(SDL3_WindowConfig) {
    const char* title;
    int         width;
    int         height;
    SDL_WindowFlags flags;  // Full SDL3 flags exposed
};

// Runtime state -- attached by lifecycle observer after creation
CEL_Component(SDL3_WindowComponent) {
    SDL_Window*      window;     // Raw SDL3 pointer (exposed per context decisions)
    SDL_WindowID     window_id;  // For event routing
    SDL3_WindowState state;      // Current lifecycle state
    int              width;      // Current dimensions
    int              height;
};
```

### Pattern 2: Lifecycle Observer for Window Creation/Destruction

**What:** `CEL_Lifecycle(SDL3_WindowLC)` with `on_create` and `on_destroy` observers. Creation observer reads `SDL3_WindowConfig`, calls `SDL_CreateWindow()`, attaches `SDL3_WindowComponent`, and advances state to CREATED->SURFACE_READY->READY.

**When to use:** Every window entity creation.

**Example:**
```c
// Source: cels-ncurses CEL_Observe pattern + SDL3 API

// In sdl3_module.c:
CEL_Lifecycle(SDL3_WindowLC);

CEL_Observe(SDL3_WindowLC, on_create) {
    const SDL3_WindowConfig* config = cel_watch(entity, SDL3_WindowConfig);
    if (!config) return;

    // Delegate to sdl3_window.c
    sdl3_window_create(entity, config);
}

CEL_Observe(SDL3_WindowLC, on_destroy) {
    // Delegate to sdl3_window.c
    sdl3_window_destroy(entity);
}
```

### Pattern 3: Composition for Natural Syntax

**What:** `CEL_Define_Composition` in header + `CEL_Composition` in sdl3_module.c, with `#define SDL3Window(...) cel_init(SDL3Window, __VA_ARGS__)` call macro.

**When to use:** All window creation by consumer.

**Example:**
```c
// Source: cels-ncurses NCursesWindow pattern

// In cels_sdl3.h:
CEL_Define_Composition(SDL3Window,
    const char* title;
    int width;
    int height;
    SDL_WindowFlags flags;
);
#define SDL3Window(...) cel_init(SDL3Window, __VA_ARGS__)

// In sdl3_module.c:
CEL_Composition(SDL3Window) {
    cel_has(SDL3_WindowConfig,
        .title  = cel.title,
        .width  = cel.width  ? cel.width  : 1280,
        .height = cel.height ? cel.height : 720,
        .flags  = cel.flags | SDL_WINDOW_RESIZABLE  // resizable by default
    );
    cels_lifecycle_bind_entity(SDL3_WindowLC_id, cels_get_current_entity());
}

// Consumer usage:
SDL3Window(.title = "My Game", .width = 800, .height = 600) {}
SDL3Window(.title = "Debug") {}  // gets 1280x720 default
```

### Pattern 4: State Machine System (runs each frame)

**What:** A `CEL_System` at `OnLoad` phase that queries all `SDL3_WindowComponent` entities and drives state transitions based on current SDL3 window state and incoming events.

**When to use:** Every frame.

**Important design note:** In Phase 2, the window system does NOT poll SDL events directly. Event polling is Phase 3/4. Instead, the Phase 2 state machine system:
- Advances initial state chain (CREATED -> SURFACE_READY -> READY) during creation
- Is designed to be DRIVEN by events routed from Phase 4

However, the system DOES use `SDL_GetWindowFlags()` to detect current window state for the initial creation flow.

```c
// Source: cels-ncurses NCurses_WindowUpdateSystem pattern

CEL_System(SDL3_WindowStateSystem, .phase = OnLoad) {
    cel_query(SDL3_WindowComponent);
    cel_each(SDL3_WindowComponent) {
        // Drive state transitions based on current SDL_Window state
        // Phase 4 will add event-driven transitions
    }
}
```

### Pattern 5: Per-TU Static ID Constraint

**What:** All `CEL_Component`, `CEL_Lifecycle`, `CEL_State`, and `CEL_System` declarations that use `_id` variables MUST live in `sdl3_module.c`. Implementation functions are delegated to helper files (sdl3_window.c) that receive entity IDs and component data as parameters.

**When to use:** Always -- this is a hard CELS framework constraint from Phase 1.

**Why:** `CEL_Component` generates `static` per-TU `_id` vars. `cels_register()` only initializes the IDs in the TU where it runs. If a component ID is used in a different TU, it will be 0 (uninitialized).

**Implication for window code:** `sdl3_window.c` cannot use `cel_watch()`, `cel_has()`, or other macros that reference component IDs. It must receive data via function parameters, and use `cels_entity_set_component()` + `cels_component_notify_change()` with explicit ID parameters for setting components.

### Pattern 6: Change Detection for Reactive Component Updates

**What:** A mechanism to detect when the developer modifies `SDL3_WindowComponent` fields and apply changes to the underlying `SDL_Window`.

**When to use:** Post-creation property modifications (title, size, fullscreen, etc.).

**Design decision (Claude's discretion per CONTEXT.md):**

Use a "previous state snapshot" approach: store a copy of the last-applied values alongside the component. Each frame, the system compares current component values to the snapshot. When a difference is detected, apply the SDL3 call and update the snapshot.

```c
// Internal state -- NOT on the public component
typedef struct SDL3_WindowPrevState {
    int  width;
    int  height;
    bool fullscreen;
    bool borderless;
    bool always_on_top;
    // ... other trackable properties
} SDL3_WindowPrevState;
```

This is simpler than dirty flags and works naturally with ECS iteration. The comparison cost is negligible (a few int/bool comparisons per window per frame).

**Alternative considered:** `cel_update()` block in systems triggers reactivity. But for component-driven window changes, the developer modifies fields outside of `cel_update`, so explicit change detection is more reliable.

### Anti-Patterns to Avoid

- **Global singleton window:** cels-ncurses uses `g_window_entity` as a global. For SDL3 multi-window, there must be no global window state. All state lives on the ECS entity.
- **Direct SDL calls in sdl3_module.c:** Keep the module file purely CELS declarations. SDL3 calls belong in sdl3_window.c.
- **Polling events in the window system:** Phase 2 should NOT call `SDL_PollEvent()`. Event polling is Phase 3's responsibility. Phase 2 only creates/destroys windows and manages state transitions that are triggered during creation or by explicit function calls.
- **Using SDL_CreateWindowAndRenderer:** The SDL3 docs recommend this to avoid flicker, but the renderer is Phase 5. Create window-only in Phase 2.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Window creation | Custom platform window code | `SDL_CreateWindow()` | SDL3 handles X11/Wayland/Win32/Cocoa |
| Window ID mapping | Hash map of window pointers | `SDL_GetWindowID()` / `SDL_GetWindowFromID()` | SDL3 maintains this mapping internally |
| DPI scaling | Custom display detection | `SDL_WINDOW_HIGH_PIXEL_DENSITY` flag + `SDL_GetWindowSizeInPixels()` | SDL3 handles this per-platform |
| Position constants | Magic numbers | `SDL_WINDOWPOS_UNDEFINED` / `SDL_WINDOWPOS_CENTERED` | Portable position semantics |
| Event injection for tests | Custom test hooks | `SDL_PushEvent()` | Built into SDL3, works with dummy driver |

**Key insight:** SDL3's window management is comprehensive. The only custom code needed is the state machine layer on top and the ECS integration.

## Common Pitfalls

### Pitfall 1: SDL_CreateWindow Position Parameter Removed in SDL3

**What goes wrong:** Attempting `SDL_CreateWindow(title, x, y, w, h, flags)` -- this is the SDL2 signature, not SDL3.
**Why it happens:** SDL2 had 6 parameters; SDL3 simplified to 4 (title, w, h, flags). Position is set separately.
**How to avoid:** Use `SDL_CreateWindow(title, w, h, flags)` only. Set position post-creation with `SDL_SetWindowPosition()`.
**Warning signs:** Compilation errors about wrong argument count.

### Pitfall 2: SDL_WindowFlags is Uint64, Not Uint32

**What goes wrong:** Storing flags in an `int` or `uint32_t` truncates high bits.
**Why it happens:** SDL3 expanded the flags type from SDL2's Uint32 to Uint64.
**How to avoid:** Use `SDL_WindowFlags` type (which is `Uint64`) in the config struct.
**Warning signs:** Flags like `SDL_WINDOW_VULKAN` (`0x10000000`) might work, but future flags above bit 31 would be silently lost.

### Pitfall 3: Per-TU Static ID Constraint

**What goes wrong:** Using `cel_watch(entity, SDL3_WindowComponent)` in `sdl3_window.c` returns NULL because the component ID is 0 in that TU.
**Why it happens:** `CEL_Component` generates a `static` ID variable per TU. Only the TU where `cels_register()` runs gets the correct ID.
**How to avoid:** All CELS macro usage (cel_watch, cel_has, etc.) must be in `sdl3_module.c`. Helper files receive raw data via function parameters and use `cels_entity_set_component()` with explicit ID parameters.
**Warning signs:** NULL returns from cel_watch in helper TUs; components not appearing on entities.

### Pitfall 4: CLOSED State Cleanup

**What goes wrong:** Destroying SDL_Window while still processing events for it causes use-after-free.
**Why it happens:** CLOSING -> CLOSED transition destroys the window, but events from that frame may still reference its windowID.
**How to avoid:** The CLOSING state gives one frame for cleanup. `SDL_DestroyWindow()` only happens on the CLOSING -> CLOSED transition (next frame). After CLOSED, the entity can be destroyed with `cel_destroy()`.
**Warning signs:** Crashes during window close, especially with multiple windows.

### Pitfall 5: Dummy Driver Window Surface Behavior

**What goes wrong:** Assuming window surface operations behave identically under dummy driver.
**Why it happens:** The dummy driver has framebuffer support but no real display.
**How to avoid:** Test state machine logic (creation, state transitions) under dummy driver. Don't test visual output.
**Warning signs:** Tests that check pixel content under dummy driver.

### Pitfall 6: Default Flag Combination

**What goes wrong:** Passing `0` for flags creates a non-resizable window, contradicting the "resizable by default" context decision.
**Why it happens:** SDL3 defaults to non-resizable unless `SDL_WINDOW_RESIZABLE` is explicitly set.
**How to avoid:** The `SDL3Window` composition must OR in `SDL_WINDOW_RESIZABLE` unless the developer explicitly provides flags that exclude it.
**Warning signs:** Windows that can't be resized when developer doesn't set flags.

## Code Examples

### Window Creation (sdl3_window.c)

```c
// Source: SDL3 3.4.2 header API verified locally

void sdl3_window_create(cels_entity_t entity, const SDL3_WindowConfig* config) {
    const char* title = (config->title && config->title[0]) ? config->title : "";
    int w = config->width  > 0 ? config->width  : 1280;
    int h = config->height > 0 ? config->height : 720;
    SDL_WindowFlags flags = config->flags;

    // Ensure resizable by default (context decision)
    if (!(flags & SDL_WINDOW_BORDERLESS)) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    SDL_Window* window = SDL_CreateWindow(title, w, h, flags);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return;
    }

    SDL3_WindowComponent comp = {
        .window    = window,
        .window_id = SDL_GetWindowID(window),
        .state     = SDL3_WINDOW_CREATED,
        .width     = w,
        .height    = h
    };

    // Advance through creation chain
    // CREATED -> SURFACE_READY (surface is always ready for SDL_Renderer path)
    comp.state = SDL3_WINDOW_SURFACE_READY;

    // SURFACE_READY -> READY (window is visible and ready)
    comp.state = SDL3_WINDOW_READY;

    // Attach component to entity (via explicit ID, not cel_has)
    cels_entity_set_component(entity, SDL3_WindowComponent_id,
                               &comp, sizeof(comp));
    cels_component_notify_change(SDL3_WindowComponent_id);
}
```

### Window Destruction (sdl3_window.c)

```c
// Source: SDL3 3.4.2 header + cels-ncurses pattern

void sdl3_window_destroy(cels_entity_t entity) {
    // Called from lifecycle on_destroy observer
    // entity parameter lets us retrieve component if needed
    // But the SDL_Window* must be passed or stored since we can't
    // use cel_watch in this TU

    // This function receives the SDL_Window* from the observer
    // which CAN use cel_watch (it's in sdl3_module.c)
}

// Alternative: pass the window pointer directly
void sdl3_window_destroy_ptr(SDL_Window* window) {
    if (window) {
        SDL_DestroyWindow(window);
    }
}
```

### Module Registration (sdl3_module.c additions)

```c
// Source: cels-ncurses ncurses_module.c pattern

// State declarations
CEL_State(SDL3_ContextState);  // existing

// Lifecycle declarations
CEL_Lifecycle(SDL3_ContextLC);  // existing
CEL_Lifecycle(SDL3_WindowLC);   // NEW

// Window lifecycle observers
CEL_Observe(SDL3_WindowLC, on_create) {
    const SDL3_WindowConfig* config = cel_watch(entity, SDL3_WindowConfig);
    if (!config) return;
    sdl3_window_create(entity, config);
}

CEL_Observe(SDL3_WindowLC, on_destroy) {
    const SDL3_WindowComponent* comp = cel_watch(entity, SDL3_WindowComponent);
    if (comp && comp->window) {
        sdl3_window_destroy_ptr(comp->window);
    }
}

// Module init -- extend existing
CEL_Module(SDL3_Engine, init) {
    cels_register(SDL3_ContextState, SDL3_ContextLC,
                  SDL3_ContextConfig);
    // NEW: window registrations
    cels_register(SDL3_WindowConfig, SDL3_WindowComponent,
                  SDL3_WindowLC, SDL3_WindowStateSystem);
}
```

### Consumer Usage

```c
// Source: cels-ncurses minimal.c pattern adapted for SDL3

#include <cels/cels.h>
#include <cels_sdl3.h>

CEL_Compose(World) {
    SDL3Context(.video = true) {}
    SDL3Window(.title = "My Game", .width = 800, .height = 600) {}
}

// Multi-window example:
CEL_Compose(MultiWindowWorld) {
    SDL3Context(.video = true) {}
    SDL3Window(.title = "Main Window") {}
    SDL3Window(.title = "Debug View", .width = 640, .height = 480) {}
}

cels_main() {
    cels_register(SDL3_Engine);
    cels_session(World) {
        while (cels_running()) {
            cels_step(0.016f);
        }
    }
}
```

### Headless Testing

```c
// Source: Phase 1 research + SDL3 PushEvent API

// Set before SDL_Init:
// SDL_VIDEODRIVER=dummy

// Create window under dummy driver -- works, returns valid SDL_Window*
SDL_Window* win = SDL_CreateWindow("test", 100, 100, 0);
assert(win != NULL);

// Inject synthetic events for state machine testing:
SDL_Event ev = {0};
ev.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
ev.window.windowID = SDL_GetWindowID(win);
ev.window.timestamp = SDL_GetTicksNS();
SDL_PushEvent(&ev);

// Poll to retrieve:
SDL_Event polled;
while (SDL_PollEvent(&polled)) {
    if (polled.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        // State machine would transition to CLOSING
    }
}
```

## State of the Art

| Old Approach (SDL2) | Current Approach (SDL3) | When Changed | Impact |
|---------------------|------------------------|--------------|--------|
| `SDL_CreateWindow(title, x, y, w, h, flags)` | `SDL_CreateWindow(title, w, h, flags)` | SDL 3.0 | Position removed from creation call |
| `SDL_WindowFlags` as `Uint32` | `SDL_WindowFlags` as `Uint64` | SDL 3.0 | Must use correct type |
| `SDL_WINDOWEVENT` with subtypes | Individual `SDL_EVENT_WINDOW_*` types | SDL 3.0 | No more subtype dispatch -- each event is its own type |
| `SDL_WINDOWPOS_UNDEFINED` for default pos | Same constant, but set via `SDL_SetWindowPosition()` | SDL 3.0 | Not needed at creation |
| `SDL_WINDOWEVENT_CLOSE` | `SDL_EVENT_WINDOW_CLOSE_REQUESTED` | SDL 3.0 | Clearer naming |
| Bool returns as `int` (0 = success) | `bool` return (true = success) | SDL 3.0 | Check `!result` for failure |

**Deprecated/outdated:**
- `SDL_CreateWindow` 6-parameter overload: Does not exist in SDL3
- `SDL_WINDOWEVENT` event type with subtypes: Replaced by individual event types
- `SDL_SetWindowDisplayMode`: Replaced by `SDL_SetWindowFullscreenMode`

## Open Questions

1. **RESIZING stability threshold**
   - What we know: Context decisions say "stay in RESIZING until stable (no resize events for a threshold)"
   - What's unclear: The exact threshold duration. Claude's discretion per CONTEXT.md.
   - Recommendation: Use 150-200ms. This is long enough to absorb rapid resize events from dragging a window border, but short enough to feel responsive. SDL3 fires `SDL_EVENT_WINDOW_RESIZED` for each intermediate size during drag.

2. **SURFACE_READY state meaning in SDL_Renderer context**
   - What we know: The state chain includes SURFACE_READY between CREATED and READY. In the Vulkan world, this is meaningful (surface creation is a separate step). In SDL_Renderer path, the surface is always available after window creation.
   - What's unclear: Whether SURFACE_READY should be a real checkpoint or instant pass-through.
   - Recommendation: For SDL_Renderer, advance CREATED -> SURFACE_READY -> READY synchronously during creation. When renderer is added in Phase 5, SURFACE_READY becomes the state where the renderer is created. This keeps the state machine correct for future SDL_GPU paths.

3. **Event-driven state machine vs polling-driven**
   - What we know: Phase 2 creates windows; Phase 4 polls events and routes them. Phase 2 should not poll events.
   - What's unclear: How Phase 2's state machine receives event notifications before Phase 4 exists.
   - Recommendation: Phase 2's state system should expose functions like `sdl3_window_handle_event(entity, event_type, data1, data2)` that Phase 4 will call. For Phase 2 testing, use `SDL_PushEvent()` + manual polling in the test app.

4. **Component_id passing to helper TU**
   - What we know: sdl3_window.c needs component IDs to call `cels_entity_set_component()`. IDs are only valid in sdl3_module.c.
   - What's unclear: Best pattern for passing IDs.
   - Recommendation: sdl3_window.c functions accept `cels_entity_t component_id` as a parameter. sdl3_module.c passes `SDL3_WindowComponent_id` when calling. This is what cels-ncurses does with `cels_entity_set_component(entity, TUI_DrawContext_Component_id, &dc, sizeof(dc))`.

## Sources

### Primary (HIGH confidence)
- SDL3 3.4.2 headers -- `SDL_video.h`, `SDL_events.h` -- read directly from `/home/cachy/workspaces/libs/cels-sdl3/build/_deps/sdl3-src/include/SDL3/`
- CELS framework -- `cels.h` -- read directly from `/home/cachy/workspaces/libs/cels/include/cels/cels.h`
- CELS runtime API -- `cels_runtime.h` -- read directly from `/home/cachy/workspaces/libs/cels/include/cels/private/cels_runtime.h`
- cels-ncurses module -- `cels_ncurses.h`, `ncurses_module.c`, `tui_window.c`, `tui_window.h` -- read directly from `/home/cachy/workspaces/libs/cels-ncurses/`
- cels-sdl3 Phase 1 code -- `sdl3_module.c`, `sdl3_init.c`, `sdl3_internal.h`, `cels_sdl3.h` -- read directly from source
- SDL3 dummy video driver -- `SDL_nullvideo.c` -- confirmed framebuffer support from source

### Secondary (MEDIUM confidence)
- [SDL3 SDL_CreateWindow wiki](https://wiki.libsdl.org/SDL3/SDL_CreateWindow) -- WebFetched, confirms 4-parameter signature

### Tertiary (LOW confidence)
- None -- all claims verified from local source files

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all APIs verified from locally fetched SDL3 3.4.2 headers
- Architecture: HIGH -- follows verified cels-ncurses patterns directly
- Pitfalls: HIGH -- derived from comparing SDL2 vs SDL3 API changes in actual headers
- State machine: MEDIUM -- the RESIZING threshold and SURFACE_READY semantics are design decisions, not hard facts

**Research date:** 2026-03-15
**Valid until:** 2026-04-15 (stable -- SDL3 3.4.x is a point release, CELS patterns are established)
