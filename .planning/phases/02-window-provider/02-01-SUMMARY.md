---
phase: 02-window-provider
plan: 01
subsystem: ui
tags: [sdl3, window, ecs, lifecycle, state-machine, cels]

# Dependency graph
requires:
  - phase: 01-sdl3-bootstrap
    provides: SDL3 initialization, CELS module pattern, build system
provides:
  - SDL3_WindowState enum (8-state lifecycle)
  - SDL3_WindowConfig and SDL3_WindowComponent ECS components
  - SDL3Window composition macro with defaults (1280x720, resizable)
  - Window creation/destruction via SDL3 API
  - CLOSING->CLOSED one-frame-delay state system
  - sdl3_window_handle_event for event-driven state transitions
affects: [03-frame-loop, 04-input-system, 05-renderer, 06-primitives]

# Tech tracking
tech-stack:
  added: []
  patterns: [per-TU component_id passing, lifecycle observer delegation, synchronous state chain]

key-files:
  created:
    - src/window/sdl3_window.c
  modified:
    - include/cels_sdl3.h
    - src/sdl3_internal.h
    - src/sdl3_module.c
    - CMakeLists.txt

key-decisions:
  - "State advances synchronously NONE->CREATED->SURFACE_READY->READY during creation (SURFACE_READY is instant pass-through for SDL_Renderer path)"
  - "component_id passed as explicit parameter from sdl3_module.c to sdl3_window.c (per-TU static ID constraint)"
  - "SDL_WINDOW_RESIZABLE ORed in both at composition level and creation level (double safety net, borderless skips resizable)"
  - "CLOSING->CLOSED driven by per-frame state system with SDL_DestroyWindow (one frame delay for cleanup)"

patterns-established:
  - "Window function delegation: sdl3_module.c (CELS macros) -> sdl3_window.c (SDL3 calls) via explicit ID parameters"
  - "handle_event pattern: Phase 4 will call sdl3_window_handle_event to drive state transitions from SDL events"

# Metrics
duration: 3min
completed: 2026-03-15
---

# Phase 2 Plan 1: Window Provider Core Summary

**SDL3 window entities with 8-state lifecycle, ECS composition (1280x720 resizable default), and per-frame CLOSING->CLOSED state system**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-15T23:10:38Z
- **Completed:** 2026-03-15T23:13:44Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- SDL3_WindowState enum with full 8-state lifecycle (NONE through CLOSED) in public header
- Window creation via 4-parameter SDL3 API with sensible defaults and RESIZABLE flag
- CELS module wiring: lifecycle observers, state system, composition, registration
- Event-driven state machine handler ready for Phase 4 integration

## Task Commits

Each task was committed atomically:

1. **Task 1: Public types and window implementation** - `ead2b17` (feat)
2. **Task 2: Module wiring, composition, system, and CMake update** - `631a8d9` (feat)

## Files Created/Modified
- `include/cels_sdl3.h` - Added SDL3_WindowState enum, SDL3_WindowConfig/SDL3_WindowComponent components, SDL3Window composition macro
- `src/window/sdl3_window.c` - Window creation (defaults + RESIZABLE), destruction, event-driven state machine
- `src/sdl3_internal.h` - Window function declarations with component_id parameter
- `src/sdl3_module.c` - Window lifecycle observers, state system, composition, module registration
- `CMakeLists.txt` - Added sdl3_window.c as INTERFACE source

## Decisions Made
- State advances synchronously to READY during creation -- SURFACE_READY is an instant pass-through for SDL_Renderer path, becomes meaningful for future SDL_GPU path
- component_id passed explicitly from sdl3_module.c to sdl3_window.c functions, following the per-TU static ID constraint established in Phase 1
- SDL_WINDOW_RESIZABLE applied at both composition level (always ORed in) and creation level (unless BORDERLESS) -- double safety net
- CLOSING->CLOSED transition handled by CEL_System at OnLoad phase, destroying SDL_Window and nulling pointer after one frame delay

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Window entities can be created with SDL3Window composition macro
- sdl3_window_handle_event is ready for Phase 4 (Input System) to call when routing SDL events
- SDL3_WindowComponent queryable via standard CELS ECS queries
- Multiple windows supported with independent state machines
- Phase 3 (Frame Loop) can proceed -- window entity creation is the prerequisite

---
*Phase: 02-window-provider*
*Completed: 2026-03-15*
