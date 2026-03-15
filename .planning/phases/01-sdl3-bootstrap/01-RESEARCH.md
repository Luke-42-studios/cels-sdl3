# Phase 1: SDL3 Bootstrap - Research

**Researched:** 2026-03-15
**Domain:** CMake INTERFACE library scaffold, SDL3 FetchContent, init/shutdown lifecycle via CELS observers
**Confidence:** HIGH

## Summary

Phase 1 establishes the build system foundation and SDL3 init/shutdown lifecycle for cels-sdl3. The research investigated four areas: (1) the cels-ncurses INTERFACE library pattern to replicate exactly, (2) the CELS framework macro API for modules, lifecycles, observers, and state, (3) current SDL3/SDL3_image/SDL3_ttf release versions and initialization APIs, and (4) CMake FetchContent best practices.

Key findings: SDL3_image 3.x has **removed** `IMG_Init()` and `IMG_Quit()` entirely -- the library initializes automatically when loading images. SDL3_ttf still requires `TTF_Init()` and `TTF_Quit()`. SDL3's `SDL_Init()` returns `bool` (not `int`), and `SDL_INIT_VIDEO` implicitly initializes `SDL_INIT_EVENTS`. The CELS framework uses a specific pattern: `CEL_Module(Name, init) { ... }` for module definition, `CEL_Lifecycle(Name)` + `CEL_Observe(Name, on_create/on_destroy)` for lifecycle hooks, and `CEL_State(Name)` + `cels_state_bind()` for cross-TU state.

**Primary recommendation:** Replicate the cels-ncurses CMakeLists.txt structure exactly, using `CEL_Lifecycle` + `CEL_Observe` for SDL3 init on entity creation and shutdown on entity destruction, with `CEL_Module(SDL3_Engine, init)` bundling all registrations. Pin SDL3 to `release-3.4.2`, SDL3_image to `release-3.4.0`, SDL3_ttf to `release-3.2.2`.

## Standard Stack

### Core

| Library | Version | GIT_TAG | Purpose | Confidence |
|---------|---------|---------|---------|------------|
| SDL3 | 3.4.2 | `release-3.4.2` | Core library (video, events, windowing) | HIGH -- verified via [GitHub releases](https://github.com/libsdl-org/SDL/releases) |
| SDL3_image | 3.4.0 | `release-3.4.0` | PNG/JPG image loading | HIGH -- verified via [GitHub releases](https://github.com/libsdl-org/SDL_image/releases) |
| SDL3_ttf | 3.2.2 | `release-3.2.2` | TrueType font rendering | HIGH -- verified via [GitHub releases](https://github.com/libsdl-org/SDL_ttf/releases) |
| CELS | 0.5.3 | sibling directory | ECS framework, module system | HIGH -- read from source |
| CMake | >= 3.21 | N/A | Build system (matches CELS minimum) | HIGH -- read from CELS CMakeLists.txt |

### Version Compatibility

| SDL3_image 3.4.0 | Requires SDL3 >= 3.4.0 | HIGH -- verified via [Arch package](https://archlinux.org/packages/extra-testing/x86_64/sdl3_image/) and [announcement](https://discourse.libsdl.org/t/announcing-sdl-image-3-4-0/65822) |
|---|---|---|
| SDL3_ttf 3.2.2 | Requires SDL3 >= 3.2.0 | MEDIUM -- release predates 3.4.x, should be compatible via ABI stability |
| CELS | C99 standard | HIGH -- read from CMakeLists.txt |

### CMake Target Names

| Library | Target | Include Path | Confidence |
|---------|--------|-------------|------------|
| SDL3 | `SDL3::SDL3` | `<SDL3/SDL.h>` | HIGH |
| SDL3_image | `SDL3_image::SDL3_image` | `<SDL3_image/SDL_image.h>` | HIGH |
| SDL3_ttf | `SDL3_ttf::SDL3_ttf` | `<SDL3_ttf/SDL_ttf.h>` | HIGH |

**Installation (FetchContent only, per user decision):**
```cmake
include(FetchContent)

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.2
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG        release-3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG        release-3.2.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3 SDL3_image SDL3_ttf)
```

## Architecture Patterns

### Pattern 1: CELS INTERFACE Library (from cels-ncurses)

The cels-ncurses CMakeLists.txt establishes the exact pattern to follow. Key elements verified from source:

**Sibling CELS auto-detection:**
```cmake
if(NOT TARGET cels)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        # Top-level build (development) -- use sibling cels directory
        set(FETCHCONTENT_SOURCE_DIR_FLECS
            "${CMAKE_CURRENT_SOURCE_DIR}/../cels/cmake-build-debug/_deps/flecs-src" CACHE PATH "")
        set(FETCHCONTENT_SOURCE_DIR_YYJSON
            "${CMAKE_CURRENT_SOURCE_DIR}/../cels/cmake-build-debug/_deps/yyjson-src" CACHE PATH "")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cels cels)
    else()
        message(FATAL_ERROR
            "cels-sdl3 requires the 'cels' target. "
            "Add cels via FetchContent or add_subdirectory before cels-sdl3.")
    endif()
endif()
```
Source: `/home/cachy/workspaces/libs/cels-ncurses/CMakeLists.txt` lines 36-50

**INTERFACE library target:**
```cmake
add_library(cels-sdl3 INTERFACE)

target_sources(cels-sdl3 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl3_module.c
    # ... additional sources
)

target_include_directories(cels-sdl3 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cels-sdl3 INTERFACE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    cels
)
```
Source: cels-ncurses CMakeLists.txt pattern, adapted

**Confidence:** HIGH -- directly read from cels-ncurses source

### Pattern 2: CELS Module Definition (from cels-ncurses)

The module pattern uses `CEL_Module` with two arities:

**Header (forward declaration):**
```c
// In include/cels_sdl3.h
CEL_Module(SDL3_Engine);
```
This expands to `extern void SDL3_Engine_init(void)` and a `SDL3_Engine_register()` inline.

**Implementation (definition):**
```c
// In src/sdl3_module.c
CEL_Module(SDL3_Engine, init) {
    cels_register(SDL3_ContextLC,
                  /* ... all components, systems, lifecycles ... */);
}
```
This defines `SDL3_Engine_init()` which calls `cels_module_register("SDL3_Engine")` then executes the body.

**Consumer usage:**
```c
cels_main() {
    cels_register(SDL3_Engine);
    cels_session(World) {
        while (cels_running()) cels_step(0);
    }
}
```
Source: `/home/cachy/workspaces/libs/cels-ncurses/src/ncurses_module.c` lines 176-183, `/home/cachy/workspaces/libs/cels-ncurses/examples/minimal.c` lines 116-122

**Confidence:** HIGH -- directly read from source

### Pattern 3: Lifecycle Observer for Init/Shutdown (from cels-ncurses)

CELS uses `CEL_Lifecycle` + `CEL_Observe` for entity lifecycle hooks. This is how cels-ncurses triggers terminal init when a window config entity is created:

```c
// Define the lifecycle
CEL_Lifecycle(SDL3_ContextLC);

// Observer: fires when entity is created with bound lifecycle
CEL_Observe(SDL3_ContextLC, on_create) {
    // `entity` is available as implicit parameter
    // Initialize SDL3 here
}

// Observer: fires when entity is destroyed
CEL_Observe(SDL3_ContextLC, on_destroy) {
    // Shutdown SDL3 here
}

// In module init, register the lifecycle
CEL_Module(SDL3_Engine, init) {
    cels_register(SDL3_ContextLC, /* ... */);
}

// In composition, bind lifecycle to entity
CEL_Composition(SDL3Context) {
    cel_has(SDL3_ContextConfig, .video = true);
    cels_lifecycle_bind_entity(SDL3_ContextLC_id, cels_get_current_entity());
}
```
Source: `/home/cachy/workspaces/libs/cels-ncurses/src/ncurses_module.c` lines 44-64, 189-196

**Confidence:** HIGH -- directly read from source

### Pattern 4: CEL_State for Cross-TU State (from cels-ncurses)

State singletons allow data to be shared across translation units:

**Header (declaration):**
```c
// In include/cels_sdl3.h
CEL_Define_State(SDL3_ContextState) {
    bool initialized;
    bool video_ready;
    bool ttf_ready;
};
```

**Implementation (.c file that owns the state):**
```c
// In src/sdl3_init.c (or wherever state is owned)
static struct SDL3_ContextState SDL3_ContextState = { .initialized = false };

// During init:
SDL3_ContextState_register();
cels_state_bind(SDL3_ContextState);
```

**Consumer reads state:**
```c
const struct SDL3_ContextState* ctx = cel_read(SDL3_ContextState);
if (ctx && ctx->initialized) { /* ... */ }
```

Source: `/home/cachy/workspaces/libs/cels-ncurses/src/window/tui_window.c` lines 50-51, 137-139, `/home/cachy/workspaces/libs/cels-ncurses/include/cels_ncurses.h` lines 110-116

**Confidence:** HIGH -- directly read from source

### Recommended Project Structure

Based on cels-ncurses actual structure + user decision for full scaffold:

```
cels-sdl3/
├── CMakeLists.txt                  # INTERFACE library + FetchContent
├── include/
│   └── cels_sdl3.h                 # Public API (CEL_Module, components, state, compositions)
├── src/
│   ├── sdl3_module.c               # CEL_Module(SDL3_Engine, init), compositions, lifecycles
│   ├── sdl3_internal.h             # Internal-only declarations (not for consumers)
│   ├── sdl3_init.c                 # SDL3 init/shutdown implementation, state ownership
│   ├── window/                     # (stub for Phase 2)
│   ├── input/                      # (stub for Phase 4)
│   ├── renderer/                   # (stub for Phase 5)
│   ├── texture/                    # (stub for Phase 7)
│   └── text/                       # (stub for Phase 8)
├── examples/
│   └── minimal/
│       ├── CMakeLists.txt
│       └── main.c                  # Minimal: init + shutdown test
└── .planning/
```

Note: cels-ncurses uses flat `include/` with `cels_ncurses.h` (not `include/cels-ncurses/`). The user decision says "Match cels-ncurses directory structure." The actual cels-ncurses has headers directly in `include/` without a subdirectory.

**Confidence:** HIGH -- directly read from cels-ncurses file tree

### Anti-Patterns to Avoid

- **Do NOT call `IMG_Init()` or `IMG_Quit()`**: These functions have been removed in SDL3_image. The library auto-initializes when loading images. Calling them will cause compilation errors.
- **Do NOT use `FIND_PACKAGE_ARGS` in FetchContent**: User decision is "FetchContent only, no system package fallback."
- **Do NOT create a `CEL_DefineModule` or `CEL_DefineFeature`**: Phase 1 only needs `CEL_Module`, `CEL_Lifecycle`, `CEL_Observe`, `CEL_Component`, and `CEL_Define_State`. Feature/Provider model comes in later phases.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| SDL3 dependency fetching | Custom download scripts | CMake FetchContent | Standard, integrates with CMake targets |
| Init/shutdown lifecycle | Manual init() / shutdown() calls | CELS `CEL_Lifecycle` + `CEL_Observe` | Ties to ECS entity lifecycle, automatic cleanup |
| Cross-TU state sharing | Global variables or extern structs | CELS `CEL_State` + `cels_state_bind` | Integrates with CELS reactivity, cel_read/cel_mutate |
| Module registration | Manual function call chains | CELS `CEL_Module(Name, init)` | Single `cels_register(SDL3_Engine)` does everything |
| CELS dependency resolution | Manual add_subdirectory chains | CelsDeps.cmake `cels_require(sdl3)` | Already registered in CelsDeps.cmake |

## Common Pitfalls

### Pitfall 1: Calling IMG_Init() / IMG_Quit() (SDL3_image removed them)

**What goes wrong:** Code calls `IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG)` and `IMG_Quit()` as was required in SDL2_image. These functions no longer exist in SDL3_image and will cause linker errors.
**Why it happens:** Every SDL2 tutorial and most SDL3 migration guides from early 2025 assume these still exist. Training data confidently suggests using them.
**How to avoid:** SDL3_image auto-initializes. Just link `SDL3_image::SDL3_image` and call `IMG_LoadTexture()` etc. directly. No init, no quit.
**Warning signs:** Linker errors: undefined reference to `IMG_Init`, `IMG_Quit`.
**Confidence:** HIGH -- verified via [SDL forum announcement](https://discourse.libsdl.org/t/sdl-image-removed-img-init-and-img-quit/55813)

### Pitfall 2: SDL_Init returns bool, not int

**What goes wrong:** Code checks `if (SDL_Init(...) != 0)` or `if (SDL_Init(...) < 0)` as in SDL2. In SDL3, `SDL_Init()` returns `bool` -- `true` on success, `false` on failure.
**Why it happens:** SDL2 muscle memory. Every SDL2 tutorial uses `if (SDL_Init(...) < 0)`.
**How to avoid:** Use `if (!SDL_Init(SDL_INIT_VIDEO)) { /* error */ }`.
**Confidence:** HIGH -- verified via [SDL3 wiki](https://wiki.libsdl.org/SDL3/SDL_Init)

### Pitfall 3: SDL_INIT_VIDEO already implies SDL_INIT_EVENTS

**What goes wrong:** Code passes `SDL_INIT_VIDEO | SDL_INIT_EVENTS` which is harmless but unnecessary.
**Why it happens:** The user decision says "VIDEO and EVENTS subsystem flags." But `SDL_INIT_VIDEO` already implies `SDL_INIT_EVENTS` per official docs.
**How to avoid:** `SDL_Init(SDL_INIT_VIDEO)` is sufficient. Adding `SDL_INIT_EVENTS` is redundant but not harmful.
**Confidence:** HIGH -- verified via [SDL3 wiki SDL_InitFlags](https://wiki.libsdl.org/SDL3/SDL_InitFlags)

### Pitfall 4: Wrong shutdown order

**What goes wrong:** Calling `SDL_Quit()` before `TTF_Quit()` invalidates SDL state that TTF may still reference.
**Why it happens:** It's natural to quit the main library first.
**How to avoid:** Shutdown in reverse init order: `TTF_Quit()` then `SDL_Quit()`. Since IMG_Init/IMG_Quit are removed, no IMG cleanup needed.
**Warning signs:** Segfault or SDL_GetError on shutdown, Valgrind use-after-free.
**Confidence:** HIGH -- standard resource management pattern

### Pitfall 5: CEL_Component IDs are per-TU statics

**What goes wrong:** Component IDs defined by `CEL_Component` are `static` per translation unit. If you define a component in a header and use it across multiple .c files, each TU gets its own ID variable. Only the TU where `cels_register()` is called gets the correct ID.
**Why it happens:** CELS's macro design requires all component usage to route through the module's registration TU.
**How to avoid:** All `CEL_Observe`, `CEL_System`, `CEL_Composition` code that references component IDs must be in the same .c file as the `CEL_Module` init, or use `CEL_Define_*` extern declarations with proper registration. Follow the cels-ncurses pattern where `ncurses_module.c` contains all CELS declarations.
**Warning signs:** Component lookups return NULL, entity queries return no results despite entities existing.
**Confidence:** HIGH -- documented in cels-ncurses source comment at line 27 of ncurses_module.c

### Pitfall 6: SDL_Init must be called on main thread

**What goes wrong:** SDL3 requires `SDL_Init(SDL_INIT_VIDEO)` on the main thread. If the CELS lifecycle observer fires on a worker thread, init fails silently or crashes.
**Why it happens:** CELS observers run in the ECS tick context, which in single-threaded mode is the main thread. But this assumption should be explicit.
**How to avoid:** Ensure the CELS session runs on the main thread (it does by default with `cels_main()`). Document this constraint.
**Confidence:** HIGH -- [SDL3 wiki](https://wiki.libsdl.org/SDL3/SDL_Init) states VIDEO "should be initialized on the main thread"

## Code Examples

### Example 1: SDL3 Initialization (Phase 1 core)

```c
// Source: SDL3 wiki + cels-ncurses pattern

// In on_create observer:
if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    // Return error -- do NOT abort
    return;
}

if (!TTF_Init()) {
    SDL_Log("TTF_Init failed: %s", SDL_GetError());
    SDL_Quit();
    return;
}

// Note: NO IMG_Init() call -- SDL3_image auto-initializes
```

### Example 2: SDL3 Shutdown (Phase 1 core)

```c
// Source: SDL3 wiki + verified API

// In on_destroy observer:
TTF_Quit();    // TTF first (depends on SDL internals)
// Note: NO IMG_Quit() call -- SDL3_image has no quit function
SDL_Quit();    // SDL last
```

### Example 3: Module Registration Pattern (from cels-ncurses)

```c
// In cels_sdl3.h (public header):
#include <cels/cels.h>

CEL_Module(SDL3_Engine);

CEL_Component(SDL3_ContextConfig) {
    bool video;
};

CEL_Define_State(SDL3_ContextState) {
    bool initialized;
    bool ttf_ready;
};

CEL_Define_Composition(SDL3Context, bool video;);
#define SDL3Context(...) cel_init(SDL3Context, __VA_ARGS__)

// In src/sdl3_module.c:
#include <cels_sdl3.h>
#include "sdl3_internal.h"

CEL_State(SDL3_ContextState);

CEL_Lifecycle(SDL3_ContextLC);

CEL_Observe(SDL3_ContextLC, on_create) {
    // Check for duplicate init
    const SDL3_ContextConfig* config = cel_watch(entity, SDL3_ContextConfig);
    if (!config) return;
    sdl3_init(config);  // Defined in sdl3_init.c
}

CEL_Observe(SDL3_ContextLC, on_destroy) {
    (void)entity;
    sdl3_shutdown();  // Defined in sdl3_init.c
}

CEL_Module(SDL3_Engine, init) {
    cels_register(SDL3_ContextState, SDL3_ContextLC,
                  SDL3_ContextConfig);
}

CEL_Composition(SDL3Context) {
    cel_has(SDL3_ContextConfig, .video = cel.video);
    cels_lifecycle_bind_entity(SDL3_ContextLC_id, cels_get_current_entity());
}
```

### Example 4: Consumer Usage (minimal example)

```c
#include <cels/cels.h>
#include <cels_sdl3.h>

CEL_Compose(World) {
    SDL3Context(.video = true) {}
}

cels_main() {
    cels_register(SDL3_Engine);
    cels_session(World) {
        // SDL3 is now initialized via lifecycle observer
        // Run a few frames then exit
        int frames = 0;
        while (cels_running() && frames < 5) {
            cels_step(0.016f);
            frames++;
        }
        // SDL3 shuts down automatically when World entity is destroyed
    }
}
```

### Example 5: CMakeLists.txt (complete Phase 1)

```cmake
cmake_minimum_required(VERSION 3.21)
project(cels-sdl3 VERSION 0.1.0 LANGUAGES C)

# ============================================================================
# Dependencies
# ============================================================================

# CELS framework -- consumer provides, or auto-detect sibling
if(NOT TARGET cels)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        set(FETCHCONTENT_SOURCE_DIR_FLECS
            "${CMAKE_CURRENT_SOURCE_DIR}/../cels/cmake-build-debug/_deps/flecs-src" CACHE PATH "")
        set(FETCHCONTENT_SOURCE_DIR_YYJSON
            "${CMAKE_CURRENT_SOURCE_DIR}/../cels/cmake-build-debug/_deps/yyjson-src" CACHE PATH "")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cels cels)
    else()
        message(FATAL_ERROR
            "cels-sdl3 requires the 'cels' target. "
            "Add cels via FetchContent or add_subdirectory before cels-sdl3.")
    endif()
endif()

# SDL3 ecosystem -- FetchContent only (no system package fallback)
include(FetchContent)

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.2
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG        release-3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG        release-3.2.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3 SDL3_image SDL3_ttf)

# ============================================================================
# INTERFACE library target
# ============================================================================

add_library(cels-sdl3 INTERFACE)

target_sources(cels-sdl3 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl3_module.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl3_init.c
)

target_include_directories(cels-sdl3 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cels-sdl3 INTERFACE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    cels
)

# ============================================================================
# Examples (only when building this repo directly)
# ============================================================================

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_executable(minimal examples/minimal/main.c)
    target_link_libraries(minimal PRIVATE cels-sdl3)
    set_target_properties(minimal PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
endif()
```

## State of the Art

| Old Approach (SDL2 era) | Current Approach (SDL3) | When Changed | Impact |
|-------------------------|------------------------|--------------|--------|
| `IMG_Init(IMG_INIT_PNG)` / `IMG_Quit()` | No init/quit needed -- auto-initializes | SDL3_image 3.x | Simplifies Phase 1 shutdown -- only TTF_Quit + SDL_Quit needed |
| `SDL_Init()` returns `int` (0 = success) | `SDL_Init()` returns `bool` (`true` = success) | SDL3 | All error checks must use `if (!SDL_Init(...))` |
| `SDL_bool` enum (`SDL_TRUE`/`SDL_FALSE`) | Standard C `bool` (`true`/`false`) | SDL3 | No more SDL_TRUE/SDL_FALSE |
| `SDL_CreateWindow(title, x, y, w, h, flags)` | `SDL_CreateWindow(title, w, h, flags)` | SDL3 | Position removed from creation (future phases) |
| `SDL_RenderCopy()` | `SDL_RenderTexture()` | SDL3 | Renamed (future phases) |
| `SDL_KEYDOWN` | `SDL_EVENT_KEY_DOWN` | SDL3 | Event constants renamed (future phases) |

## Open Questions

1. **SDL3_ttf 3.2.2 compatibility with SDL3 3.4.2**
   - What we know: SDL3_ttf 3.2.2 was released March 2025, before SDL3 3.4.x. SDL3 maintains ABI compatibility within the 3.x series.
   - What's unclear: Whether SDL3_ttf 3.2.2 builds correctly when linked against SDL3 3.4.2 via FetchContent (the SDL3_ttf CMake may `find_package(SDL3 3.2.0)` which 3.4.2 satisfies).
   - Recommendation: Test during implementation. If incompatible, fall back to using SDL3 `release-3.2.30` + SDL3_image `release-3.2.6` + SDL3_ttf `release-3.2.2` (all 3.2.x family). **Fallback version set:**
     - SDL3: `release-3.2.30`
     - SDL3_image: `release-3.2.6`
     - SDL3_ttf: `release-3.2.2`

2. **CELS cmake-build-debug path for sibling detection**
   - What we know: cels-ncurses hardcodes `../cels/cmake-build-debug/_deps/` for FETCHCONTENT_SOURCE_DIR overrides.
   - What's unclear: Whether the developer's CELS build uses `cmake-build-debug` or `build` as the build directory. The CELS repo has both `build/` and `cmake-build-debug/` directories.
   - Recommendation: Check both paths, prefer `cmake-build-debug` (CLion convention) matching cels-ncurses. Can also check `build/` as fallback.

3. **Header naming convention**
   - What we know: cels-ncurses uses `cels_ncurses.h` (flat in `include/`), not `cels-ncurses/cels_ncurses.h`.
   - What's unclear: User decision says "SDL3_ prefix for all public API functions and types" -- should the header be `cels_sdl3.h` (matching cels-ncurses naming pattern)?
   - Recommendation: Use `cels_sdl3.h` in `include/`, matching cels-ncurses exactly.

## Sources

### Primary (HIGH confidence)
- `/home/cachy/workspaces/libs/cels-ncurses/CMakeLists.txt` -- INTERFACE library pattern, sibling detection, target_sources
- `/home/cachy/workspaces/libs/cels-ncurses/src/ncurses_module.c` -- CEL_Module, CEL_Lifecycle, CEL_Observe, CEL_Composition, cels_register patterns
- `/home/cachy/workspaces/libs/cels-ncurses/include/cels_ncurses.h` -- CEL_Define_State, CEL_Component, CEL_Define_Composition patterns
- `/home/cachy/workspaces/libs/cels-ncurses/src/window/tui_window.c` -- CEL_State ownership, cels_state_bind, init/shutdown implementation
- `/home/cachy/workspaces/libs/cels/include/cels/cels.h` -- CEL_Module macro expansion, CEL_Lifecycle, CEL_Observe, CEL_State
- `/home/cachy/workspaces/libs/cels/cmake/CelsDeps.cmake` -- cels_require(sdl3) already registered
- `/home/cachy/workspaces/libs/cels/CMakeLists.txt` -- C99 standard, version 0.5.3
- [SDL3 GitHub releases](https://github.com/libsdl-org/SDL/releases) -- release-3.4.2 (Feb 21, 2026)
- [SDL3_image GitHub releases](https://github.com/libsdl-org/SDL_image/releases) -- release-3.4.0 (Jan 22, 2026)
- [SDL3_ttf GitHub releases](https://github.com/libsdl-org/SDL_ttf/releases) -- release-3.2.2 (Mar 31, 2025)
- [SDL3 SDL_Init wiki](https://wiki.libsdl.org/SDL3/SDL_Init) -- bool return, flags
- [SDL3 SDL_InitFlags wiki](https://wiki.libsdl.org/SDL3/SDL_InitFlags) -- VIDEO implies EVENTS
- [SDL3 SDL_Quit wiki](https://wiki.libsdl.org/SDL3/SDL_Quit) -- void return
- [SDL3_ttf TTF_Init wiki](https://wiki.libsdl.org/SDL3_ttf/TTF_Init) -- bool return
- [SDL3_ttf TTF_Quit wiki](https://wiki.libsdl.org/SDL3_ttf/TTF_Quit) -- void return
- [SDL forum: IMG_Init/IMG_Quit removed](https://discourse.libsdl.org/t/sdl-image-removed-img-init-and-img-quit/55813) -- no init/quit needed

### Secondary (MEDIUM confidence)
- [Arch Linux sdl3_image package](https://archlinux.org/packages/extra-testing/x86_64/sdl3_image/) -- SDL3_image 3.4.0 requires SDL3 3.4.0+

### Tertiary (LOW confidence)
- None -- all claims verified with primary sources

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all versions verified via GitHub releases pages
- Architecture: HIGH -- all patterns read directly from cels-ncurses source code
- Init/Shutdown API: HIGH -- verified via SDL3 wiki documentation
- Pitfalls: HIGH -- IMG_Init removal verified, bool return verified, per-TU static documented in source
- Version compatibility: MEDIUM -- SDL3_ttf 3.2.2 + SDL3 3.4.2 compatibility inferred but not tested

**Research date:** 2026-03-15
**Valid until:** 2026-04-15 (SDL3 ecosystem is actively releasing; check for new versions monthly)
