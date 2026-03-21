---
phase: 07-textures
plan: 01
subsystem: rendering
tags: [sdl3-image, texture-cache, sprite, reference-counting, IMG_LoadTexture]

# Dependency graph
requires:
  - phase: 06-draw-primitives
    provides: "Renderer component with draw buffer, RenderClear/Present systems, sdl3_internal.h patterns"
  - phase: 05-renderer-core
    provides: "SDL3_Renderer component, renderer create/destroy lifecycle"
provides:
  - "SDL3_Sprite component with texture_path, texture_handle, state, rendering params"
  - "SDL3_TextureState enum (NONE, LOADING, READY, FAILED, UNLOADED)"
  - "Per-renderer texture cache with 128-entry capacity and reference counting"
  - "TextureLoadSystem at OnLoad (auto-detects and loads textures)"
  - "SpriteRenderSystem at OnRender (renders READY sprites with rotation/flip/alpha)"
  - "Renderer destruction cascade (cache invalidated before renderer destroy)"
  - "sdl3_set_asset_base_dir for configurable asset directory"
affects: [08-text-rendering, 07-02-textures-example]

# Tech tracking
tech-stack:
  added: [SDL3_image (IMG_LoadTexture)]
  patterns: [per-renderer-cache, opaque-handle, declarative-asset-loading, state-machine-loading]

key-files:
  created:
    - "src/texture/sdl3_texture.c"
  modified:
    - "include/cels_sdl3.h"
    - "src/sdl3_internal.h"
    - "src/sdl3_module.c"
    - "CMakeLists.txt"

key-decisions:
  - "Sprites live on window entities (same entity as renderer) for simplest query pattern"
  - "Opaque uint32_t handle (1-based index) instead of raw SDL_Texture* pointer -- survives cache invalidation"
  - "Cache invalidation zeroes entries without SDL_DestroyTexture -- SDL_DestroyRenderer handles that"
  - "Per-sprite alpha modulation with reset to prevent state leakage between sprites sharing same texture"

patterns-established:
  - "Declarative asset loading: set path on component, system detects NONE state and loads automatically"
  - "Per-renderer cache with renderer-to-cache lookup table (max 8 renderers)"
  - "Reference counting for shared GPU resources (texture cache entries)"
  - "State machine loading (NONE -> LOADING -> READY/FAILED) for future async support"

# Metrics
duration: 4min
completed: 2026-03-21
---

# Phase 7 Plan 1: Texture Core Summary

**Per-renderer texture cache with reference counting, SDL3_Sprite component, declarative load system via IMG_LoadTexture, and sprite rendering with rotation/flip/alpha**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-21T23:31:29Z
- **Completed:** 2026-03-21T23:35:56Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- SDL3_Sprite component with full rendering parameters (texture_path, src/dst rects, rotation, flip, alpha)
- Per-renderer texture cache (128 entries, reference counted, path-keyed lookup) with renderer-to-cache table (max 8)
- TextureLoadSystem auto-detects sprites with texture_path + NONE state and loads via IMG_LoadTexture
- SpriteRenderSystem renders READY sprites via SDL_RenderTextureRotated with per-sprite alpha modulation
- Renderer destruction cascade: texture cache invalidated before SDL_DestroyRenderer (no double-free)

## Task Commits

Each task was committed atomically:

1. **Task 1: Sprite types, texture cache implementation, and internal declarations** - `abdf787` (feat)
2. **Task 2: Module wiring -- systems, registration, lifecycle, CMake** - `4bd8e99` (feat)

## Files Created/Modified
- `include/cels_sdl3.h` - SDL3_TextureState enum, SDL3_Sprite component, SDL3_image include, sdl3_set_asset_base_dir declaration
- `src/sdl3_internal.h` - Texture cache types (SDL3_TextureCacheEntry, SDL3_TextureCache), function declarations
- `src/texture/sdl3_texture.c` - Per-renderer texture cache with reference counting, renderer-to-cache lookup table, asset base dir
- `src/sdl3_module.c` - TextureLoadSystem, SpriteRenderSystem, cels_ensure_component for SDL3_Sprite, renderer destruction cascade
- `CMakeLists.txt` - Added sdl3_texture.c to INTERFACE sources

## Decisions Made
- Sprites live on window entities (same entity as renderer) -- simplest query pattern, follows RESEARCH recommendation
- Opaque uint32_t handle (1-based index) for texture references -- survives cache invalidation gracefully (stale handle returns NULL)
- Cache invalidation zeroes entries without calling SDL_DestroyTexture -- SDL_DestroyRenderer frees all associated textures automatically
- Per-sprite alpha modulation with reset after render to prevent state leakage between sprites sharing the same texture
- TextureLoadSystem registered after InputSystem (OnLoad phase) so sprite creation from input responses happens before loading
- SpriteRenderSystem at OnRender phase, between RenderClear (PreRender) and DrawFlush/RenderPresent (PostRender)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed include path for sdl3_internal.h in texture subdirectory**
- **Found during:** Task 2 (CMake build)
- **Issue:** `src/texture/sdl3_texture.c` used `#include "sdl3_internal.h"` but the header is in `src/`, not `src/texture/`
- **Fix:** Changed to `#include "../sdl3_internal.h"` (relative path from subdirectory)
- **Files modified:** `src/texture/sdl3_texture.c`
- **Verification:** All 5 examples build and link successfully
- **Committed in:** `4bd8e99` (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Minor path correction for subdirectory structure. No scope creep.

## Issues Encountered
- Pre-existing cels framework test build failures (test_sparse_set, test_entity_model, test_lifecycle_fsm) due to missing internal headers -- does not affect cels-sdl3 targets

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Texture core infrastructure complete, ready for Phase 7 Plan 2 (textures example)
- Example will demonstrate SDL3_Sprite usage with actual image files
- Asset base directory pattern ready for use in examples

---
*Phase: 07-textures*
*Completed: 2026-03-21*
