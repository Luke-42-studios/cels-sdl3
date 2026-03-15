# Stack Research

**Domain:** SDL3 CELS Backend Module
**Researched:** 2026-03-15
**Confidence:** MEDIUM (versions need live verification -- WebSearch/WebFetch/Context7 were unavailable during this research session)

## Important Notice on Version Confidence

All version numbers below are based on training data with a cutoff of early-to-mid 2025. SDL3 had its initial stable release in early 2025. Versions cited here were accurate as of that window but **must be verified against the official GitHub release pages before pinning in CMakeLists.txt**:

- https://github.com/libsdl-org/SDL/releases
- https://github.com/libsdl-org/SDL_image/releases
- https://github.com/libsdl-org/SDL_ttf/releases

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended | Confidence |
|------------|---------|---------|-----------------|------------|
| SDL3 | 3.2.x (verify) | Windowing, input, 2D rendering, event loop | The project requirement. SDL3 is the current major version with revamped API, properties-based configuration, improved multi-window, and GPU abstraction. SDL2 is legacy. | HIGH (library choice), MEDIUM (exact version) |
| SDL3_image | 3.2.x (verify) | PNG, JPG, WebP texture loading | Standard SDL3 companion for image I/O. Replaces hand-rolling stb_image or libpng integration. Matches SDL3 versioning cadence. | HIGH (library choice), MEDIUM (exact version) |
| SDL3_ttf | 3.2.x (verify) | TrueType/OpenType font rendering | Standard SDL3 companion for text. Uses HarfBuzz for shaping internally. Only sane option for SDL3 text rendering without building a custom glyph pipeline. | HIGH (library choice), MEDIUM (exact version) |
| CMake | >= 3.24 | Build system | Required for FetchContent with `FIND_PACKAGE_ARGS`, `FetchContent_MakeAvailable`, and modern SDL3 CMake targets. SDL3 itself requires CMake >= 3.16 but 3.24+ gives best FetchContent ergonomics. | HIGH |
| C99 | -- | Source language | Project constraint. SDL3's public API is C-compatible. CELS exposes C99 API via `extern "C"`. No C++ needed in this module. | HIGH |

### Supporting Libraries (Transitive -- pulled by SDL3_ttf)

| Library | Version | Purpose | When to Use | Confidence |
|---------|---------|---------|-------------|------------|
| FreeType | (bundled by SDL3_ttf) | Font rasterization | Transitive dependency -- you do not interact with it directly | HIGH |
| HarfBuzz | (bundled by SDL3_ttf) | Text shaping (ligatures, complex scripts) | Transitive dependency -- SDL3_ttf 3.x uses HarfBuzz internally | MEDIUM |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| CMake >= 3.24 | Build system, dependency fetching | FetchContent for SDL3 ecosystem |
| pkg-config | Fallback library discovery | Used by `find_package(SDL3)` on Linux when system-installed |
| ccache | Compilation cache | SDL3 is a large C codebase; ccache dramatically speeds rebuilds when fetched from source |
| clang-format | Code formatting | Match CELS project formatting conventions |
| Valgrind / ASan | Memory debugging | SDL3 programs manage GPU resources manually; leak detection is essential |

## SDL3 vs SDL2: Why SDL3

This is not optional -- the PROJECT.md specifies SDL3. But for documentation completeness, here is why SDL3 is the right call:

| Aspect | SDL2 | SDL3 |
|--------|------|------|
| Multi-window | Supported but awkward | First-class, cleaner API |
| Renderer | SDL_Renderer (software/GL backend) | SDL_Renderer (now backed by SDL_GPU internally) |
| GPU API | None | SDL_GPU -- cross-platform GPU abstraction |
| Properties | Not available | `SDL_SetProperty` / `SDL_GetProperty` -- extensible config |
| Input | Good | Improved gamepad DB, touch, pen support |
| Init/Shutdown | `SDL_Init` / `SDL_Quit` flags | Same pattern, cleaner subsystem management |
| CMake | Bolted on | First-class CMake support with exported targets |
| API naming | Mixed conventions | Consistent `SDL_VerbNoun` naming |
| Maintenance | Security fixes only | Active development |

## CMake FetchContent Setup

This is the recommended pattern for fetching SDL3 and companions. It tries `find_package` first (system install), then falls back to FetchContent (source build).

```cmake
cmake_minimum_required(VERSION 3.24)
project(cels-sdl3 LANGUAGES C)

include(FetchContent)

# --- SDL3 Core ---
# FIND_PACKAGE_ARGS lets FetchContent try find_package() first.
# If a system SDL3 >= 3.2.0 exists, it uses that. Otherwise, fetches source.
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.2.x   # VERIFY: pin to latest stable tag
    GIT_SHALLOW    TRUE
    FIND_PACKAGE_ARGS NAMES SDL3
)

# --- SDL3_image ---
FetchContent_Declare(
    SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG        release-3.2.x   # VERIFY: pin to latest stable tag
    GIT_SHALLOW    TRUE
    FIND_PACKAGE_ARGS NAMES SDL3_image
)

# --- SDL3_ttf ---
FetchContent_Declare(
    SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG        release-3.2.x   # VERIFY: pin to latest stable tag
    GIT_SHALLOW    TRUE
    FIND_PACKAGE_ARGS NAMES SDL3_ttf
)

FetchContent_MakeAvailable(SDL3 SDL3_image SDL3_ttf)

# --- cels-sdl3 INTERFACE library ---
add_library(cels-sdl3 INTERFACE)

target_include_directories(cels-sdl3 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# SDL3 exports these targets:
#   SDL3::SDL3         - shared/static SDL3 core
#   SDL3_image::SDL3_image - image loading
#   SDL3_ttf::SDL3_ttf     - text rendering
target_link_libraries(cels-sdl3 INTERFACE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
)

# Consumer must also link cels -- handled externally, not here.
# The consumer's CMakeLists.txt links both cels and cels-sdl3.
```

### Key CMake Details

**INTERFACE library pattern:** cels-sdl3 is an INTERFACE library (header-only from CMake's perspective). The `.c` files are listed in the consumer's `target_sources()` or added via a convenience macro. This matches the cels-ncurses pattern where the module compiles in the consumer's translation unit.

**GIT_SHALLOW TRUE:** SDL3 has a massive git history. Shallow clone saves significant fetch time and disk space.

**FIND_PACKAGE_ARGS:** On systems with SDL3 installed (e.g., `pacman -S sdl3` on Arch/CachyOS), this avoids building from source entirely. Huge time savings for iterative development.

**SDL3 CMake target names:**
- `SDL3::SDL3` -- the main library (shared or static depending on build config)
- `SDL3::SDL3-static` -- explicitly static
- `SDL3::SDL3-shared` -- explicitly shared
- `SDL3_image::SDL3_image` -- image loading library
- `SDL3_ttf::SDL3_ttf` -- TTF rendering library

**Prefer `SDL3::SDL3` (not static/shared):** Let the consumer or system decide linkage. Avoids forcing a linkage model.

### CELS Framework Linkage

The consumer project (example app) links both cels and cels-sdl3:

```cmake
# In the consumer/example CMakeLists.txt:
add_subdirectory(/home/cachy/workspaces/libs/cels cels)
add_subdirectory(/home/cachy/workspaces/libs/cels-sdl3 cels-sdl3)

add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE cels cels-sdl3)
```

## SDL3 API Patterns to Adopt

These are critical SDL3-specific patterns that differ from SDL2:

### 1. Properties-Based Configuration (SDL3 Novelty)

SDL3 introduces `SDL_PropertiesID` for extensible configuration. Use this for window creation instead of flag-based approaches:

```c
// SDL3 way -- properties-based window creation
SDL_PropertiesID props = SDL_CreateProperties();
SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "My Window");
SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 800);
SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 600);
SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
SDL_Window *window = SDL_CreateWindowWithProperties(props);
SDL_DestroyProperties(props);
```

Confidence: HIGH (this is a fundamental SDL3 API pattern documented in the SDL3 migration guide)

### 2. SDL_Renderer Creation (Paired with Window)

```c
// SDL3: renderer creation is explicit, not auto-paired
SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
// NULL = let SDL pick the best driver. Can pass "vulkan", "opengl", etc.
```

For multi-window: each window gets its own renderer. Store the pair together.

### 3. Event Loop (Same as SDL2, Minor Tweaks)

```c
SDL_Event event;
while (SDL_PollEvent(&event)) {
    switch (event.type) {
        case SDL_EVENT_QUIT:          // was SDL_QUIT in SDL2
            // ...
        case SDL_EVENT_KEY_DOWN:      // was SDL_KEYDOWN
            // ...
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: // new granular window events
            // ...
    }
}
```

Event names changed from `SDL_KEYDOWN` to `SDL_EVENT_KEY_DOWN` pattern. Window events are now top-level event types instead of sub-events.

Confidence: HIGH (well-documented SDL2->SDL3 migration change)

### 4. Boolean Returns

SDL3 functions return `bool` (from `<stdbool.h>`) instead of `int` for success/failure. Some functions return `true` on success (not 0 like SDL2).

```c
// SDL3
if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("Init failed: %s", SDL_GetError());
}
```

Confidence: HIGH (fundamental API change in SDL3)

## Alternatives Considered

| Category | Recommended | Alternative | When to Use Alternative |
|----------|-------------|-------------|-------------------------|
| Image loading | SDL3_image | stb_image (header-only) | Only if you need zero external dependencies. SDL3_image is better because it integrates with SDL_Surface/SDL_Texture natively. |
| Text rendering | SDL3_ttf | stb_truetype + custom atlas | Only for games needing pixel-perfect bitmap fonts or SDF text. SDL3_ttf is better for general text rendering, especially with HarfBuzz shaping. |
| Build system | CMake | Meson | Only if the entire project ecosystem uses Meson. CMake is required because CELS uses CMake. |
| 2D renderer | SDL_Renderer | SDL_GPU (direct) | For v2 when you need custom shaders, compute, or 3D. SDL_Renderer is backed by SDL_GPU internally in SDL3, so you get GPU acceleration anyway. |
| 2D renderer | SDL_Renderer | Raw OpenGL/Vulkan | Never for this project. SDL_Renderer abstracts the backend and works everywhere. Raw APIs add platform complexity with no benefit for 2D. |
| ECS framework | flecs (via CELS) | EnTT, custom ECS | Not applicable -- CELS uses flecs as its backend. This is fixed. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| SDL2 (any version) | Deprecated for new projects. Different API, different CMake targets, no SDL_GPU, less capable multi-window. | SDL3 |
| `SDL_CreateWindowAndRenderer()` | Convenience function that hides configuration. Does not support properties-based setup. Harder to manage multi-window. | `SDL_CreateWindowWithProperties()` + `SDL_CreateRenderer()` separately |
| `SDL_INIT_EVERYTHING` | Initializes subsystems you do not need (audio, haptics, sensors). Wastes startup time, may fail on systems without those devices. | `SDL_INIT_VIDEO \| SDL_INIT_EVENTS` (add gamepad/joystick only when needed) |
| Manual `dlopen`/`dlsym` for SDL3 | Some tutorials suggest dynamic loading. Unnecessary complexity when using CMake FetchContent or system packages. | CMake `target_link_libraries` with `SDL3::SDL3` |
| `IMG_Load()` returning `SDL_Surface*` then manual `SDL_CreateTextureFromSurface()` | SDL3_image provides direct texture loading functions. The surface round-trip is SDL2-era boilerplate. | `IMG_LoadTexture()` -- loads directly to GPU texture |
| Global/singleton window state | Breaks multi-window from the start. Extremely hard to refactor later. | Per-window state structs stored as ECS components |
| `SDL_Delay()` for frame timing | Imprecise, blocks the thread. | `SDL_GetPerformanceCounter()` / `SDL_GetPerformanceFrequency()` for delta time, or let the ECS frame loop manage timing |
| Vendored/copied SDL3 source in repo | Bloats the repo, version management nightmare. | FetchContent with `GIT_TAG` pinned to a release |

## Version Compatibility Matrix

| Package | Compatible With | Notes | Confidence |
|---------|-----------------|-------|------------|
| SDL3 3.2.x | SDL3_image 3.2.x | Companion libraries track SDL3 major.minor versioning | MEDIUM |
| SDL3 3.2.x | SDL3_ttf 3.2.x | Same versioning cadence | MEDIUM |
| SDL3 3.2.x | CMake >= 3.16 | SDL3 requires CMake 3.16+; use 3.24+ for FetchContent features | HIGH |
| SDL3 3.2.x | flecs v4.x | No direct dependency; connected through CELS framework | HIGH |
| SDL3 3.2.x | C99 | SDL3 public headers are C89/C99 compatible | HIGH |
| SDL3_ttf 3.2.x | FreeType 2.x, HarfBuzz | Bundled/fetched by SDL3_ttf's own CMake; you do not manage these | HIGH |
| SDL3_image 3.2.x | libpng, libjpeg, libwebp | Optional backends; SDL3_image bundles minimal decoders or uses system libs | HIGH |

## SDL3 Subsystem Map (What We Need)

| SDL3 Subsystem | Init Flag | Needed for v1 | Notes |
|----------------|-----------|----------------|-------|
| Video | `SDL_INIT_VIDEO` | YES | Windows, renderers, OpenGL/Vulkan context |
| Events | `SDL_INIT_EVENTS` | YES (implicit with VIDEO) | Event queue, polling |
| Gamepad | `SDL_INIT_GAMEPAD` | YES (for input provider) | USB/Bluetooth gamepads |
| Joystick | `SDL_INIT_JOYSTICK` | MAYBE | Low-level; gamepad subsystem is higher-level |
| Audio | `SDL_INIT_AUDIO` | NO | Out of scope for v1 |
| Haptic | `SDL_INIT_HAPTIC` | NO | Force feedback -- future milestone |
| Sensor | `SDL_INIT_SENSOR` | NO | Accelerometer/gyro -- mobile only |
| Camera | `SDL_INIT_CAMERA` | NO | Webcam capture -- not needed |

## Build Configuration Recommendations

### Debug vs Release

```cmake
# For development: enable SDL3 debug features
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # SDL3 assertion behavior
    target_compile_definitions(cels-sdl3 INTERFACE SDL_ASSERT_LEVEL=2)
endif()
```

### Sanitizers (Development)

```cmake
# Address Sanitizer -- catches buffer overflows, use-after-free
target_compile_options(my_app PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(my_app PRIVATE -fsanitize=address)
```

### CachyOS/Arch Linux System Packages

On the developer's system (CachyOS, Arch-based), SDL3 may be available via:
```
pacman -S sdl3 sdl3_image sdl3_ttf
```

If available, `FIND_PACKAGE_ARGS` in FetchContent will find these automatically, avoiding source builds. This is a significant developer experience win on Arch-based distros which tend to have cutting-edge packages.

Confidence: MEDIUM (SDL3 package availability in Arch repos needs verification -- it was entering repos in 2025 but packaging status as of March 2026 is unverified)

## Verification Checklist (For Human/Follow-up Research)

These items could not be verified with live sources during this research session. They should be confirmed before finalizing CMakeLists.txt:

- [ ] **Exact SDL3 stable version:** Check https://github.com/libsdl-org/SDL/releases for latest 3.x.y tag
- [ ] **Exact SDL3_image version:** Check https://github.com/libsdl-org/SDL_image/releases
- [ ] **Exact SDL3_ttf version:** Check https://github.com/libsdl-org/SDL_ttf/releases
- [ ] **SDL3 CMake target names:** Verify `SDL3::SDL3`, `SDL3_image::SDL3_image`, `SDL3_ttf::SDL3_ttf` are current
- [ ] **FetchContent GIT_TAG format:** Confirm whether to use `release-3.2.0` tag or `release-3.2.x` branch
- [ ] **CachyOS package availability:** Run `pacman -Ss sdl3` to check
- [ ] **SDL3_ttf HarfBuzz bundling:** Confirm whether SDL3_ttf 3.x bundles HarfBuzz or requires system install
- [ ] **`SDL_CreateWindowWithProperties` API:** Verify exact property constant names in current SDL3 headers
- [ ] **`IMG_LoadTexture` availability:** Confirm this function exists in SDL3_image (it did in SDL2_image, may have been renamed)

## Sources

All findings below are from training data (cutoff: early-to-mid 2025). Live verification was not possible during this session.

| Source | What It Informed | Confidence |
|--------|------------------|------------|
| Training data: SDL3 release announcements (Feb/Mar 2025) | SDL3 initial stable release, version numbering, API patterns | MEDIUM |
| Training data: SDL3 migration guide (wiki.libsdl.org) | API differences from SDL2, event naming, boolean returns, properties | HIGH (migration guide was well-documented) |
| Training data: SDL3 CMake integration docs | FetchContent patterns, target names | MEDIUM |
| Training data: SDL3_image / SDL3_ttf companion library docs | Companion library versioning, API surface | MEDIUM |
| PROJECT.md (this repo) | Project constraints, architecture decisions, scope | HIGH (primary source) |

---
*Stack research for: SDL3 CELS Backend Module*
*Researched: 2026-03-15*
*Note: Version numbers require live verification -- see Verification Checklist above*
