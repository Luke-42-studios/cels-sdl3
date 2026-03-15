# cels-sdl3

## What This Is

An SDL3 backend module for the CELS declarative ECS framework. Follows the same Provider Module Pattern as cels-ncurses — an Engine module bundling Window, Input, and Renderer providers — but targeting SDL3 for 2D hardware-accelerated rendering, texture loading, text rendering, and multi-device input. Designed as a CMake INTERFACE library that compiles in the consumer's translation unit.

## Core Value

Developers can build 2D graphical applications using the CELS framework with SDL3 as the backend, using the same `CEL_Build` / `CEL_DefineModule` / Feature/Provider patterns they already know from cels-ncurses.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] SDL3 Window provider with lifecycle state machine, frame loop, and multi-window support
- [ ] SDL3 Input provider supporting keyboard, mouse, gamepad, and touch — summarized state snapshot + raw event queue
- [ ] SDL3 Renderer provider using SDL_Renderer (2D) integrated with CELS Feature/Provider model
- [ ] Texture loading — load PNG/JPG into SDL_Texture, render at position/size
- [ ] Basic text rendering — load TTF fonts via SDL_ttf, render text at position with color/size
- [ ] Engine module that bundles all providers via `SDL3_Engine_use()` with `CEL_DefineModule`
- [ ] Example app — window opens, clears to a color, handles input to close, renders text

### Out of Scope

- Audio (playback, mixing, recording) — planned as its own future milestone
- SDL_GPU renderer backend — module designed for future swap-in, but v1 targets SDL_Renderer only
- Cross-backend abstraction layer — no shared CELS_Window/CELS_Input interface with cels-ncurses for now
- Sprite sheet slicing / frame animation — v1 is load-and-render only
- Asset pipeline (caching, atlas packing, hot-reload) — future milestone
- 3D rendering — out of scope entirely for this module

## Context

- **Sibling module:** cels-ncurses is the existing CELS backend module. cels-sdl3 follows the same architectural patterns: Provider Module Pattern, INTERFACE CMake library, `*_use()` registration, Feature/Provider rendering, `CEL_DefineModule` bundling.
- **CELS framework:** Located at `/home/cachy/workspaces/libs/cels/`. Provides component system, state management, reactivity, lifecycle control, feature/provider model, module system. C++ runtime with C99 API via `extern "C"`.
- **SDL3:** The latest major version of SDL. Uses a new GPU-abstracted renderer, revamped audio API, improved gamepad support, and properties-based configuration. Major API changes from SDL2.
- **Naming convention:** cels-ncurses uses `tui_` prefix for all symbols. cels-sdl3 should use `sdl3_` or `SDL3_` prefix following the same PascalCase/snake_case conventions.
- **WindowState state machine:** cels-ncurses defined a Vulkan-aligned state machine (`NONE -> CREATED -> SURFACE_READY -> READY -> RESIZING -> MINIMIZED -> CLOSING -> CLOSED`) but fast-tracked for TUI. SDL3 will use the full state chain.
- **Multi-window:** Supported from v1. Each window gets its own SDL_Window + SDL_Renderer pair.

## Constraints

- **Tech stack**: C99 source, CMake INTERFACE library, SDL3 (not SDL2) — matches cels-ncurses patterns
- **Dependency**: Must link against CELS framework (`/home/cachy/workspaces/libs/cels/`) via CMake
- **SDL3 dependency**: Fetched via CMake FetchContent or system package — SDL3, SDL3_image (for PNG/JPG), SDL3_ttf (for text)
- **Platform**: Linux primary (matching cels-ncurses), but SDL3 is cross-platform so avoid POSIX-only APIs where SDL3 provides alternatives
- **Rendering**: SDL_Renderer (2D) for v1 — architecture should allow SDL_GPU provider to replace it later

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| SDL_Renderer over SDL_GPU for v1 | Simpler API, sufficient for 2D, SDL_GPU can slot in later as alternative provider | — Pending |
| Multi-window from day one | SDL3 supports it natively, avoids single-window assumptions baked into architecture | — Pending |
| Self-contained module (no cross-backend abstraction) | Keeps v1 scope manageable, cross-backend layer is a separate future effort | — Pending |
| SDL3_image + SDL3_ttf as dependencies | Standard SDL3 companion libraries for image loading and text rendering | — Pending |
| Input: summarized state + event queue | Summarized snapshot for simple use, raw queue for advanced handling | — Pending |

---
*Last updated: 2026-03-15 after initialization*
