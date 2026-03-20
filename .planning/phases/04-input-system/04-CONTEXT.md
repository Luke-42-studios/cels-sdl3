# Phase 4: Input System - Context

**Gathered:** 2026-03-20
**Status:** Ready for planning

<domain>
## Phase Boundary

Event buffering into an ECS-accessible raw event queue, plus window event routing to the correct window entity's state machine. Covers INPT-01 (single drain, raw queue), INPT-02 (queue as ECS component), INPT-03 (window event routing). Keyboard/mouse state snapshots and gamepad support are v2 (INPT-04 through INPT-07).

</domain>

<decisions>
## Implementation Decisions

### Event queue contents
- Event pump drains all SDL events from a single point
- Window events (close, resize, minimize, restore, focus) are intercepted and routed directly to the window entity's state machine -- they never enter the raw queue
- All other events (keyboard, mouse, app lifecycle, etc.) are pushed into per-window queues based on which window had focus
- SDL_EVENT_QUIT is handled by the drain system (calls cel_quit), not queued

### Event consumption model
- Raw SDL_Event structs in the queue -- zero abstraction, no wrapper types
- Queue exposes .events[] array and .count -- no convenience macros or iteration helpers
- Consumer systems write their own for-loop and switch on event.type
- Per-window queues are cleared at the start of each frame, before polling new events
- Events live for exactly one frame

### Phase 3 refactor
- Phase 4 replaces Phase 3's SDL3_EventPumpSystem entirely with a new unified system
- New system handles: poll SDL, route window events, fill per-window queues -- all in one pass
- SDL_PollEvent is destructive (events consumed on read), so layering two systems is not viable
- Minimized-pause behavior preserved: SDL_WaitEvent blocks with zero CPU when all windows are minimized
- Input implementation lives in a new file (sdl3_input.c), matching the existing pattern of sdl3_init.c and sdl3_loop.c
- System declaration remains in sdl3_module.c (per-TU static ID constraint)

### Example app behavior
- Single window, console logging for event feedback
- Demonstrates keyboard events (Escape closes window) and mouse events
- Summarized logging: key presses logged individually, mouse position logged on click or periodically (not every frame)
- No renderer needed -- stays within Phase 4 scope

### Claude's Discretion
- Per-window queue component type design (dynamic array, fixed ring buffer, etc.)
- How to determine which window has focus for routing non-window events
- System registration order adjustments in CEL_Module
- Exact console log format for the example

</decisions>

<specifics>
## Specific Ideas

No specific requirements -- open to standard approaches

</specifics>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 04-input-system*
*Context gathered: 2026-03-20*
