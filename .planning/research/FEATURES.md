# Feature Research

**Domain:** SDL3 CELS Backend Module
**Researched:** 2026-03-15
**Confidence:** MEDIUM-HIGH (based on SDL3 stable API knowledge, ECS backend pattern analysis, and cels-ncurses precedent; no live doc verification performed due to tool constraints)

## Context

This research covers the feature landscape for an SDL3 backend module within the CELS declarative ECS framework. The module follows the Provider Module Pattern established by cels-ncurses: an Engine module bundling Window, Input, and Renderer providers, registered via `CEL_DefineModule` and `*_use()` calls. The target is 2D application/game development using SDL_Renderer.

The competitive frame of reference is: "Why use cels-sdl3 instead of raw SDL3 C code?" The answer must be: declarative ECS ergonomics, lifecycle management, and composability -- not additional low-level features.

---

## Feature Landscape

### Table Stakes (Developers Expect These)

These are features without which the module is non-functional or unusable for its stated purpose. A developer choosing a "windowing + input + rendering + textures + text" module expects all of these.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| **Window creation and destruction** | Literally cannot display anything without it. Every SDL3 tutorial starts here. | Low | `SDL_CreateWindow` + `SDL_DestroyWindow`. Must handle `SDL_EVENT_QUIT`. |
| **Window lifecycle state machine** | Developers expect minimized/restored/resized/closed to work correctly without manual event handling. Framework promise is lifecycle management. | Medium | States: NONE -> CREATED -> SURFACE_READY -> READY -> RESIZING -> MINIMIZED -> CLOSING -> CLOSED. SDL3 events: `SDL_EVENT_WINDOW_*`. This is table stakes because the whole point of the ECS wrapper is not hand-rolling lifecycle. |
| **Frame loop integration** | A backend that does not drive `SDL_PumpEvents` + present in a loop is useless. Developers expect a main loop that ticks systems. | Medium | Must integrate with CELS system scheduling. `SDL_AppIterate`-style or manual pump. Must handle vsync/present. |
| **Keyboard input (key state snapshot)** | Every 2D app needs keyboard. Summarized "is key X pressed?" is the minimum bar. | Low | `SDL_GetKeyboardState` for snapshot. Map to CELS component. |
| **Mouse input (position + button state)** | Every 2D app with a window needs mouse. Position + buttons + scroll. | Low | `SDL_GetMouseState`, `SDL_EVENT_MOUSE_*`. |
| **Raw event queue access** | Advanced users need events the summary does not capture (text input, drag, specific timing). | Low-Medium | Buffer `SDL_Event` per frame, expose as iterable component. Without this, power users are blocked. |
| **SDL_Renderer creation and management** | The stated rendering backend. Without it, nothing draws. | Low | One renderer per window. `SDL_CreateRenderer`. |
| **Clear + Present cycle** | Developer expects "clear background, draw stuff, present" per frame. | Low | `SDL_SetRenderDrawColor` + `SDL_RenderClear` + `SDL_RenderPresent`. |
| **Texture loading from file (PNG, JPG)** | Stated scope. Developers expect `load("sprite.png")` to work. | Low | `IMG_LoadTexture` from SDL3_image. Returns `SDL_Texture*`. |
| **Texture rendering at position/size** | Loading without rendering is pointless. Basic blit with position and size. | Low | `SDL_RenderTexture` with `SDL_FRect` dest. |
| **TTF font loading** | Stated scope. Text rendering requires font loading. | Low | `TTF_OpenFont` from SDL3_ttf. |
| **Text rendering at position with color/size** | The other stated scope item. Show text on screen. | Medium | Render text to surface -> create texture -> blit. Or use SDL3_ttf's newer `TTF_CreateText` API for GPU-friendly text. Cache strategy matters for performance. |
| **Proper SDL3 init/shutdown** | `SDL_Init` with correct subsystem flags, `SDL_Quit` on teardown. Library init for SDL3_image and SDL3_ttf. | Low | Must happen once, in correct order. `IMG_Init` / `TTF_Init` + their quit counterparts. |
| **Error reporting** | When SDL3 calls fail, developers need to know why, not get silent black screens. | Low | `SDL_GetError()` surfaced through CELS logging/error component or callback. |
| **Resource cleanup on shutdown** | Textures, fonts, renderers, windows -- all must be freed. Leaking GPU resources is unacceptable. | Medium | Requires tracking all allocated resources. Destroy in correct order (textures before renderer, renderer before window). |

### Differentiators (Competitive Advantage Over Raw SDL3)

These features are why a developer would choose cels-sdl3 over writing raw SDL3 code. They represent the ECS framework value proposition.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Declarative window configuration** | Define window properties as components (`WindowConfig { .title, .width, .height, .flags }`), system creates it. No manual `SDL_CreateWindow` call. Change components = window updates reactively. | Medium | This IS the CELS value prop. Without it, why use the framework? |
| **Multi-window as entities** | Each window is an entity with Window + Renderer components. Spawn a second window = spawn an entity. Raw SDL3 requires manual bookkeeping of window/renderer pairs. | Medium | SDL3 natively supports multi-window, but managing the mapping is tedious. ECS makes it natural: one entity per window. |
| **Input as components, not event polling** | `KeyboardState`, `MouseState`, `GamepadState` as queryable ECS components. Systems just query `Query<KeyboardState>` instead of writing event loops. | Medium | Massive DX improvement. Raw SDL3 requires manual event switch statements. This is the #1 selling point for input. |
| **Automatic resource lifecycle** | Textures and fonts tied to entity lifecycle. Remove entity = resources freed. No manual `SDL_DestroyTexture` calls. | Medium-High | Requires cleanup systems that watch for removed components. High value: resource leaks are a top SDL pitfall. |
| **Provider-swappable renderer** | Feature/Provider pattern means renderer implementation can be swapped (SDL_Renderer today, SDL_GPU tomorrow) without changing user systems. | Low (design) / High (full impl) | v1 only needs SDL_Renderer provider. But the architecture must support future swap. Design cost is low; just use the existing CELS Feature/Provider pattern. |
| **Gamepad hot-plug handling** | SDL3 has excellent gamepad hot-plug support. Wrapping it as "gamepad entity appears/disappears" is a natural ECS pattern that raw SDL3 requires manual bookkeeping for. | Medium | `SDL_EVENT_GAMEPAD_ADDED` / `SDL_EVENT_GAMEPAD_REMOVED`. Map to entities. |
| **Touch input normalization** | Touch as first-class input component, not an afterthought. Consistent with keyboard/mouse component patterns. | Medium | `SDL_EVENT_FINGER_*`. Normalize coordinates. Multiple touch points as sub-components or array. |
| **Composable module registration** | `SDL3_Engine_use()` bundles everything. But individual providers (`SDL3_Window_use()`, `SDL3_Input_use()`) can be registered a la carte for custom setups. | Low | Follows cels-ncurses pattern. Low effort, high flexibility. |
| **Draw command buffering / render queue** | Instead of immediate-mode draw calls, systems emit draw commands (DrawSprite, DrawText, DrawRect) as components. A render system collects and z-sorts them. | Medium-High | Decouples game logic from render order. Enables z-ordering, layer management, batching. More advanced but very valuable. |
| **Frame-scoped input events** | Events tagged with frame number so systems processing in any order see consistent input state for that frame. No "event already consumed" bugs. | Low-Medium | Input system snapshots once per frame, all systems read same snapshot. Standard ECS pattern but must be explicitly designed. |

### Anti-Features (Deliberately NOT Building in v1)

These are commonly requested, often found in similar frameworks, but problematic for v1 scope. Deferring them is a deliberate, correct decision.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| **Audio playback/mixing** | Games need sound. Developers will ask immediately. | SDL3's audio API is completely redesigned (stream-based). It is a full subsystem with its own lifecycle, mixing model, and device management. Bundling it doubles module scope. | Defer to separate `cels-sdl3-audio` milestone. Audio has zero coupling to window/input/render lifecycle. |
| **Sprite sheet slicing / animation** | Every 2D game uses sprite sheets. | Requires an asset definition format (JSON? custom?), frame timing system, animation state machine. This is game engine territory, not backend module territory. | v1 provides `SDL_RenderTexture` with source rect parameter. Users can slice manually. Animation system is a higher-level library concern. |
| **Asset pipeline (caching, atlas packing, hot-reload)** | Production games need asset management. | Enormous scope. Atlas packing alone is a complex algorithm. Hot-reload requires file watchers. Caching requires lifetime management. Each is a project unto itself. | v1 loads files directly. Asset pipeline is a separate `cels-assets` module. |
| **Physics integration** | 2D games often need collision/physics. | Not an SDL3 concern at all. Physics is an independent system. Coupling it to the SDL3 backend is an architectural mistake. | Separate `cels-physics` module using Box2D or Chipmunk2D. |
| **UI layout / widget system** | Apps need buttons, text fields, panels. | Massive scope. This is what Clay (already in the workspace) does. Duplicating it is wasteful. | Integrate with `libs/clay` for UI. cels-sdl3 provides the render surface; Clay provides layout. |
| **Scene graph / transform hierarchy** | Game engines provide parent-child transforms. | Adds architectural weight. ECS purists argue against transform hierarchies (use flat transforms with parent reference instead). | Users compose parent-child relationships with CELS entities and a user-space transform propagation system. |
| **SDL_GPU renderer backend** | SDL_GPU is the modern path with shaders, compute, etc. | SDL_GPU has a significantly more complex API (pipeline state objects, shader compilation, buffer management). v1 needs to ship, not architect a full GPU abstraction. | Design the Provider interface so SDL_GPU can slot in later. Ship with SDL_Renderer, iterate. |
| **Cross-backend abstraction (shared interface with cels-ncurses)** | Nice to swap backends. | Premature abstraction. The two backends have fundamentally different capabilities (TUI vs graphical). A shared interface would be the lowest common denominator of both, which is useless. | Each module exposes its own component types. If a cross-backend layer emerges from real usage patterns later, great. |
| **Networking** | Multiplayer games need networking. | Not SDL3's concern (SDL_net is minimal and not recommended for games). Networking is a completely independent subsystem. | Separate concern entirely. Use ENet, GameNetworkingSockets, or similar. |
| **Tiled/LDtk map loading** | Common for 2D games. | Asset format concern, not backend concern. Loading Tiled maps requires JSON/XML parsing, tileset management, layer rendering. | User-space or separate module. cels-sdl3 provides texture rendering; map systems sit on top. |
| **Built-in camera system** | 2D games need viewport management, scrolling, zoom. | Conceptually simple but interactions with multi-window, coordinate spaces, and different game types make a "one size fits all" camera premature. | Provide coordinate transform utilities or examples. Let the camera be a user-space system operating on a viewport component. |
| **ImGui / debug overlay integration** | Developers want debug tools during development. | SDL3 ImGui integration exists but adds a dependency and rendering complexity (mixing ImGui's GL/GPU context with SDL_Renderer). | Document how to integrate ImGui separately. Do not bake it in. |

---

## Feature Dependencies

```
SDL3 Init/Shutdown
    |
    +-- Window Creation
    |       |
    |       +-- Window Lifecycle State Machine
    |       |       |
    |       |       +-- Frame Loop Integration
    |       |
    |       +-- Renderer Creation (requires Window)
    |               |
    |               +-- Clear + Present Cycle
    |               |
    |               +-- Texture Loading (requires Renderer for SDL_CreateTextureFromSurface)
    |               |       |
    |               |       +-- Texture Rendering
    |               |
    |               +-- Font Loading (independent of Renderer)
    |                       |
    |                       +-- Text Rendering (requires Renderer + Font)
    |
    +-- Input System (independent of Window in SDL3, but practically needs event pump)
            |
            +-- Keyboard State Snapshot
            +-- Mouse State Snapshot
            +-- Gamepad Discovery + State
            +-- Touch Input
            +-- Raw Event Queue

Resource Cleanup (depends on all allocated resources being tracked)

Module Registration (wraps all of the above)
    |
    +-- SDL3_Engine_use() = bundles all providers
    +-- Individual *_use() = a la carte
```

Key dependency insights:
- **Renderer depends on Window.** Cannot create SDL_Renderer without SDL_Window. Multi-window means one renderer per window entity.
- **Texture loading depends on Renderer.** `IMG_LoadTexture` takes `SDL_Renderer*`. This means textures are tied to a specific renderer/window.
- **Text rendering depends on both Font and Renderer.** Font is loaded independently, but rendering text to a texture requires the renderer.
- **Input is logically independent of rendering** but practically requires the SDL event pump running (which happens in the frame loop). Input and rendering share the event pump but not each other.
- **Frame loop is the integration point.** It pumps events (feeding Input), then runs user systems, then presents (Renderer). Everything else hangs off this.

---

## MVP Definition

### Launch With (v1)

The "colored window opens, handles input to close, can render text and textures" bar from PROJECT.md, expanded to full table stakes:

1. **SDL3 init/shutdown** -- subsystem flags, library init, proper teardown order
2. **Window provider** -- create, lifecycle state machine (full state chain), multi-window as entities, frame loop with event pump + present
3. **Input provider** -- keyboard state snapshot, mouse state snapshot (position + buttons + scroll), raw event queue per frame
4. **Renderer provider** -- SDL_Renderer per window, clear/present cycle, basic draw primitives (filled rect, line -- they are trivial and useful for debugging)
5. **Texture loading** -- PNG/JPG via SDL3_image, load from file path, create SDL_Texture
6. **Texture rendering** -- blit at position/size, with source rect (enables manual sprite sheet slicing), with optional rotation and flip
7. **Font loading** -- TTF via SDL3_ttf, load from file path at point size
8. **Text rendering** -- render string at position with color, basic caching (do not re-render unchanged text every frame)
9. **Error reporting** -- SDL_GetError surfaced on failures
10. **Resource cleanup** -- destroy all allocated SDL resources in correct order on shutdown
11. **Module registration** -- `SDL3_Engine_use()` bundles all providers, individual `*_use()` functions available
12. **Example app** -- demonstrates all of the above working together

### Add After Validation (v1.x)

Features that become important once real usage validates the architecture:

1. **Gamepad input** -- discovery, hot-plug as entity spawn/despawn, axis/button state. Deferred from v1 launch because it requires a gamepad to test and does not block the example app.
2. **Touch input** -- multi-touch point tracking, normalized coordinates. Deferred because primary target is desktop Linux.
3. **Draw command buffering** -- systems emit DrawSprite/DrawText/DrawRect commands, render system z-sorts and batches. Deferred because immediate-mode rendering is simpler for v1 and the architecture can be migrated.
4. **Texture source rect in component** -- define sprite region as component data for cleaner sprite sheet usage (stepping stone toward animation).
5. **Window configuration reactivity** -- changing WindowConfig component properties (title, size, fullscreen) at runtime triggers SDL3 calls automatically. v1 may only apply config at creation time.
6. **Render target support** -- render to texture (for post-processing, minimap, etc). `SDL_SetRenderTarget`.
7. **Color modulation / alpha blending** -- `SDL_SetTextureColorMod`, `SDL_SetTextureAlphaMod`, blend modes. Trivial SDL3 calls but need component design.
8. **Clipboard access** -- `SDL_GetClipboardText` / `SDL_SetClipboardText`. Simple but useful for apps.
9. **High-DPI / display scaling** -- SDL3 handles this better than SDL2, but multi-monitor DPI-aware rendering needs testing.

### Future Consideration (v2+)

1. **SDL_GPU renderer provider** -- modern GPU-accelerated rendering with shaders, compute. Major effort, requires pipeline/shader management.
2. **Audio module** -- separate milestone, separate module (`cels-sdl3-audio`). SDL3's stream-based audio API.
3. **Asset pipeline** -- caching, atlas packing, hot-reload. Separate module (`cels-assets`).
4. **Animation system** -- sprite sheet definition, frame sequencing, playback state machine. Higher-level module.
5. **Cross-backend abstraction** -- shared component interfaces between cels-ncurses and cels-sdl3. Only if real usage demands it.
6. **Camera / viewport system** -- zoom, pan, follow target. Could be a user-space CELS system rather than baked into the module.
7. **Particle system** -- common in 2D games, but a higher-level system that uses the renderer, not part of the renderer itself.

---

## Feature Prioritization Matrix

Priority is determined by: (1) Is it a hard dependency for other features? (2) Does it block the example app? (3) How complex is it?

| Feature | Dependency Rank | Blocks Example | Complexity | Priority |
|---------|----------------|----------------|------------|----------|
| SDL3 init/shutdown | Root dependency | Yes | Low | **P0** |
| Window creation + lifecycle | High (renderer depends on it) | Yes | Medium | **P0** |
| Frame loop integration | High (everything runs in it) | Yes | Medium | **P0** |
| Renderer creation + clear/present | High (all drawing depends on it) | Yes | Low | **P0** |
| Keyboard input snapshot | Medium (example needs close-on-ESC) | Yes | Low | **P0** |
| Mouse input snapshot | Low (example may not need it) | No | Low | **P1** |
| Raw event queue | Medium (SDL_QUIT handling) | Yes (for quit) | Low-Medium | **P0** |
| Texture loading | Medium (example shows texture) | Yes | Low | **P1** |
| Texture rendering | Medium (example shows texture) | Yes | Low | **P1** |
| Font loading | Medium (example shows text) | Yes | Low | **P1** |
| Text rendering | Medium (example shows text) | Yes | Medium | **P1** |
| Resource cleanup | Required for correctness | No (but must ship) | Medium | **P1** |
| Module registration (`*_use()`) | Required for CELS integration | Yes | Low | **P0** |
| Error reporting | Required for debuggability | No | Low | **P1** |
| Multi-window support | Architectural decision | No | Medium | **P1** (design P0, impl P1) |
| Gamepad input | Independent | No | Medium | **P2** (v1.x) |
| Touch input | Independent | No | Medium | **P2** (v1.x) |
| Draw command buffering | Architectural enhancement | No | Medium-High | **P2** (v1.x) |
| Window config reactivity | Enhancement | No | Medium | **P2** (v1.x) |

### Recommended Build Order

Based on the dependency graph and priority matrix:

**Phase 1 (Foundation):** SDL3 init/shutdown, Window creation + lifecycle state machine, Renderer creation, Frame loop (pump + clear + present). Milestone: colored window that responds to close.

**Phase 2 (Input):** Keyboard state snapshot, Mouse state snapshot, Raw event queue. Milestone: window closes on ESC, mouse position displayed in title bar.

**Phase 3 (Content):** Texture loading + rendering, Font loading + Text rendering, Basic draw primitives. Milestone: textured sprite and "Hello World" text rendered.

**Phase 4 (Integration):** Module registration (`SDL3_Engine_use()`), Resource cleanup systems, Error reporting, Multi-window support. Milestone: complete example app, clean shutdown.

**Phase 5 (Polish -- v1.x):** Gamepad, touch, draw command buffering, window config reactivity, render targets, color modulation.

---

## Key Observations for Roadmap

1. **The dependency chain is strictly linear for the core path.** Init -> Window -> Renderer -> Content. This means phases cannot be parallelized much; each builds on the previous. Plan accordingly.

2. **Multi-window is an architectural decision, not a feature toggle.** If you design for single-window and retrofit multi-window later, you will rewrite the renderer/window coupling. Design for multi-window from day one (one entity = one window/renderer pair) even if the example app only creates one window.

3. **Text rendering has a hidden performance trap.** Naive "render text to surface, create texture, blit, destroy" every frame is extremely slow. Must cache rendered text textures and only re-render when text/font/color changes. SDL3_ttf's `TTF_CreateText` + `TTF_DrawRendererText` API (new in SDL3_ttf 3.x) is the modern approach and avoids this trap entirely.

4. **SDL3's texture ownership model matters.** `SDL_Texture` is tied to a specific `SDL_Renderer`. In multi-window setups, a texture loaded for Window A cannot be used with Window B's renderer. The resource management system must track this association.

5. **The Provider pattern is the key differentiator.** Without it, cels-sdl3 is just "SDL3 with extra steps." The Feature/Provider model (where rendering is a Feature and SDL_Renderer is one Provider implementation) is what makes the module architecturally valuable and future-proof for SDL_GPU.

## Sources and Confidence Notes

- **SDL3 API:** Based on SDL 3.x stable API knowledge (SDL3 reached stable release in early 2025). The API surface for window, renderer, events, and input is well-established. **MEDIUM-HIGH confidence** -- the API is stable but specific function signatures should be verified against current SDL3 headers during implementation.
- **SDL3_image / SDL3_ttf:** Companion libraries updated for SDL3. `IMG_LoadTexture` and TTF APIs are stable. **MEDIUM confidence** -- SDL3_ttf introduced new text rendering APIs (`TTF_CreateText`) that should be verified.
- **CELS framework patterns:** Based on PROJECT.md description of cels-ncurses patterns. **MEDIUM confidence** -- actual cels-ncurses source was not inspected; patterns inferred from project documentation.
- **ECS backend patterns:** Based on general knowledge of ECS framework backends (Bevy, Flecs, EnTT usage patterns). **HIGH confidence** -- these patterns are well-established in the ECS ecosystem.
- **Feature categorization:** Based on analysis of what 2D game framework backends typically provide and what developers expect. **HIGH confidence** -- table stakes are genuinely table stakes; differentiators are genuine value adds.

---
*Feature research for: SDL3 CELS Backend Module*
*Researched: 2026-03-15*
