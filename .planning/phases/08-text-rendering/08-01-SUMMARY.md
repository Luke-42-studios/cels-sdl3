---
phase: 08-text-rendering
plan: 01
subsystem: rendering
tags: [sdl3-ttf, text-rendering, ttf-text, font-loading, ecs-caching]

# Dependency graph
requires:
  - phase: 07-textures
    provides: renderer component with draw buffer, texture cache, sprite system
  - phase: 05-renderer-core
    provides: SDL3_Renderer component, render clear/present systems
provides:
  - SDL3_Text and SDL3_TextHandle ECS components
  - Global font array with sdl3_font_load/close API
  - SDL3_TextRenderSystem (OnRender phase) with cached TTF_Text objects
  - Text engine creation per-window alongside renderer
  - Correct destruction order (TTF_Text -> TTF_TextEngine -> SDL_Renderer)
affects: [08-text-rendering plan 02, 09-clay-integration]

# Tech tracking
tech-stack:
  added: []
  patterns: [per-window text engine, cached TTF_Text with change detection, block-scoped cel_query for multi-query systems]

key-files:
  created: [src/text/sdl3_text.c]
  modified: [include/cels_sdl3.h, src/sdl3_internal.h, src/sdl3_module.c, src/renderer/sdl3_renderer.c, src/sdl3_init.c, CMakeLists.txt]

key-decisions:
  - "TTF_TextEngine stored on SDL3_Renderer component (couples engine to renderer lifetime)"
  - "Text entities separate from window entities -- system finds first active renderer then iterates text"
  - "Block scope required for multiple cel_query calls in same system body"
  - "Font close_all called in sdl3_shutdown before TTF_Quit for cleanup safety"

patterns-established:
  - "Block-scoped cel_query: wrap in {} when a system needs multiple cel_query calls to avoid _cel_next_field_index redefinition"
  - "Two-pass cleanup: first pass destroys dependent resources (TTF_Text), second pass destroys owners (text engine, renderer, window)"

# Metrics
duration: 6min
completed: 2026-03-21
---

# Phase 8 Plan 1: Text Rendering Core Summary

**SDL3_ttf text engine with cached TTF_Text objects, global font array, and ECS-driven change detection**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-21T23:46:06Z
- **Completed:** 2026-03-21T23:51:44Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- Global font array (32 slots) with sdl3_font_load/close/close_all API matching Clay's fontId pattern
- SDL3_TextRenderSystem renders text entities at OnRender phase using cached TTF_Text objects
- Change detection via last_string/last_color/last_wrap/last_font_id fields -- no per-frame recreation
- Correct destruction order: TTF_Text -> TTF_TextEngine -> SDL_Renderer -> SDL_Window

## Task Commits

Each task was committed atomically:

1. **Task 1: Font API and text types** - `07199ec` (feat)
2. **Task 2: Text engine creation, render system, cleanup, and module wiring** - `4376430` (feat)

## Files Created/Modified
- `src/text/sdl3_text.c` - Global font array, font load/get/close API, TTF_Text create/sync/destroy helpers
- `include/cels_sdl3.h` - SDL3_Text component, SDL3_TextHandle component, SDL3_TextAlign enum, TTF_TextEngine field on SDL3_Renderer, font API declarations
- `src/sdl3_internal.h` - Internal text function declarations (sdl3_font_get, sdl3_text_create/sync/destroy)
- `src/sdl3_module.c` - SDL3_TextRenderSystem, text cleanup in WindowStateSystem, component registration
- `src/renderer/sdl3_renderer.c` - TTF_TextEngine creation alongside SDL_Renderer
- `src/sdl3_init.c` - sdl3_fonts_close_all() call in sdl3_shutdown() before TTF_Quit()
- `CMakeLists.txt` - Added src/text/sdl3_text.c to target_sources

## Decisions Made
- TTF_TextEngine* stored as field on SDL3_Renderer component (not a separate component) -- couples text engine lifetime to renderer lifetime correctly
- Text entities are separate from window entities -- the text render system finds the first READY window's renderer and text engine, then iterates all text entities. Single-window text assignment is the v1 approach.
- Block scope `{}` required around cel_query calls when a system body needs multiple queries (CELS macro declares local `_cel_next_field_index` variable)
- Two-pass cleanup in WindowStateSystem: first pass destroys all TTF_Text handles, second pass destroys text engine then renderer -- ensures "all text destroyed before engine" requirement from SDL3_ttf docs
- sdl3_fonts_close_all() called in sdl3_shutdown() before TTF_Quit() (not in WindowStateSystem) for safe font cleanup regardless of shutdown path

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Block-scoped cel_query calls**
- **Found during:** Task 2 (implementing TextRenderSystem and WindowStateSystem)
- **Issue:** Multiple cel_query calls in the same system function body cause "redefinition of '_cel_next_field_index'" compiler error because the macro declares a local variable
- **Fix:** Wrapped the first cel_query/cel_each in each multi-query system in a `{}` block scope
- **Files modified:** src/sdl3_module.c
- **Verification:** Build succeeds with no errors
- **Committed in:** 4376430 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Auto-fix was necessary for compilation. No scope creep.

## Issues Encountered
- SDL_VIDEODRIVER=dummy crashes with all examples (including pre-existing ones like minimal) -- this is a pre-existing issue with the dummy video driver not supporting renderer creation, not a regression from this plan

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Text rendering core is complete and ready for the example app (Plan 02)
- All existing examples still build with no new errors or warnings
- Font loading, text rendering, caching, and cleanup all wired into the module

---
*Phase: 08-text-rendering*
*Completed: 2026-03-21*
