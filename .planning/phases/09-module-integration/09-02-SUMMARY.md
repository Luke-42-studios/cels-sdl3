---
phase: 09-module-integration
plan: 02
subsystem: infra
tags: [registration, module, a-la-carte, sdl3, refactor]

# Dependency graph
requires:
  - phase: 09-module-integration plan 01
    provides: "SDL3_ErrorCallback, SDL3_Config, sdl3_set_error_callback, sdl3_set_init_flags"
  - phase: 08-text-rendering
    provides: "sdl3_fonts_close_all real implementation, text system registrations"
provides:
  - "SDL3_use(config) all-in-one registration with optional config"
  - "7 individual _use() functions for a la carte provider selection"
  - "Guard-flagged double-registration prevention"
  - "CEL_Module(SDL3_Engine, init) backward compatibility shim"
affects: [10-example-showcase (consumer API patterns)]

# Tech tracking
tech-stack:
  added: []
  patterns: ["A la carte registration via guard-flagged _use() functions within single TU"]

key-files:
  modified:
    - include/cels_sdl3.h
    - src/sdl3_module.c

key-decisions:
  - "SDL3_Textures_use() includes TextureLoadSystem, Sprite, SpriteRenderSystem (not just TextureLoadSystem)"
  - "SDL3_Text_use() includes Text, TextHandle, TextRenderSystem (complete text subsystem)"
  - "SDL3_Renderer_use() includes RenderClearSystem, DrawFlushSystem, RenderPresentSystem (render pipeline)"
  - "SDL3_Window_use() includes FrameStateSystem (monitors window states for loop exit)"
  - "SDL3_use() call order: Init -> FrameLoop -> Input -> Textures -> Window -> Text -> Renderer"
  - "Task 2 (cleanup ordering) was already implemented by Phase 8 -- no changes needed"

patterns-established:
  - "Registration partitioning: all _use() functions in same TU (sdl3_module.c) per per-TU static ID constraint"
  - "Guard flag pattern: static bool s_*_registered prevents double-registration"
  - "SDL3_use(NULL) as delegation target for CEL_Module backward compatibility"

# Metrics
duration: 4min
completed: 2026-03-22
---

# Phase 9 Plan 2: Registration API Summary

**7 individual _use() functions with guard flags partitioning monolithic CEL_Module, SDL3_use(config) as all-in-one entry point**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-22T00:07:00Z
- **Completed:** 2026-03-22T00:11:01Z
- **Tasks:** 2 (1 implemented, 1 verified-already-done)
- **Files modified:** 2

## Accomplishments
- Partitioned monolithic CEL_Module(SDL3_Engine, init) into 7 individually callable _use() functions
- SDL3_use(config) registers all providers in correct order with optional error callback and init flags
- CEL_Module(SDL3_Engine, init) now delegates to SDL3_use(NULL) for full backward compatibility
- Guard flags prevent double-registration when mixing SDL3_use() with individual _use() calls
- All 6 existing examples (minimal, frame-loop, input, renderer, textures, text) build identically

## Task Commits

Each task was committed atomically:

1. **Task 1: Registration partitioning -- individual _use() functions and SDL3_use()** - `65fd0a3` (feat)
2. **Task 2: Cleanup ordering** - no commit needed (already implemented by Phase 8)

## Files Created/Modified
- `include/cels_sdl3.h` - SDL3_use() and 7 individual _use() extern declarations
- `src/sdl3_module.c` - Guard flags, _use() implementations, SDL3_use(), thin CEL_Module body

## Decisions Made
- Partitioned TextureLoadSystem + Sprite + SpriteRenderSystem into SDL3_Textures_use() (not just load system) -- complete texture subsystem per function
- Partitioned Text + TextHandle + TextRenderSystem into SDL3_Text_use() -- complete text subsystem per function
- Partitioned RenderClearSystem + DrawFlushSystem + RenderPresentSystem into SDL3_Renderer_use() -- render pipeline as unit
- FrameStateSystem placed in SDL3_Window_use() (not SDL3_FrameLoop_use()) because it monitors window states for loop exit -- the "all windows closed" logic belongs with the window provider
- SDL3_FrameLoop_use() registers only SDL3_FrameState (the state singleton, not the monitoring system)
- SDL3_use() call order is Init -> FrameLoop -> Input -> Textures -> Window -> Text -> Renderer, which preserves system execution order within each CELS phase
- Task 2 cleanup ordering was already complete from Phase 8 (sdl3_fonts_close_all in sdl3_shutdown before TTF_Quit) -- verified and documented, no code changes needed

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Updated partitioning for Phase 6-8 declarations not in original plan**
- **Found during:** Task 1 (registration partitioning)
- **Issue:** Plan was written before Phases 6-8 executed, so it only listed 3 _use() partitions with content (Init, FrameLoop/Input/Window combined, Renderer) and had Textures_use/Text_use as empty stubs. The actual code now has TextureLoadSystem, SpriteRenderSystem, SDL3_Sprite, SDL3_Text, SDL3_TextHandle, SDL3_TextRenderSystem, SDL3_DrawFlushSystem needing partitioning.
- **Fix:** Populated SDL3_Textures_use() with TextureLoadSystem + Sprite + SpriteRenderSystem + ensure_component(Sprite). Populated SDL3_Text_use() with Text + TextHandle + TextRenderSystem + ensure_component(Text, TextHandle). Added DrawFlushSystem to SDL3_Renderer_use().
- **Files modified:** src/sdl3_module.c
- **Verification:** All 6 examples build. Registration order within each CELS phase preserved.
- **Committed in:** 65fd0a3 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking -- plan referenced stubs for code that now exists)
**Impact on plan:** Auto-fix was necessary since Phase 6-8 declarations already exist and need proper partitioning. Same pattern applied, just more registrations to distribute.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 9 (Module Integration) complete -- both error reporting and registration API delivered
- All INTG requirements met: INTG-01 (SDL3_use bundles all), INTG-02 (a la carte registration), INTG-03 (correct cleanup ordering)
- Ready for Phase 10 (Example Showcase) with clean public API: SDL3_use(config) or individual _use() functions

---
*Phase: 09-module-integration*
*Completed: 2026-03-22*
