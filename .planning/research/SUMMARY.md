# Project Research Summary

**Project:** cels-sdl3
**Domain:** SDL3 CELS Backend Module
**Researched:** 2026-03-15
**Confidence:** MEDIUM

## Executive Summary

cels-sdl3 is an ECS backend module that wraps SDL3 windowing, input, and 2D rendering into the CELS declarative framework using the Provider Module Pattern established by cels-ncurses. The project follows a well-understood architectural pattern: an Engine facade module bundles Window, Input, and Renderer providers, each registered via `CEL_DefineModule` and `*_use()` calls. The core value proposition is not "SDL3 with extra steps" but declarative ECS ergonomics -- windows as entities, input as queryable components, automatic resource lifecycle, and provider-swappable rendering. SDL3 itself reached stable release in early 2025 and provides a cleaner API than SDL2, with properties-based configuration, first-class multi-window support, SDL_GPU-backed rendering, and consistent naming conventions.

The recommended approach is to build incrementally along the strict dependency chain: SDL3 init, then window creation with lifecycle state machine, then input summarization as ECS components, then SDL_Renderer-based rendering, and finally texture/font loading via SDL3_image and SDL3_ttf. Multi-window support must be designed from day one (one entity = one window/renderer pair) even though the example app only uses one window. The CELS Feature/Provider model should be used for rendering dispatch so SDL_GPU can replace SDL_Renderer in a future version without changing consumer code.

The dominant risk is SDL2 muscle memory contaminating SDL3 code -- event names, return types, coordinate types, and window creation patterns all changed significantly. Phase 1 (Window Provider) carries the highest pitfall density: 9 of 15 identified pitfalls must be addressed there, including event loop ownership, resource destruction ordering, multi-window data model, and SDL3's properties API. The secondary risk is SDL3_ttf's poorly documented API changes, which require a mini-research spike before implementation. All SDL3 version numbers in this research need live verification against current release tags before writing CMakeLists.txt.

## Key Findings

### Recommended Stack

The stack is SDL3-native with CMake FetchContent for dependency management. All companion libraries track SDL3's major.minor versioning cadence.

**Core technologies:**
- **SDL3 3.2.x**: Windowing, input, 2D rendering, event loop -- the project requirement; SDL3 is the current major version with revamped API
- **SDL3_image 3.2.x**: PNG/JPG/WebP texture loading -- standard SDL3 companion, integrates natively with SDL_Surface/SDL_Texture
- **SDL3_ttf 3.2.x**: TrueType/OpenType font rendering with HarfBuzz shaping -- only sane option for SDL3 text rendering
- **CMake >= 3.24**: Build system with FetchContent, tries system packages first via `FIND_PACKAGE_ARGS`, falls back to source builds
- **C99**: Source language matching CELS conventions; SDL3 public API is C-compatible

**Key CMake detail:** cels-sdl3 is an INTERFACE library. Source files compile in the consumer's translation unit, matching the cels-ncurses pattern. `GIT_SHALLOW TRUE` is essential for SDL3's massive git history.

### Expected Features

**Must have (table stakes):**
- SDL3 init/shutdown with correct subsystem flags and library init ordering
- Window creation with full lifecycle state machine (NONE through CLOSED)
- Frame loop integration: event pump, ECS tick, present cycle
- Keyboard and mouse input as summarized ECS components (not raw event polling)
- Raw event queue access for advanced consumers
- SDL_Renderer per window with clear/present cycle
- Texture loading (PNG/JPG via SDL3_image) and rendering at position/size
- Font loading (TTF via SDL3_ttf) and text rendering with basic caching
- Resource cleanup in correct destruction order
- Module registration via `SDL3_Engine_use()` and individual `*_use()` functions
- Error reporting surfacing `SDL_GetError()`
- Example application demonstrating all features

**Should have (differentiators -- the reason to use cels-sdl3 over raw SDL3):**
- Declarative window configuration via components (change components = window updates)
- Multi-window as entities (spawn entity = spawn window)
- Input as queryable ECS components, not manual event switch statements
- Automatic resource lifecycle tied to entity lifecycle
- Provider-swappable renderer (SDL_Renderer now, SDL_GPU later)
- Composable a la carte module registration
- Frame-scoped input snapshots (all systems see consistent input per frame)

**Defer (v2+):**
- Audio (completely separate subsystem, zero coupling to window/input/render)
- SDL_GPU renderer backend (significant API complexity increase)
- Asset pipeline (caching, atlas packing, hot-reload)
- Animation system, physics, UI layout, scene graph
- Cross-backend abstraction with cels-ncurses (premature; fundamentally different capabilities)

### Architecture Approach

The architecture follows a layered provider model: SDL3_Engine is the facade module that registers three providers (SDL3_Window, SDL3_Input, SDL3_Renderer) in dependency order. SDL3_Window drives the outer frame loop (poll all events, then ECS tick, then present per window). SDL3_Input drains events forwarded from the window provider and writes summarized state to an ECS singleton. SDL3_Renderer creates one SDL_Renderer per window entity and dispatches draw calls during PostUpdate. Textures and fonts are resource components managed through SDL3_Texture and SDL3_Text sub-modules.

**Major components:**
1. **SDL3_Engine** -- Module facade; SDL_Init/SDL_Quit lifecycle, provider registration in dependency order
2. **SDL3_Window** -- Window creation/destruction, WindowState FSM, frame loop (poll events -> ECS tick -> present), multi-window entity management
3. **SDL3_Input** -- Event drain and summarization, keyboard/mouse/gamepad state as ECS singleton, raw event ring buffer
4. **SDL3_Renderer** -- SDL_Renderer per window, clear/present cycle, draw primitives, Feature/Provider dispatch for sprites and labels
5. **SDL3_Texture** -- SDL3_image loading, SDL_Texture lifecycle management, renderer-affinity tracking
6. **SDL3_Text** -- SDL3_ttf font loading, text-to-texture rendering with caching

**Critical architectural decisions:**
- Single event drain point (SDL3_Window) -- never poll events from multiple systems
- Per-entity windows with paired renderer components -- never global/singleton state
- CELS frame loop is the outer driver; SDL3 is the backend within it
- Texture creation requires a renderer, creating a hard dependency chain: init -> window -> renderer -> textures/text

### Critical Pitfalls

1. **SDL2 API patterns in SDL3 code** -- Hundreds of functions renamed, return types changed (int to bool), event constants restructured, coordinate types changed to float. Prevention: always check SDL3 headers, never code from memory. Use `#include <SDL3/SDL.h>`, enable `-Werror`.

2. **Renderer-window pairing broken in multi-window** -- SDL_Renderer is bound to exactly one SDL_Window. Global renderer state silently renders to wrong window or crashes. Prevention: store SDL_Window + SDL_Renderer as paired components on each window entity. Textures must track which renderer they belong to.

3. **Event loop ownership conflict** -- SDL_PollEvent must be called on the main thread, once per frame, from a single drain point. Multiple drain points cause events to split non-deterministically. Prevention: one SDL3_PollEvents system at frame start, pinned to main pipeline phase. Always poll even when minimized.

4. **Resource destruction in wrong order** -- ECS deferred destruction fires component hooks in implementation-defined order, but SDL requires: textures first, then renderers, then windows, then SDL_Quit. Prevention: explicit shutdown system with ordered destruction, or make textures child entities of windows (flecs destroys children first).

5. **SDL3_ttf API changes** -- SDL3_ttf likely has a new text engine API (TTF_CreateText/TTF_DrawRendererText) replacing the SDL2-era surface-based workflow. This is the least documented dependency. Prevention: read SDL3_ttf headers before writing any text code; prototype "Hello World" before designing the component system.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Foundation (SDL3_Engine + SDL3_Window)

**Rationale:** Everything depends on a working window. The dependency chain is strictly linear: init -> window -> renderer -> content. This phase also carries the highest pitfall density (9 of 15 pitfalls). Investing extra time here prevents cascading issues in all subsequent phases.
**Delivers:** A colored window that opens, handles lifecycle events (minimize, resize, close), and runs a frame loop integrated with CELS. Module scaffold with `SDL3_Engine_use()`.
**Addresses:** SDL3 init/shutdown, window creation + lifecycle state machine, frame loop integration, multi-window entity data model (designed but single window tested).
**Avoids:** Pitfalls #1 (SDL2 vs SDL3 API), #2 (renderer-window pairing design), #3 (event loop ownership), #4 (properties API), #5 (destruction order), #6 (event type changes), #9 (multi-window data model), #12 (high-DPI size tracking), #15 (init vs window creation separation).

### Phase 2: Input

**Rationale:** Input depends only on Phase 1 (needs event pump running) and is needed to interact with the window for testing everything else. Logically independent of rendering.
**Delivers:** Keyboard and mouse state as queryable ECS components. Raw event queue. Press Escape to close window. Mouse position readable by consumer systems.
**Addresses:** Keyboard state snapshot, mouse state snapshot (position + buttons + scroll), raw event queue per frame, frame-scoped input consistency.
**Avoids:** Pitfalls #6 (event type mapping for input events), #9 (per-window input state routing by windowID), #13 (gamepad hot-plug design, even if gamepad deferred to v1.x).

### Phase 3: Rendering

**Rationale:** Depends on window (Phase 1). Needed before textures/text. This is where the Feature/Provider model gets established, which is the key architectural differentiator.
**Delivers:** SDL_Renderer per window, clear/present cycle, basic draw primitives (filled rect, line), Feature/Provider scaffold (CEL_DefineFeature, CEL_Provides).
**Addresses:** Renderer creation + clear/present, draw primitives, Feature/Provider rendering model, float-based coordinate components.
**Avoids:** Pitfalls #2 (renderer binding enforcement), #11 (float coordinates from day one), #14 (flat positioning, no scene graph).

### Phase 4: Resources (Textures + Text)

**Rationale:** Depends on renderer (Phase 3). These are the highest-level features. Text rendering specifically requires a mini-research spike due to SDL3_ttf API uncertainty.
**Delivers:** Image loading and rendering via SDL3_image, font loading and text rendering via SDL3_ttf, texture lifecycle management, SDL3_Sprite and SDL3_Label features.
**Addresses:** Texture loading (PNG/JPG), texture rendering at position/size with source rect, font loading, text rendering with caching, resource-to-renderer affinity tracking.
**Avoids:** Pitfalls #7 (sync loading acceptable for v1 but API designed for future async), #8 (SDL3_ttf API changes -- requires header investigation before implementation).

### Phase 5: Integration and Example

**Rationale:** All components exist; this phase assembles them, tests edge cases, and produces the demo that validates the architecture.
**Delivers:** Complete `SDL3_Engine_use()` module registration, resource cleanup systems, error reporting, multi-window explicit testing, example application demonstrating all features.
**Addresses:** Module registration, resource cleanup ordering, error reporting, multi-window validation, example app.
**Avoids:** All pitfalls surface here during integration testing. Specifically validates the "Looks Done But Isn't" checklist from pitfalls research.

### Phase Ordering Rationale

- **Strictly linear dependency chain:** Init -> Window -> Renderer -> Content. Phases cannot be parallelized; each builds on the previous. The architecture research and feature dependency graph both confirm this.
- **Pitfall-weighted front-loading:** Phase 1 addresses 9 pitfalls because the foundation decisions (event loop ownership, multi-window data model, destruction ordering) propagate to every subsequent phase. Getting Phase 1 right is more important than getting it done fast.
- **Input before rendering:** Input depends only on the event pump (Phase 1) and is needed to test window interactions. Rendering depends on the window. Separating them keeps phases focused and testable.
- **Text deferred to Phase 4:** SDL3_ttf has the highest API uncertainty. By the time Phase 4 starts, the team will have SDL3 muscle memory from Phases 1-3, making the SDL3_ttf research spike more productive.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 1:** Needs `/gsd:research-phase` to verify exact SDL3 API signatures for window creation, properties API constant names, and event type constants against installed SDL3 headers. Also needs live verification of CMake FetchContent GIT_TAG format and CachyOS package availability.
- **Phase 4 (Text):** Needs `/gsd:research-phase` to investigate SDL3_ttf's new text engine API. The SDL2_ttf workflow may be completely replaced. Read headers before designing the component system.

Phases with standard patterns (skip research-phase):
- **Phase 2 (Input):** Well-documented ECS input patterns. SDL3 event constants need header verification but the architecture is standard.
- **Phase 3 (Rendering):** SDL_Renderer API is well-established. Feature/Provider pattern follows existing CELS conventions from cels-ncurses.
- **Phase 5 (Integration):** Assembly and testing, not new patterns.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | MEDIUM | Library choices are HIGH confidence; exact version numbers need live verification against GitHub releases. CachyOS SDL3 package availability unverified. |
| Features | MEDIUM-HIGH | Table stakes and differentiators are well-established ECS backend patterns. Feature prioritization matrix is solid. SDL3_ttf text API specifics are LOW-MEDIUM. |
| Architecture | HIGH | Provider Module Pattern directly derived from cels-ncurses. SDL3 event/window/renderer architecture is canonical. Multi-window ECS pattern is well-established. |
| Pitfalls | MEDIUM | Pitfall patterns are HIGH confidence (universal game dev concerns). Specific SDL3 API details (function signatures, constant names) are MEDIUM and need header verification. |

**Overall confidence:** MEDIUM

The architecture and patterns are sound. The uncertainty is concentrated in SDL3 API specifics -- exact function signatures, property constant names, SDL3_ttf's new text engine API, and version numbers. These are all resolvable by reading installed SDL3 headers during Phase 1 implementation, not fundamental unknowns.

### Gaps to Address

- **SDL3 exact version tags:** Must run `pacman -Ss sdl3` and check GitHub releases before writing CMakeLists.txt. Blocks Phase 0/1.
- **SDL3_ttf text engine API:** The old `TTF_RenderText_Blended` -> surface -> texture workflow may be replaced. Must read SDL3_ttf headers before Phase 4 implementation.
- **CELS framework macros:** Exact signatures for `CEL_DefineModule`, `CEL_DefineFeature`, `CEL_Provides`, and `CEL_Feature` should be verified against current CELS headers. Inferred from PROJECT.md descriptions.
- **cels-ncurses source inspection:** Architecture patterns were derived from project documentation, not actual cels-ncurses source. Verify the Provider Module Pattern implementation matches what is described.
- **SDL3 `IMG_LoadTexture` existence:** Confirm this function exists in SDL3_image (carried over from SDL2_image but may have been renamed).
- **High-DPI behavior on CachyOS/Wayland:** SDL3's high-DPI handling needs testing on the actual development environment. Wayland compositor behavior may differ from X11.

## Sources

### Primary (HIGH confidence)
- PROJECT.md (this repo) -- project constraints, architecture decisions, scope, CELS patterns
- SDL3 migration guide (wiki.libsdl.org, via training data) -- API differences from SDL2, event naming, boolean returns, properties system
- ECS backend patterns (Bevy, flecs, EnTT ecosystem knowledge) -- standard patterns for input, rendering, resource lifecycle in ECS frameworks

### Secondary (MEDIUM confidence)
- SDL3 release announcements (Feb/Mar 2025, training data) -- version numbering, API patterns, initial stable release
- SDL3 CMake integration docs (training data) -- FetchContent patterns, target names
- SDL3_image / SDL3_ttf companion library docs (training data) -- versioning, API surface

### Tertiary (LOW confidence)
- SDL3_ttf new text engine API -- inferred from training data hints about `TTF_CreateText` / `TTF_DrawRendererText`. Needs header verification.
- CachyOS/Arch SDL3 package availability -- SDL3 was entering Arch repos in 2025; current status as of March 2026 is unverified.

---
*Research completed: 2026-03-15*
*Ready for roadmap: yes*
