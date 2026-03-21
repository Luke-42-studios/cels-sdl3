---
phase: 05-renderer-core
plan: 01
subsystem: renderer
tags: [sdl3, sdl-renderer, ecs-component, clear-present-cycle, pipeline-phases]

# Dependency graph
requires:
  - phase: 04-input-system
    provides: "Window entities with SDL3_WindowComponent, SDL3_EventQueue, InputSystem, WindowStateSystem"
  - phase: 03-frame-loop
    provides: "Frame loop with sdl3_should_run/sdl3_delta, FrameStateSystem"
  - phase: 02-window-provider
    provides: "Window lifecycle (create/destroy), window state machine"
provides:
  - "SDL3_Renderer component type (renderer pointer + per-window clear_color)"
  - "Automatic renderer lifecycle (create with window, destroy before window)"
  - "RenderClearSystem (PreRender phase) and RenderPresentSystem (PostRender phase)"
  - "Render pipeline brackets for developer draw systems at OnRender phase"
affects: [06-draw-primitives, 07-texture-sprites, 08-text-rendering, 09-render-example]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Render cycle via CELS built-in pipeline phases (PreRender/PostRender)"
    - "Paired ECS component (SDL3_Renderer alongside SDL3_WindowComponent on same entity)"
    - "SDL_DestroyWindowSurface before SDL_CreateRenderer for surface/renderer mutual exclusion"

key-files:
  created:
    - "src/renderer/sdl3_renderer.c"
  modified:
    - "include/cels_sdl3.h"
    - "src/sdl3_internal.h"
    - "src/sdl3_module.c"
    - "CMakeLists.txt"

key-decisions:
  - "Used cels_entity_get_component to read back window pointer in on_create observer rather than modifying sdl3_window_create return type"
  - "No cel_update wrapping for SDL render calls -- component struct fields are not modified, only SDL-internal backbuffer state"
  - "Skip MINIMIZED, CLOSING, and CLOSED windows in render systems (not just != READY, to allow RESIZING windows to render)"

patterns-established:
  - "Paired component pattern: SDL3_Renderer attached to same entity as SDL3_WindowComponent"
  - "Pipeline phase render cycle: PreRender (clear) -> OnRender (developer draws) -> PostRender (present)"
  - "cels_entity_get_component for reading back components set by cross-TU functions in observers"

# Metrics
duration: 3min
completed: 2026-03-21
---

# Phase 5 Plan 1: Renderer Core Summary

**SDL3_Renderer paired component with automatic lifecycle, PreRender clear and PostRender present cycle using CELS pipeline phases**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-21T20:00:45Z
- **Completed:** 2026-03-21T20:03:35Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- SDL3_Renderer component type with renderer pointer and per-window clear_color (default cornflower blue)
- Automatic renderer creation in window on_create observer with SDL_DestroyWindowSurface for surface/renderer mutual exclusion
- Renderer destroyed before window in CLOSING->CLOSED transition (SDL requirement)
- RenderClearSystem (PreRender) and RenderPresentSystem (PostRender) bracket the render frame for developer draw systems
- All three existing examples (minimal, frame-loop, input) build and run without regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: SDL3_Renderer component type and sdl3_renderer.c implementation** - `ccebcc1` (feat)
2. **Task 2: Module wiring -- render systems, lifecycle integration, CMake update** - `c8e3f7b` (feat)

**Plan metadata:** (pending)

## Files Created/Modified
- `src/renderer/sdl3_renderer.c` - Renderer create/destroy implementation with SDL_DestroyWindowSurface cleanup
- `include/cels_sdl3.h` - SDL3_Renderer component type definition (renderer pointer + clear_color)
- `src/sdl3_internal.h` - Renderer function declarations (sdl3_renderer_create, sdl3_renderer_destroy)
- `src/sdl3_module.c` - RenderClear/RenderPresent systems, lifecycle wiring, module registration
- `CMakeLists.txt` - Added sdl3_renderer.c to INTERFACE sources

## Decisions Made
- Used `cels_entity_get_component` to read back the SDL3_WindowComponent after `sdl3_window_create` sets it, rather than modifying `sdl3_window_create` to return `SDL_Window*`. This keeps the existing function signature stable.
- No `cel_update` wrapping for SDL_SetRenderDrawColor/SDL_RenderClear/SDL_RenderPresent calls. The ECS component struct fields are not modified; only SDL-internal backbuffer state changes. Using cel_update would trigger unnecessary change notifications.
- Render systems skip MINIMIZED, CLOSING, and CLOSED windows (allows RESIZING windows to continue rendering visibly).

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Render pipeline brackets established: developer draw systems slot in at OnRender phase between clear and present
- SDL3_Renderer component accessible via cel_query/cel_each for future draw primitives, textures, and text phases
- Clear color runtime-changeable for Phase 5 Plan 2 example app (interactive color changes)

---
*Phase: 05-renderer-core*
*Completed: 2026-03-21*
