---
phase: 01-sdl3-bootstrap
plan: 01
subsystem: infra
tags: [cmake, sdl3, sdl3_image, sdl3_ttf, fetchcontent, cels, interface-library]

# Dependency graph
requires:
  - phase: none
    provides: first phase -- no prior dependencies
provides:
  - CMake INTERFACE library target (cels-sdl3) with FetchContent for SDL3 ecosystem
  - Public header with CEL_Module(SDL3_Engine), SDL3_ContextConfig, SDL3_ContextState, SDL3Context
  - SDL3 init/shutdown lifecycle via CELS observers (entity create/destroy)
  - Directory scaffold for all future phases (window, input, renderer, texture, text)
affects: [02-window-provider, 01-02 minimal example build]

# Tech tracking
tech-stack:
  added: [SDL3 3.4.2, SDL3_image 3.4.0, SDL3_ttf 3.2.2]
  patterns: [CELS INTERFACE library, FetchContent dependency, lifecycle observer init/shutdown, CEL_State cross-TU state]

key-files:
  created:
    - CMakeLists.txt
    - include/cels_sdl3.h
    - src/sdl3_internal.h
    - src/sdl3_module.c
    - src/sdl3_init.c
    - examples/minimal/main.c
    - .gitignore
  modified: []

key-decisions:
  - "SDL3 3.4.2 + SDL3_image 3.4.0 + SDL3_ttf 3.2.2 confirmed compatible (no fallback to 3.2.x needed)"
  - "No IMG_Init/IMG_Quit calls -- SDL3_image auto-initializes, functions removed in 3.x"
  - "SDL_Init returns bool -- use if (!SDL_Init(...)) pattern"
  - "Shutdown order: TTF_Quit then SDL_Quit (reverse of init)"
  - "All CELS declarations in sdl3_module.c per Pitfall 5 (per-TU static IDs)"

patterns-established:
  - "INTERFACE library pattern: sources compile in consumer context, matching cels-ncurses"
  - "Sibling CELS detection: cmake-build-debug/_deps/ path for flecs/yyjson"
  - "Lifecycle observer pattern: CEL_Lifecycle + CEL_Observe(on_create/on_destroy) for resource management"
  - "CEL_State ownership: static state in implementation TU, registered via cels_state_bind"

# Metrics
duration: 4min
completed: 2026-03-15
---

# Phase 1 Plan 1: SDL3 Bootstrap - Build System and Lifecycle Summary

**CMake INTERFACE library with FetchContent for SDL3/SDL3_image/SDL3_ttf, CELS module with lifecycle observers for init/shutdown, and directory scaffold for all future phases**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-15T20:56:26Z
- **Completed:** 2026-03-15T21:00:07Z
- **Tasks:** 3
- **Files modified:** 8

## Accomplishments
- CMake build system fetches SDL3 ecosystem via FetchContent and creates INTERFACE library target
- Public header declares full CELS module API: SDL3_Engine, SDL3_ContextConfig, SDL3_ContextState, SDL3Context composition
- Init/shutdown lifecycle wired through CELS observers -- SDL_Init(SDL_INIT_VIDEO) + TTF_Init on create, TTF_Quit + SDL_Quit on destroy
- CMake configure verified successful with SDL3 3.4.2, SDL3_image 3.4.0, SDL3_ttf 3.2.2

## Task Commits

Each task was committed atomically:

1. **Task 1: CMakeLists.txt, directory scaffold, and example stub** - `7affa58` (feat)
2. **Task 2: Public header, internal header, and module implementation** - `967dfab` (feat)
3. **Task 3: CMake configure verification** - `ec12c86` (chore)

## Files Created/Modified
- `CMakeLists.txt` - INTERFACE library target, FetchContent for SDL3/SDL3_image/SDL3_ttf, sibling CELS detection, example build
- `include/cels_sdl3.h` - Public API: CEL_Module(SDL3_Engine), SDL3_ContextConfig, SDL3_ContextState, SDL3Context composition
- `src/sdl3_internal.h` - Internal declarations for sdl3_init/sdl3_shutdown
- `src/sdl3_module.c` - CEL_Module(SDL3_Engine, init), lifecycle observers, composition
- `src/sdl3_init.c` - SDL3 init/shutdown implementation with state ownership
- `examples/minimal/main.c` - Stub for CMake configure (overwritten by Plan 01-02)
- `.gitignore` - Build directory exclusions
- `src/window/.gitkeep`, `src/input/.gitkeep`, `src/renderer/.gitkeep`, `src/texture/.gitkeep`, `src/text/.gitkeep` - Stub directories

## Decisions Made
- **SDL3 version compatibility confirmed:** SDL3_ttf 3.2.2 works with SDL3 3.4.2 -- no fallback to 3.2.x family needed
- **No IMG_Init/IMG_Quit:** SDL3_image 3.x auto-initializes; calling removed functions would cause linker errors
- **Bool return pattern:** SDL3's SDL_Init returns bool (true=success), not int like SDL2
- **All CELS declarations in single TU:** sdl3_module.c contains all CEL_Lifecycle, CEL_Observe, CEL_Module, CEL_Composition code per Pitfall 5

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Build system foundation complete, ready for Plan 01-02 (minimal example app, full build and run verification)
- All stub directories in place for future phases (window, input, renderer, texture, text)
- SDL3_ttf compatibility with SDL3 3.4.2 confirmed -- no version concerns going forward

---
*Phase: 01-sdl3-bootstrap*
*Completed: 2026-03-15*
