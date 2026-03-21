---
phase: 06-draw-primitives
plan: 01
subsystem: renderer
tags: [sdl3, draw-primitives, vtable, draw-buffer, z-index, renderable]

# Dependency graph
requires:
  - phase: 05-renderer-core
    provides: "SDL3_Renderer component, RenderClearSystem/RenderPresentSystem, render pipeline phases"
provides:
  - "SDL3_Renderable vtable (filled_rect, filled_rects, outlined_rect, outlined_rects, line)"
  - "SDL3_DrawCmd tagged union type with z-index and creation order"
  - "Per-window draw buffer embedded in SDL3_Renderer component"
  - "SDL3_DrawBufferTable renderer-to-buffer lookup for cross-TU vtable access"
  - "SDL3_DrawFlushSystem (PostRender) sorts by (z, order) and flushes to SDL3"
  - "sdl3_renderable() accessor and SDL3_Renderable_use() registration"
  - "sdl3_set_blend_mode() convenience wrapper"
affects: [06-02-draw-example, 07-texture-sprites, 08-text-rendering, 09-render-example]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Renderable vtable: C struct of function pointers for backend-swappable draw dispatch"
    - "Draw command buffering: buffer during OnRender, sort by z-index, flush before present"
    - "Draw buffer embedded in SDL3_Renderer component (not a separate ECS component)"
    - "Renderer-to-buffer lookup table: static SDL3_DrawBufferTable rebuilt each frame in PreRender"

key-files:
  created:
    - "src/renderer/sdl3_draw.c"
  modified:
    - "include/cels_sdl3.h"
    - "src/sdl3_internal.h"
    - "src/sdl3_module.c"
    - "src/renderer/sdl3_renderer.c"
    - "CMakeLists.txt"

key-decisions:
  - "Draw buffer fields embedded in SDL3_Renderer component rather than separate SDL3_DrawBuffer component -- avoids extra cels_ensure_component and tightly couples buffer to renderer"
  - "Renderer-to-buffer lookup via static SDL3_DrawBufferTable rebuilt each frame -- enables cross-TU vtable functions to find the correct draw buffer from SDL_Renderer*"
  - "Simple per-command flush (no batch coalescing) -- SDL3 batches internally on GPU side; CPU-side coalescing deferred to profiling-driven optimization"
  - "cel_update required for draw buffer reset in RenderClearSystem (modifies ECS component struct fields) and flush in DrawFlushSystem (qsort modifies draw_cmds in-place)"

patterns-established:
  - "Vtable provider pattern: SDL3_Renderable_use() populates static vtable, sdl3_renderable() returns const pointer"
  - "Draw buffer lifecycle: init in renderer create, clear in PreRender, push during OnRender, flush+sort in PostRender, destroy in CLOSING"
  - "Renderer-to-buffer lookup table: populated during RenderClearSystem, consumed by vtable functions in sdl3_draw.c"

# Metrics
duration: 4min
completed: 2026-03-21
---

# Phase 6 Plan 1: Draw Primitives Core Summary

**SDL3_Renderable vtable with 5 draw functions, per-window draw buffer with z-index sorting, and DrawFlushSystem wired into the render pipeline**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-21T23:17:32Z
- **Completed:** 2026-03-21T23:21:23Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- SDL3_Renderable vtable struct with 5 function pointers (filled_rect, filled_rects, outlined_rect, outlined_rects, line) for backend-swappable draw dispatch
- SDL3_DrawCmd tagged union with z-index and creation order for sorted rendering across all systems
- Per-window draw buffer embedded in SDL3_Renderer component with dynamic array (initial capacity 256, doubles on overflow)
- DrawFlushSystem at PostRender phase sorts by (z ascending, order ascending) and issues SDL3 draw calls before present
- Full lifecycle: buffer allocated on renderer create, cleared each frame in PreRender, freed during CLOSING transition

## Task Commits

Each task was committed atomically:

1. **Task 1: Draw buffer types, vtable, and sdl3_draw.c implementation** - `b14c3d6` (feat)
2. **Task 2: Module wiring -- systems, registration, lifecycle, CMake** - `98980a4` (feat)

**Plan metadata:** (pending)

## Files Created/Modified
- `src/renderer/sdl3_draw.c` - Draw buffer lifecycle, vtable implementations, renderer-to-buffer lookup table, flush logic with qsort
- `include/cels_sdl3.h` - SDL3_DrawCmdType enum, SDL3_DrawCmd tagged union, draw buffer fields in SDL3_Renderer, SDL3_Renderable vtable, public API declarations
- `src/sdl3_internal.h` - SDL3_DrawBufferTable type, draw buffer function declarations, SDL3_DRAW_BUFFER_INITIAL_CAPACITY constant
- `src/sdl3_module.c` - DrawFlushSystem, updated RenderClearSystem with draw table/buffer management, draw buffer destroy in CLOSING, updated registration order
- `src/renderer/sdl3_renderer.c` - sdl3_draw_buffer_init call during renderer creation
- `CMakeLists.txt` - Added sdl3_draw.c to INTERFACE sources

## Decisions Made
- Embedded draw buffer fields directly in SDL3_Renderer component rather than creating a separate SDL3_DrawBuffer component. Rationale: avoids extra component registration, avoids extra cels_ensure_component call, and the draw buffer is tightly coupled to the renderer.
- Used static SDL3_DrawBufferTable rebuilt each frame for renderer-to-buffer lookup. This enables the vtable functions in sdl3_draw.c (cross-TU) to find the correct draw buffer from an SDL_Renderer pointer without using cel_query/cel_each.
- Started with simple per-command flush (no batch coalescing). SDL3 already batches internally on the GPU side for D3D/Metal/Vulkan backends. CPU-side coalescing can be added later if profiling warrants it.
- Used cel_update for draw buffer reset in RenderClearSystem and flush in DrawFlushSystem because these operations modify actual ECS component struct fields (draw_count, draw_next_order, qsort on draw_cmds), unlike SDL render calls which only modify SDL-internal state.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Renderable vtable fully functional -- developer can call draw->filled_rect(r, rect, color, z) etc. from OnRender systems
- Draw buffer, flush, and lifecycle all wired into the existing render pipeline
- Ready for Phase 6 Plan 2: draw primitives example app demonstrating all three draw types with z-ordering

---
*Phase: 06-draw-primitives*
*Completed: 2026-03-21*
