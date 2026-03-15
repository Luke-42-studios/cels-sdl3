# Phase 2: Window Provider - Context

**Gathered:** 2026-03-15
**Status:** Ready for planning

<domain>
## Phase Boundary

Window creation as ECS entities with a full lifecycle state machine. Developers can spawn one or more windows, each with its own SDL_Window and independent state tracking. Frame loop, input handling, and rendering are separate phases.

</domain>

<decisions>
## Implementation Decisions

### Window defaults
- Default size: 1280x720 when developer doesn't specify
- Default title: empty string — developer is expected to set their own
- Default position: OS decides (SDL_WINDOWPOS_UNDEFINED)
- Resizable by default — modifying the window component fields drives resize, fullscreen, and other window behavior reactively

### State machine transitions
- CLOSED is terminal — to get a new window, spawn a new entity
- MINIMIZED restore goes through the full chain: MINIMIZED -> SURFACE_READY -> READY (verify surface each time)
- Rapid resize: stay in RESIZING state until stable (no resize events for a threshold), then transition back to READY
- Close goes through CLOSING first, giving systems one frame to react (save state, cleanup), then transitions to CLOSED

### Multi-window policy
- No primary window concept — all windows are equal ECS entities
- Framework does not signal or react when last window closes — developer queries for open windows and decides what to do
- Windows are completely independent — no shared state, no cross-window awareness, no window registry
- No framework limit on window count — SDL3/OS limits are the natural ceiling

### Creation API surface
- Config struct pattern: fill a WindowConfig struct (title, size, flags), pass to a creation function
- Full SDL3 window flags exposed through the config — power users get full control (borderless, fullscreen, always-on-top, etc.)
- Post-creation modification is component-driven: change a field on the Window component, a system detects changes and applies them to SDL_Window
- SDL_Window* is exposed as a field on the component for advanced users who need direct SDL3 access

### Claude's Discretion
- Exact field names and layout of the WindowConfig struct
- Internal change-detection mechanism for component-driven updates
- RESIZING stability threshold duration
- State machine implementation details (enum, bitfield, etc.)

</decisions>

<specifics>
## Specific Ideas

- Window behavior should be reactive/declarative: modifying component fields is the primary way to control window properties after creation. This aligns with the ECS philosophy — data drives behavior.
- The pattern is: config struct for creation, component mutations for runtime changes, systems react to component state.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 02-window-provider*
*Context gathered: 2026-03-15*
