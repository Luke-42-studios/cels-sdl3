---
phase: 02-window-provider
plan: 02
subsystem: ui
tags: [sdl3, window, ecs, lifecycle, multi-window, wayland, verification]

# Dependency graph
requires:
  - phase: 02-window-provider/01
    provides: Window types, creation/destruction, state machine, composition macro
provides:
  - Multi-window lifecycle verification example app
  - Proof that SDL3Window composition creates visible windows with READY state
  - Wayland-compatible window visibility (surface buffer commit + SDL_ShowWindow)
affects: [03-frame-loop, 05-renderer]

# Tech tracking
tech-stack:
  added: []
  patterns: [Wayland surface buffer commit for window visibility, SDL_ShowWindow explicit call]

key-files:
  created: []
  modified:
    - examples/minimal/main.c
    - src/window/sdl3_window.c

key-decisions:
  - "SDL_ShowWindow + SDL_GetWindowSurface + SDL_FillSurfaceRect + SDL_UpdateWindowSurface required for Wayland visibility (dummy driver works without, but real compositors need a committed surface buffer)"
  - "Example runs 180 frames with SDL_Delay(16) pacing for ~3s visible window display"

patterns-established:
  - "Wayland window visibility: must commit initial surface buffer before window becomes visible to compositor"
  - "Verification system pattern: cel_query + cel_each to iterate ECS components with pass/fail output"

# Metrics
duration: 5min
completed: 2026-03-15
---

# Phase 2 Plan 2: Window Lifecycle Verification Summary

**Multi-window example app with ECS query verification, plus Wayland surface buffer fix for real-compositor visibility**

## Performance

- **Duration:** ~5 min (including checkpoint)
- **Started:** 2026-03-15T23:50:00Z
- **Completed:** 2026-03-15T23:55:00Z
- **Tasks:** 2 (1 auto + 1 checkpoint)
- **Files modified:** 2

## Accomplishments
- Two-window example app exercising SDL3Window composition with verification system
- Consumer-side ECS query pattern (cel_query + cel_each on SDL3_WindowComponent) proven
- Both windows reach READY state under dummy driver and real Wayland compositor
- Wayland visibility fix ensures windows actually appear on real displays

## Task Commits

Each task was committed atomically:

1. **Task 1: Window lifecycle example app** - `746cbd7` (feat)
2. **Task 2: Checkpoint human-verify** - approved after fix

**Fix commit (deviation):** `b27d43b` (fix) - Wayland window visibility

## Files Created/Modified
- `examples/minimal/main.c` - Two-window lifecycle test with verification system, frame counter, pass/fail output
- `src/window/sdl3_window.c` - Added SDL_GetWindowSurface + SDL_FillSurfaceRect + SDL_UpdateWindowSurface + SDL_ShowWindow for Wayland compatibility

## Decisions Made
- Windows need explicit SDL_ShowWindow() and an initial surface buffer commit (SDL_GetWindowSurface + SDL_FillSurfaceRect + SDL_UpdateWindowSurface) for Wayland compositors to display them. The dummy driver works without this, but real compositors require a committed buffer before mapping the window.
- Example frame count increased from 5 to 180 with SDL_Delay(16) pacing so windows are visible long enough for manual verification (~3 seconds).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Windows invisible on Wayland compositor**
- **Found during:** Task 2 (checkpoint human-verify)
- **Issue:** Windows were created and reached READY state but were not visible on Wayland. The dummy driver worked fine, but real Wayland compositors require a committed surface buffer before they will map (display) a window.
- **Fix:** Added SDL_GetWindowSurface + SDL_FillSurfaceRect + SDL_UpdateWindowSurface to commit an initial surface buffer during window creation, plus SDL_ShowWindow() to explicitly request display. Also increased frame count to 180 with SDL_Delay pacing for adequate visibility.
- **Files modified:** src/window/sdl3_window.c, examples/minimal/main.c
- **Verification:** Running ./build/minimal on Wayland shows both windows visibly on screen
- **Committed in:** b27d43b

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Essential fix for real-world usability. Without this, windows only worked under the dummy driver. No scope creep.

## Issues Encountered
- Wayland compositor requires surface buffer commitment before window visibility -- this is a known Wayland protocol requirement but was not initially accounted for since dummy driver testing bypasses it.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 2 (Window Provider) is fully complete -- both plans delivered
- Window creation, lifecycle state machine, multi-window support, and Wayland compatibility all verified
- Phase 3 (Frame Loop) can proceed -- window entities exist and are queryable
- The example app serves as a regression test for window creation behavior
- Event-driven state transitions (minimize, resize, close) ready for Phase 4 integration via sdl3_window_handle_event

---
*Phase: 02-window-provider*
*Completed: 2026-03-15*
