---
phase: 03-frame-loop
plan: 02
subsystem: loop
tags: [sdl3, frame-loop, example, fps-reporting, context-binding]

# Dependency graph
requires:
  - phase: 03-frame-loop
    plan: 01
    provides: "sdl3_should_run(), sdl3_delta(), SDL3_FrameState, event pump system"
provides:
  - "Frame loop example demonstrating canonical consumer pattern"
  - "FPS reporting system reading SDL3_FrameState every second"
  - "Window stays open until user closes it (no auto-exit)"
  - ".context = true flag binding SDL3 context lifecycle to window"
affects: [04-input-system]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Context binding: SDL3Window(.context = true) auto-inits SDL3 and shuts down on window close"
    - "FPS reporter: cel_delta() accumulator with cel_read(SDL3_FrameState) for smoothed values"

key-files:
  created:
    - "examples/frame-loop/main.c"
  modified:
    - "CMakeLists.txt"
    - "include/cels_sdl3.h"
    - "src/sdl3_module.c"
    - "src/sdl3_init.c"
    - "src/sdl3_internal.h"
    - "src/window/sdl3_window.c"

key-decisions:
  - "Window stays open until user closes it -- no auto-exit frame counter"
  - ".context = true on SDL3Window binds SDL3 context lifecycle to that window"
  - "sdl3_ensure_init() only called for context-bound windows, not all windows"
  - "WindowStateSystem handles context shutdown when context-bound window reaches CLOSED"
  - "FPS reporting uses time-based accumulator (every ~1 second)"

patterns-established:
  - "Context binding: SDL3Window(.context = true) replaces separate SDL3Context composition"
  - "Consumer loop: while (sdl3_should_run()) { cels_step(sdl3_delta()); }"

# Metrics
duration: 8min
completed: 2026-03-16
---

# Phase 3 Plan 2: Frame Loop Example Summary

**Frame loop example app with FPS reporting, context-bound window lifecycle, canonical consumer loop pattern**

## Performance

- **Duration:** 8 min
- **Tasks:** 2 (1 auto + 1 checkpoint)
- **Files modified:** 7

## Accomplishments

- Created frame loop example with real measured delta time via SDL_GetPerformanceCounter
- FPS reporter system prints smoothed FPS every ~1 second from SDL3_FrameState
- Window stays open until user closes it (no artificial frame limit)
- Added .context = true flag to SDL3Window for binding SDL3 context lifecycle
- Context auto-inits on window create, auto-shuts-down on window close

## Task Commits

1. **Task 1: Frame loop example app and CMake target** - `4008095` (feat)
2. **Checkpoint: Human verification** - approved, changes committed as `2758273` (feat)

## Files Created/Modified

- `examples/frame-loop/main.c` - Consumer loop pattern with FPS reporting
- `CMakeLists.txt` - frame-loop build target
- `include/cels_sdl3.h` - Added context flag to SDL3_WindowConfig/Component/Composition
- `src/sdl3_module.c` - Context-aware window lifecycle observers, WindowStateSystem shutdown
- `src/sdl3_init.c` - sdl3_ensure_init() for conditional auto-initialization
- `src/sdl3_internal.h` - sdl3_ensure_init() declaration
- `src/window/sdl3_window.c` - Propagate context_bound flag to window component

## Decisions Made

- **No auto-exit:** Example stays open until window close (user feedback)
- **Context binding via .context flag:** User requested explicit binding of SDL3 context lifecycle to a chosen window, using observer pattern. WindowStateSystem observes CLOSING→CLOSED transition and calls sdl3_shutdown() for context-bound windows.
- **sdl3_ensure_init():** Only called when .context = true, not unconditionally on all windows

## Deviations from Plan

### User-Requested Changes

**1. Removed FrameCounter auto-exit system**
- Plan specified 300-frame auto-exit for testing
- User wanted window to stay open until manual close
- Removed FrameCounter system entirely

**2. Added .context = true window binding**
- Plan used separate SDL3Context composition
- User requested context lifecycle bound to window via observer pattern
- Added context flag to SDL3_WindowConfig, SDL3_WindowComponent, SDL3Window composition
- WindowStateSystem triggers sdl3_shutdown() when context-bound window closes

---

*Phase: 03-frame-loop*
*Completed: 2026-03-16*
