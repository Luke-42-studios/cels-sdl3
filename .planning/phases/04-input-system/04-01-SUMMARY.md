---
phase: 04-input-system
plan: 01
subsystem: input
tags: [sdl3, events, ecs, event-queue, input-routing]

# Dependency graph
requires:
  - phase: 03-frame-loop
    provides: SDL3_EventPumpSystem, frame loop, minimized-pause behavior
provides:
  - SDL3_EventQueue component type (per-window raw event buffer)
  - SDL3_InputSystem replacing SDL3_EventPumpSystem
  - Single-pass event drain with window event routing and queue buffering
  - sdl3_input.c implementation (event drain, windowID routing)
  - SDL3_WindowTable cross-TU data passing pattern
affects: [04-input-system plan 02, 05-renderer-core, 10-example-application]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Window table pattern: collect mutable ECS pointers in system body, pass to cross-TU function"
    - "Eager component ID init: cels_ensure_component for components used in observers before systems run"

key-files:
  created:
    - src/input/sdl3_input.c
  modified:
    - include/cels_sdl3.h
    - src/sdl3_internal.h
    - src/sdl3_module.c
    - src/window/sdl3_window.c
    - CMakeLists.txt

key-decisions:
  - "Fixed 64-event array (8 KiB per window) over dynamic allocation -- cleared every frame, no malloc needed"
  - "Window table pattern for cross-TU iteration: system body builds stack-local table via cel_each/cel_update, passes to sdl3_input.c drain function"
  - "cels_ensure_component required for SDL3_EventQueue_id: CEL_Component _register() is a no-op, IDs only set by cel_has/cel_query which run after observer fires"
  - "Focus events (GAINED/LOST) routed to sdl3_window_handle_event as no-ops -- intercepted but no state transition"
  - "Events with windowID == 0 or stale IDs silently dropped -- no global queue needed for Phase 4"

patterns-established:
  - "Window table pattern: sdl3_module.c iterates entities via cel_each, builds SDL3_WindowTable, passes to cross-TU functions"
  - "Eager component ID initialization: call cels_ensure_component in CEL_Module init for components used in observers"

# Metrics
duration: 55min
completed: 2026-03-20
---

# Phase 4 Plan 1: Input System Core Summary

**Per-window SDL3_EventQueue component with single-pass event drain, inline window event routing, and SDL3_InputSystem replacing EventPumpSystem**

## Performance

- **Duration:** 55 min
- **Started:** 2026-03-20T19:57:47Z
- **Completed:** 2026-03-20T20:52:55Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- SDL3_EventQueue component type defined in public header (fixed 64-event array of raw SDL_Event structs)
- Complete event drain implementation in sdl3_input.c: queue clearing, minimized-pause preservation, quit routing, window event routing (including focus), per-window queue buffering by windowID, overflow protection, stale windowID dropping
- SDL3_InputSystem replaces SDL3_EventPumpSystem with dramatically simpler module-side code (system body just builds window table and calls drain function)
- All existing examples (minimal, frame-loop) build and run correctly with no regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: SDL3_EventQueue component type and sdl3_input.c implementation** - `80f21a9` (feat)
2. **Task 2: Module wiring -- replace EventPumpSystem, register component, attach queue** - `68241e1` (feat)

## Files Created/Modified
- `include/cels_sdl3.h` - Added SDL3_EventQueue component type (CEL_Component with fixed 64-event array)
- `src/sdl3_internal.h` - Added SDL3_WindowTable/SDL3_WindowEntry types and sdl3_input_drain_events declaration
- `src/input/sdl3_input.c` - NEW: Complete event drain implementation (single-pass poll, window event routing, per-window queue buffering)
- `src/sdl3_module.c` - Replaced SDL3_EventPumpSystem with SDL3_InputSystem, added SDL3_EventQueue registration and observer attachment, eager component ID initialization
- `src/window/sdl3_window.c` - Added explicit focus event cases in sdl3_window_handle_event
- `CMakeLists.txt` - Added sdl3_input.c to INTERFACE target sources

## Decisions Made

1. **Window table pattern for cross-TU iteration:** Since sdl3_input.c cannot use cel_query/cel_each (per-TU static ID constraint) and the CELS framework has no public low-level query API, the SDL3_InputSystem body in sdl3_module.c builds a stack-allocated SDL3_WindowTable via cel_each/cel_update and passes it to the drain function. This follows the existing pattern of passing component IDs to cross-TU functions, extended to pass mutable component pointers for whole-table access.

2. **Eager cels_ensure_component for SDL3_EventQueue_id:** CEL_Component's _register() function is a no-op (component IDs are lazily assigned by cel_has/cel_query). The window creation observer fires before any system body runs, so SDL3_EventQueue_id would be 0 when the observer tries to attach the queue. Fixed by calling cels_ensure_component explicitly in the module init body.

3. **SDL3_MAX_WINDOWS = 8 limit:** The window table is stack-allocated with a fixed-size array. 8 windows is far above typical usage (1-3). If exceeded, additional windows simply won't have events routed to them (safe degradation, not a crash).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] SDL3_EventQueue_id was 0 at observer call time**
- **Found during:** Task 2 (module wiring)
- **Issue:** SIGABRT crash in ecs_set_id when attaching SDL3_EventQueue to window entity. The component ID was 0 because CEL_Component _register() is a no-op -- the actual ID is only assigned when cel_has or cel_query first references the type. The observer fires during composition before any system body runs, so the ID was never initialized.
- **Fix:** Added explicit cels_ensure_component call in CEL_Module(SDL3_Engine, init) after cels_register, forcing early ID initialization.
- **Files modified:** src/sdl3_module.c
- **Verification:** Both examples (minimal, frame-loop) now run without crashes.
- **Committed in:** 68241e1 (Task 2 commit)

**2. [Rule 3 - Blocking] CMake reconfiguration required local SDL3 source**
- **Found during:** Task 2 (build verification)
- **Issue:** Adding sdl3_input.c to CMakeLists.txt triggered a CMake GLOB mismatch and reconfiguration, which attempted to git-clone SDL3 from GitHub (no network access). Build was blocked.
- **Fix:** Reconfigured with -DFETCHCONTENT_SOURCE_DIR_SDL3 pointing to the local /home/cachy/workspaces/engines/SDL checkout and -DFETCHCONTENT_FULLY_DISCONNECTED=ON.
- **Files modified:** None (build system configuration only)
- **Verification:** cmake --build succeeds, both targets link and run.
- **Committed in:** N/A (build configuration, not code)

---

**Total deviations:** 2 auto-fixed (1 bug, 1 blocking)
**Impact on plan:** Both fixes were essential for correctness and build functionality. No scope creep.

## Issues Encountered
- Build environment lacks network access, requiring local SDL3 source for CMake reconfiguration. Resolved by pointing FETCHCONTENT_SOURCE_DIR_SDL3 to the sibling engines/SDL directory.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- SDL3_EventQueue component is attached to every window entity and cleared each frame
- Consumer systems can query SDL3_EventQueue and iterate raw SDL_Event structs
- Phase 4 Plan 02 (input example app) can demonstrate keyboard/mouse event consumption from the queue
- No blockers for Plan 02

---
*Phase: 04-input-system*
*Completed: 2026-03-20*
