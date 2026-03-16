# Phase 3: Frame Loop - Context

**Gathered:** 2026-03-15
**Status:** Ready for planning

<domain>
## Phase Boundary

CELS-integrated frame loop that pumps SDL events, calculates delta time, and ticks the ECS each frame. This phase delivers the core loop infrastructure — input processing and rendering are separate phases.

</domain>

<decisions>
## Implementation Decisions

### Loop ownership
- CELS drives the loop — cels_step() is the heartbeat, SDL event pump and delta time calculation happen inside CELS systems
- Consumer owns the while loop (`while (running) { cels_step(session); }`) — no convenience `sdl3_run()` function
- Event pump system registered with explicit CELS phase/priority so consumer controls ordering relative to other systems
- Loop running state is ECS-driven — a component/state in the ECS that systems can read/write, consumer queries it for their while condition

### Timing model
- Variable delta time — each frame measures actual elapsed time via SDL_GetPerformanceCounter/Frequency
- Configurable frame rate — support both VSync and target FPS cap, consumer chooses
- Delta time exposed via session parameter (not an ECS component) — available through session context during tick
- Track and expose both delta time and smoothed FPS counter for debug overlays

### Exit conditions
- Default exit trigger: all windows reach CLOSED state
- Consumer can also force-quit programmatically by setting the running state to false (e.g., fatal error, explicit exit)
- On loop exit, auto-cleanup SDL state — remaining windows transition to CLOSED and SDL resources are cleaned up
- SDL_QUIT events trigger immediate exit — set running state to false, auto-cleanup on next iteration

### Background behavior
- Pause when all windows are minimized — loop blocks on SDL_WaitEvent, zero CPU usage
- Resume normal loop immediately when any window is restored
- Unfocused (but visible) windows do NOT trigger pause — only minimized
- Events still processed during pause wake (SDL_WaitEvent returns the waking event)

### Claude's Discretion
- Internal delta time calculation implementation
- FPS smoothing algorithm
- System registration priority values
- Auto-cleanup ordering details

</decisions>

<specifics>
## Specific Ideas

- Consumer loop pattern should look like: `while (sdl3_should_run(session)) { cels_step(session); }` or equivalent ECS state check
- SDL_WaitEvent for minimized pause — clean blocking, instant wake

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 03-frame-loop*
*Context gathered: 2026-03-15*
