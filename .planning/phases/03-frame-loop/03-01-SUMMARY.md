---
phase: 03-frame-loop
plan: 01
subsystem: loop
tags: [sdl3, frame-loop, delta-time, fps, event-pump, performance-counter]

# Dependency graph
requires:
  - phase: 02-window-provider
    provides: "SDL3_WindowComponent with state machine, sdl3_window_handle_event"
provides:
  - "sdl3_should_run() / sdl3_delta() consumer loop API"
  - "SDL3_EventPumpSystem draining SDL events and routing window events"
  - "SDL3_FrameStateSystem tracking all-windows-closed exit condition"
  - "SDL3_FrameState singleton with running, delta_time, fps, smoothed_fps"
  - "Delta time via SDL_GetPerformanceCounter/Frequency (sub-microsecond)"
  - "FPS tracking with exponential moving average smoothing"
  - "Frame rate capping via sdl3_cap_frame_rate()"
  - "Minimized-pause blocking on SDL_WaitEvent for zero CPU"
affects: [04-input-system, 05-renderer, 03-02]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Event pump system pattern: CEL_System with cel_run for global event draining"
    - "Frame state singleton pattern: CEL_State owned by helper TU, systems reference via sdl3_frame_set_running()"
    - "Consumer loop pattern: while (sdl3_should_run()) { cels_step(sdl3_delta()); }"

key-files:
  created:
    - "src/loop/sdl3_loop.c"
  modified:
    - "include/cels_sdl3.h"
    - "src/sdl3_internal.h"
    - "src/sdl3_module.c"
    - "CMakeLists.txt"

key-decisions:
  - "SDL3_FrameState data owned by sdl3_loop.c (same pattern as SDL3_ContextState in sdl3_init.c)"
  - "Event pump routes window events inline via cel_query/cel_each/cel_update (no separate routing layer)"
  - "Minimized pause uses SDL_WaitEvent in event pump before normal SDL_PollEvent loop"
  - "Registration order EventPump -> WindowState -> FrameState ensures correct execution within OnLoad phase"
  - "sdl3_delta() both computes delta and updates SDL3_FrameState fields (single call does both)"
  - "sdl3_frame_set_running() helper exposes frame state mutation to systems in sdl3_module.c"

patterns-established:
  - "Event pump system: global cel_run system that drains SDL events before entity systems"
  - "Minimized pause: count minimized windows via cel_each, block on SDL_WaitEvent when all minimized"
  - "Frame state mutation: helper function in owner TU, called by system in sdl3_module.c"

# Metrics
duration: 4min
completed: 2026-03-16
---

# Phase 3 Plan 1: Frame Loop Core Summary

**Delta time via SDL performance counters, event pump with window routing and minimized-pause, FPS tracking with EMA smoothing, consumer loop API (sdl3_should_run/sdl3_delta)**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-16T00:48:47Z
- **Completed:** 2026-03-16T00:52:19Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- Created frame loop infrastructure with sub-microsecond delta time via SDL_GetPerformanceCounter
- Built event pump system that drains SDL events, routes window events, blocks on SDL_WaitEvent when minimized
- Integrated running state management: all-windows-CLOSED detection and SDL_EVENT_QUIT handling
- Exposed consumer-facing API: sdl3_should_run() and sdl3_delta() for the main loop pattern

## Task Commits

Each task was committed atomically:

1. **Task 1: Loop helpers, types, and CMake update** - `a7715a4` (feat)
2. **Task 2: Event pump system, frame state system, module registration** - `42f354e` (feat)

## Files Created/Modified

- `src/loop/sdl3_loop.c` - Delta time, FPS tracking, frame rate capping, consumer API (sdl3_should_run, sdl3_delta)
- `include/cels_sdl3.h` - SDL3_FrameState (CEL_Define_State), SDL3_FrameConfig type, public API declarations
- `src/sdl3_internal.h` - Loop function declarations (sdl3_loop_init, sdl3_compute_delta, etc.)
- `src/sdl3_module.c` - SDL3_EventPumpSystem, SDL3_FrameStateSystem, updated module registration
- `CMakeLists.txt` - Added src/loop/sdl3_loop.c to target_sources

## Decisions Made

- **SDL3_FrameState ownership in sdl3_loop.c:** Follows established pattern from sdl3_init.c -- CEL_State declaration in sdl3_module.c, static data and register/bind in the helper TU. This keeps all CELS macro declarations in the single required TU while allowing data ownership elsewhere.
- **Inline event routing in pump system:** Window events are matched by windowID via cel_query + cel_each + cel_update inside the event pump system body, rather than a separate routing function. This keeps the event handling self-contained for Phase 3; Phase 4 will add the full event buffer.
- **sdl3_frame_set_running() helper:** Rather than exposing SDL3_FrameState directly to sdl3_module.c systems, a helper function in sdl3_loop.c mutates the running flag. This maintains the per-TU data ownership pattern.
- **Registration order for system execution:** EventPump registered before WindowState, FrameState registered after WindowState, ensuring correct pump -> state-transition -> exit-check ordering within OnLoad phase.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added CEL_State(SDL3_FrameState) in Task 1 for compilation**
- **Found during:** Task 1 (verification step)
- **Issue:** sdl3_loop.c references SDL3_FrameState_register() and SDL3_FrameState_id symbols defined by CEL_State(SDL3_FrameState), which was planned for Task 2. Without it, Task 1 produced linker errors.
- **Fix:** Added `CEL_State(SDL3_FrameState);` to sdl3_module.c in Task 1 (alongside the existing `CEL_State(SDL3_ContextState);`)
- **Files modified:** src/sdl3_module.c
- **Verification:** Build succeeds after addition
- **Committed in:** a7715a4 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Minimal -- pulled a single line forward from Task 2 into Task 1 for compilation. No scope creep.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Frame loop infrastructure complete, ready for Plan 03-02 (frame loop verification example)
- Consumer can now write `while (sdl3_should_run()) { cels_step(sdl3_delta()); }` with real timing
- Event pump handles SDL_EVENT_QUIT and window events; Phase 4 (Input System) will extend with full event buffer
- No blockers for next plan

---
*Phase: 03-frame-loop*
*Completed: 2026-03-16*
