---
phase: 10-example-application
plan: 01
subsystem: examples
tags: [demo, landscape, draw-primitives, textures, text, input, sdl3, showcase]

# Dependency graph
requires:
  - phase: 06-draw-primitives
    provides: sdl3_renderable() vtable for filled_rect, outlined_rect, line
  - phase: 07-textures
    provides: SDL3_Sprite component with declarative texture_path loading
  - phase: 08-text-rendering
    provides: SDL3_Text component, sdl3_font_load, TTF_Text caching
  - phase: 09-module-integration
    provides: cels_register(SDL3_Engine) all-in-one registration
provides:
  - Full demo application exercising all v1 features of cels-sdl3
  - Bundled assets (PressStart2P font, pixel-art tree sprite, license file)
  - CMake demo target for build validation
  - Living documentation for cels-sdl3 consumer API patterns
affects: []

# Tech tracking
tech-stack:
  added: [PressStart2P-Regular.ttf (OFL 1.1)]
  patterns:
    - "Double-buffered string for text cache invalidation (pointer swap triggers sync)"
    - "cels_entity_set_component for attaching sprite to window entity outside SDL3Window block"
    - "Y-position matching to identify text entities in systems (same as text example)"

key-files:
  created:
    - examples/demo/main.c
    - examples/assets/PressStart2P-Regular.ttf
    - examples/assets/tree.png
    - examples/assets/LICENSE-assets.txt
  modified:
    - CMakeLists.txt

key-decisions:
  - "Sprite attached to window entity via cels_entity_set_component (TextureLoadSystem requires Sprite+Renderer+WindowComponent on same entity)"
  - "FPS string uses double-buffered static arrays for pointer-change detection in text cache invalidation"
  - "Font loaded in OnLoad system with static guard (not in CEL_Compose) to avoid per-TU timing issues"
  - "Tree sprite created via ImageMagick convert (32x48 pixel art, CC0)"

patterns-established:
  - "Full-app composition: window + sprite + text entities in one CEL_Compose"
  - "Separate systems for input (OnUpdate), rendering (OnRender), and text updates (OnUpdate)"

# Metrics
duration: 4min
completed: 2026-03-22
---

# Phase 10 Plan 01: Demo Application Summary

**Full-feature landscape demo composing window, input, draw primitives, textures, and text with interactive click-to-place shapes and live FPS counter**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-22T00:16:56Z
- **Completed:** 2026-03-22T00:20:53Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Complete demo app (320 lines) exercising every v1 feature of cels-sdl3 in a cohesive landscape scene
- Bundled assets: PressStart2P font (OFL 1.1), hand-created 32x48 pixel tree sprite (CC0), license attribution file
- Interactive click-to-place mechanic with cycling colors (up to 256 shapes) validates input + draw primitive integration
- Live FPS counter with double-buffered string validates text cache invalidation (TEXT-03)

## Task Commits

Each task was committed atomically:

1. **Task 1: Bundle assets -- font, sprite, and license file** - `2b21fb0` (feat)
2. **Task 2: Demo app implementation and CMake integration** - `7b95bbf` (feat)

## Files Created/Modified
- `examples/assets/PressStart2P-Regular.ttf` - Retro pixel font (OFL 1.1 licensed)
- `examples/assets/tree.png` - 32x48 pixel-art tree sprite (CC0, ImageMagick generated)
- `examples/assets/LICENSE-assets.txt` - Attribution for both bundled assets
- `examples/demo/main.c` - Full demo app: landscape scene with all v1 features
- `CMakeLists.txt` - Added demo executable target

## Decisions Made
- **Sprite placement:** Tree sprite attached to window entity via `cels_entity_set_component` (required by TextureLoadSystem query pattern)
- **Font loading:** Used dedicated FontLoader system at OnLoad phase with static guard, matching text example pattern
- **FPS invalidation:** Double-buffered static string arrays -- pointer swap between buffers triggers text system's pointer-change detection
- **Tree sprite source:** Hand-created via ImageMagick (transparent background, simple green tree on brown trunk) -- avoids third-party download dependency
- **Clear color:** Set sky-blue (135, 206, 235) via copy-modify-set on SDL3_Renderer in composition init block

## Deviations from Plan

None -- plan executed exactly as written.

## Issues Encountered
- Google Fonts download URL returned HTML instead of ZIP -- resolved by fetching font directly from Google's GitHub fonts repository
- Pre-existing cels framework test build failures (missing headers) do not affect cels-sdl3 targets -- built demo target specifically

## User Setup Required

None -- no external service configuration required.

## Next Phase Readiness
- Phase 10 Plan 01 is the only plan in this phase (final phase)
- All 10 phases complete: the cels-sdl3 library delivers the full v1 feature set
- Demo app serves as living documentation and integration validation

---
*Phase: 10-example-application*
*Completed: 2026-03-22*
