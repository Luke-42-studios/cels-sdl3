# Architecture Research

**Domain:** SDL3 CELS Backend Module
**Researched:** 2026-03-15
**Confidence:** HIGH (SDL3 architecture is well-documented; CELS Provider Module Pattern is defined by cels-ncurses reference)

## System Overview

```
Consumer Application (e.g., projects/persona)
 |
 | CEL_Build(..., SDL3_Engine_use, ...)
 |
 v
+=====================================================================+
|  cels-sdl3 (CMake INTERFACE library - compiles in consumer TU)      |
|                                                                     |
|  +-------------------+                                              |
|  | SDL3_Engine       |  CEL_DefineModule(SDL3_Engine)               |
|  | (Facade Module)   |  Bundles all providers, init/shutdown order  |
|  +--------+----------+                                              |
|           |                                                         |
|           | registers providers in order:                           |
|           |                                                         |
|  +--------v----------+  +----------------+  +------------------+    |
|  | SDL3_Window       |  | SDL3_Input     |  | SDL3_Renderer    |    |
|  | (Provider)        |  | (Provider)     |  | (Provider)       |    |
|  |                   |  |                |  |                  |    |
|  | SDL_Window*       |  | SDL_PollEvent  |  | SDL_Renderer*    |    |
|  | lifecycle FSM     |  | state snapshot |  | draw commands    |    |
|  | multi-window map  |  | event queue    |  | feature/provider |    |
|  +-------------------+  +----------------+  +------------------+    |
|           |                     |                    |              |
|           |                     |           +--------+--------+    |
|           |                     |           |                 |    |
|           |                     |   +-------v------+ +--------v-+ |
|           |                     |   | SDL3_Texture | | SDL3_Text| |
|           |                     |   | (Resource)   | | (Resource| |
|           |                     |   | SDL3_image   | | SDL3_ttf)| |
|           |                     |   +--------------+ +----------+ |
+=====================================================================+
 |             |                  |                |
 v             v                  v                v
+-------------------------------------------------------------------+
| CELS Framework (/home/cachy/workspaces/libs/cels/)                |
| - Component system (ECS via flecs v4.1.4)                         |
| - State management, reactivity                                    |
| - Lifecycle control (OnLoad, PreUpdate, OnUpdate, PostUpdate)     |
| - Feature/Provider model (CEL_DefineFeature / CEL_Provides)      |
| - Module system (CEL_DefineModule)                                |
+-------------------------------------------------------------------+
 |
 v
+-------------------------------------------------------------------+
| SDL3 + SDL3_image + SDL3_ttf                                      |
| (FetchContent or system package)                                  |
+-------------------------------------------------------------------+
```

## Component Responsibilities

| Component | Responsibility | Owns | Communicates With |
|-----------|----------------|------|-------------------|
| **SDL3_Engine** | Module facade. Init SDL3 subsystems, register providers in dependency order, shutdown in reverse order. | SDL3 init/quit lifecycle | All providers (registration), CELS module system |
| **SDL3_Window** | Create/destroy SDL_Window instances. Drive window state machine. Own the frame loop (poll events, delegate, present). | `SDL_Window*` per window, WindowState FSM, window-to-renderer mapping | SDL3_Input (forwards events), SDL3_Renderer (frame begin/end), SDL3_Engine (lifecycle) |
| **SDL3_Input** | Drain SDL event queue per frame. Build summarized input state snapshot. Maintain raw event queue for advanced consumers. Register as ECS system at OnLoad phase. | Summarized `SDL3_InputState`, raw event ring buffer | SDL3_Window (receives events), consumer systems (read input state via ECS component) |
| **SDL3_Renderer** | SDL_Renderer creation per window. Clear/present cycle. Expose draw primitives. Host feature/provider rendering dispatch. | `SDL_Renderer*` per window, render command state | SDL3_Window (paired 1:1), SDL3_Texture (texture rendering), SDL3_Text (text rendering), CELS Feature/Provider model |
| **SDL3_Texture** | Load images (PNG/JPG) via SDL3_image into SDL_Texture. Manage texture lifecycle (create, destroy, cache reference). | `SDL_Texture*` handles, source rects | SDL3_Renderer (needs renderer to create textures) |
| **SDL3_Text** | Load TTF fonts via SDL3_ttf. Render text strings to textures. Manage font lifecycle. | `TTF_Font*` handles, glyph caches | SDL3_Renderer (needs renderer for texture targets) |

## Recommended Project Structure

```
cels-sdl3/
├── CMakeLists.txt                  # INTERFACE library, FetchContent for SDL3/image/ttf
├── include/
│   └── cels-sdl3/
│       ├── sdl3_engine.h           # CEL_DefineModule(SDL3_Engine), SDL3_Engine_use()
│       ├── sdl3_window.h           # Window provider, WindowState enum, lifecycle API
│       ├── sdl3_input.h            # Input provider, SDL3_InputState, event queue
│       ├── sdl3_renderer.h         # Renderer provider, draw API, feature/provider
│       ├── sdl3_texture.h          # Texture loading/management API
│       ├── sdl3_text.h             # Font loading, text rendering API
│       └── sdl3_components.h       # ECS component types (WindowState, InputState, etc.)
├── src/
│   ├── sdl3_engine.c               # Module registration, SDL_Init/SDL_Quit
│   ├── window/
│   │   └── sdl3_window.c           # Window lifecycle, state machine, frame loop
│   ├── input/
│   │   └── sdl3_input.c            # Event polling, state summarization, ECS system
│   ├── renderer/
│   │   └── sdl3_renderer.c         # SDL_Renderer lifecycle, clear/present, draw API
│   ├── texture/
│   │   └── sdl3_texture.c          # SDL3_image loading, texture management
│   └── text/
│       └── sdl3_text.c             # SDL3_ttf loading, text surface/texture creation
├── examples/
│   └── hello_sdl3/
│       ├── CMakeLists.txt
│       └── main.c                  # Window + clear + input + text rendering demo
└── .planning/
    ├── PROJECT.md
    ├── config.json
    └── research/
        └── ARCHITECTURE.md         # (this file)
```

**Rationale for structure:**
- Mirrors cels-ncurses exactly (`include/cels-sdl3/` headers, `src/` with subdirectories per provider)
- Adds `texture/` and `text/` subdirectories since SDL3 has these as separate concerns unlike ncurses
- `sdl3_components.h` centralizes all ECS component type definitions (WindowState, InputState, Position, Size, Sprite, Text, etc.)
- Example directory validates the module works end-to-end

## Architectural Patterns

### Pattern 1: SDL3 Event Loop Integration with CELS Frame Loop

**The core challenge:** SDL3 owns the event queue (SDL_PollEvent drains a global queue). CELS owns the ECS tick cycle (OnLoad, PreUpdate, OnUpdate, PostUpdate). These must be harmonized.

**Recommended pattern: Window Provider drives the outer loop, CELS systems run within it.**

```
SDL3_Window frame loop (runs per-frame):
  |
  |-- 1. SDL_PollEvent() drain ALL events
  |      |
  |      +-- Window events -> update WindowState FSM
  |      +-- Input events  -> forward to SDL3_Input provider
  |      +-- Quit event    -> set WindowState = CLOSING
  |
  |-- 2. SDL3_Input summarizes accumulated events into snapshot
  |      (writes SDL3_InputState ECS component)
  |
  |-- 3. CELS frame tick: ecs_progress(world, delta_time)
  |      |
  |      +-- OnLoad systems run (input system already populated state)
  |      +-- PreUpdate systems run (game logic reads input)
  |      +-- OnUpdate systems run (game state updates)
  |      +-- PostUpdate systems run (rendering dispatch)
  |
  |-- 4. SDL_RenderPresent() for each window
  |
  +-- Loop continues while WindowState != CLOSED
```

**Why this order:**
- Events MUST be drained before ECS tick so input state is current
- ECS tick runs all game logic including render commands
- Present happens AFTER all render commands are issued
- This matches SDL3's recommended pattern: poll, update, render, present

**Key detail for SDL3:** Unlike SDL2, SDL3 uses `SDL_PollEvent` with `SDL_Event*` (same signature) but event types are renamed (e.g., `SDL_EVENT_QUIT` not `SDL_QUIT`, `SDL_EVENT_KEY_DOWN` not `SDL_KEYDOWN`). Window events use `SDL_EVENT_WINDOW_*` subtypes. This is a namespace change, not an architectural change.

### Pattern 2: Multi-Window Management

**Architecture:** Each window is an ECS entity with associated components.

```
Entity: Window_Main
  Components:
    - SDL3_WindowHandle { SDL_Window*, SDL_Renderer*, window_id }
    - SDL3_WindowState  { state: READY, flags }
    - SDL3_WindowConfig { title, width, height, flags }

Entity: Window_Debug
  Components:
    - SDL3_WindowHandle { SDL_Window*, SDL_Renderer*, window_id }
    - SDL3_WindowState  { state: READY, flags }
    - SDL3_WindowConfig { title, width, height, flags }
```

**Event routing:** SDL3 window events contain `event.window.windowID`. The Input provider uses this ID to route events to the correct window entity. The Window provider maintains a `windowID -> ecs_entity_t` lookup map.

**Renderer pairing:** Each SDL_Window gets its own SDL_Renderer (SDL3 enforces this -- one renderer per window). The SDL3_WindowHandle component stores both pointers. Rendering systems query for `SDL3_WindowHandle` to get the renderer for a given window.

**Frame loop with multi-window:**
```
per frame:
  SDL_PollEvent() -- drains events for ALL windows
  route events by windowID to correct window entity
  ecs_progress() -- systems run once, query across all windows
  for each window entity with state == READY:
    SDL_RenderPresent(window.renderer)
```

**Critical decision: Single event drain, single ECS tick, per-window present.** Do NOT run separate ECS ticks per window. Systems that need to render to specific windows query for the window entity they target.

### Pattern 3: Resource Lifecycle (Textures, Fonts)

**Textures and fonts are ECS-managed resources.** They are created when needed, stored as components, and destroyed during cleanup.

```
Entity: Texture_PlayerSprite
  Components:
    - SDL3_TextureHandle { SDL_Texture*, width, height }
    - SDL3_TextureSource { path: "assets/player.png" }
    - (optional) SDL3_TextureOwner { window_entity }  // which renderer owns it

Entity: Font_UI
  Components:
    - SDL3_FontHandle { TTF_Font*, size }
    - SDL3_FontSource { path: "assets/font.ttf", pt_size: 16 }
```

**Texture creation requires a renderer.** This means textures cannot be loaded until a window (and its renderer) exists. The state machine enforces this: textures can only be loaded when WindowState >= SURFACE_READY.

**Font loading is renderer-independent.** TTF_Font can be opened without a renderer. But rendering text to a texture requires a renderer (for `SDL_CreateTextureFromSurface`).

**Cleanup order matters:**
1. Destroy textures (SDL_DestroyTexture)
2. Close fonts (TTF_CloseFont)
3. Destroy renderers (SDL_DestroyRenderer)
4. Destroy windows (SDL_DestroyWindow)
5. Quit subsystems (TTF_Quit, IMG_Quit, SDL_Quit)

This is the reverse of creation order, and the Engine module's shutdown must enforce it.

### Pattern 4: Feature/Provider Rendering Model

**Following CELS conventions:** Renderable entities use the Feature/Provider model.

```c
// Provider side (in sdl3_renderer.c):
CEL_DefineFeature(SDL3_Sprite);    // "I can render sprites"
CEL_DefineFeature(SDL3_Label);     // "I can render text labels"
CEL_Provides(SDL3_Renderer, SDL3_Sprite);  // SDL3_Renderer provides sprite rendering
CEL_Provides(SDL3_Renderer, SDL3_Label);   // SDL3_Renderer provides label rendering

// Consumer side (in app code):
CEL_Feature(entity, SDL3_Sprite, {
    .texture = "player.png",
    .src_rect = { 0, 0, 32, 32 },
});
```

**Rendering dispatch:** During PostUpdate phase, the renderer system queries for entities with SDL3_Sprite or SDL3_Label features and dispatches draw calls to SDL_Renderer.

### Pattern 5: WindowState State Machine

**States and transitions for SDL3:**

```
NONE ──────> CREATED ──────> SURFACE_READY ──────> READY
  |              |                |                   |
  |              |                |                   |
  |              v                v                   v
  |           (failed)      MINIMIZED <──────── RESIZING
  |              |                |                   |
  |              v                v                   |
  |           CLOSED         READY  <─────────────────+
  |              ^                                    |
  |              |                                    v
  +──────────────+──────── CLOSING <──────────────────+
                               |
                               v
                            CLOSED
```

| State | Meaning | SDL3 Trigger |
|-------|---------|--------------|
| NONE | No window exists | Initial state |
| CREATED | SDL_CreateWindow succeeded | After SDL_CreateWindow returns non-NULL |
| SURFACE_READY | SDL_Renderer created and bound | After SDL_CreateRenderer succeeds |
| READY | Window is visible and operational | After first successful clear/present cycle |
| RESIZING | Window is being resized | SDL_EVENT_WINDOW_RESIZED / SDL_EVENT_WINDOW_SIZE_CHANGED |
| MINIMIZED | Window is minimized | SDL_EVENT_WINDOW_MINIMIZED |
| CLOSING | Close requested, cleanup in progress | SDL_EVENT_WINDOW_CLOSE_REQUESTED or SDL_EVENT_QUIT |
| CLOSED | Fully destroyed | After SDL_DestroyRenderer + SDL_DestroyWindow |

**SDL3-specific note:** SDL3 uses `SDL_EVENT_WINDOW_CLOSE_REQUESTED` (not `SDL_WINDOWEVENT_CLOSE`). The event structure changed: no more `SDL_WindowEvent` subtype field. Each window event is its own top-level event type.

## Data Flow

### Per-Frame Data Flow (Detailed)

```
Time ───────────────────────────────────────────────────>

1. EVENT DRAIN (SDL3_Window drives)
   ┌─────────────────────────────────────────┐
   │ SDL_PollEvent(&event) loop              │
   │   ├─ SDL_EVENT_QUIT        -> CLOSING   │
   │   ├─ SDL_EVENT_WINDOW_*    -> FSM       │
   │   ├─ SDL_EVENT_KEY_*       -> Input     │
   │   ├─ SDL_EVENT_MOUSE_*     -> Input     │
   │   ├─ SDL_EVENT_GAMEPAD_*   -> Input     │
   │   └─ SDL_EVENT_FINGER_*    -> Input     │
   └─────────────────────────────────────────┘
                    │
                    v
2. INPUT SUMMARIZATION (SDL3_Input)
   ┌─────────────────────────────────────────┐
   │ Build SDL3_InputState from raw events:  │
   │   - keyboard: key_down[], key_pressed[] │
   │   - mouse: x, y, dx, dy, buttons       │
   │   - gamepad: axes[], buttons[]          │
   │ Write to ECS singleton component        │
   │ Append raw events to ring buffer        │
   └─────────────────────────────────────────┘
                    │
                    v
3. ECS TICK: ecs_progress(world, dt)
   ┌─────────────────────────────────────────┐
   │ OnLoad:    Input system (already done)  │
   │ PreUpdate: Consumer reads InputState    │
   │ OnUpdate:  Game logic, state changes    │
   │ PostUpdate: Render dispatch             │
   │   ├─ SDL_RenderClear(renderer)          │
   │   ├─ Query SDL3_Sprite entities -> draw │
   │   ├─ Query SDL3_Label entities -> draw  │
   │   └─ (other feature renders)            │
   └─────────────────────────────────────────┘
                    │
                    v
4. PRESENT (SDL3_Window)
   ┌─────────────────────────────────────────┐
   │ for each window with state == READY:    │
   │   SDL_RenderPresent(window.renderer)    │
   └─────────────────────────────────────────┘
```

### Input Flow Detail

```
SDL Event Queue (global, all windows)
        │
        v
SDL3_Window: SDL_PollEvent() drain
        │
        ├── window events ──> WindowState FSM update (per-window entity)
        │
        └── input events ──> SDL3_Input accumulator
                                    │
                                    v
                            SDL3_InputState (ECS singleton)
                            {
                              keys_down[SDL_SCANCODE_COUNT],    // held this frame
                              keys_pressed[SDL_SCANCODE_COUNT], // just pressed
                              keys_released[SDL_SCANCODE_COUNT],// just released
                              mouse_x, mouse_y,
                              mouse_dx, mouse_dy,
                              mouse_buttons,
                              // ... gamepad, touch
                            }
                                    │
                                    v
                            Consumer systems read via:
                            const SDL3_InputState* input =
                              ecs_singleton_get(world, SDL3_InputState);
                            if (input->keys_pressed[SDL_SCANCODE_ESCAPE]) { ... }
```

### Render Flow Detail

```
PostUpdate Phase
        │
        v
SDL3_Renderer system:
  for each window entity (SDL3_WindowHandle, SDL3_WindowState == READY):
    │
    ├── SDL_SetRenderDrawColor(renderer, bg)
    ├── SDL_RenderClear(renderer)
    │
    ├── Query: (SDL3_Sprite, Position, Size)
    │   for each match:
    │     SDL_RenderTexture(renderer, texture, &src, &dst)
    │
    ├── Query: (SDL3_Label, Position)
    │   for each match:
    │     render text surface -> texture -> SDL_RenderTexture
    │
    └── (additional feature queries)

After ecs_progress returns:
  SDL3_Window: SDL_RenderPresent(renderer) per window
```

## Anti-Patterns to Avoid

### Anti-Pattern 1: Global SDL State Instead of Per-Window ECS Components

**What:** Storing SDL_Window* and SDL_Renderer* in global variables or static structs instead of as ECS components on window entities.

**Why bad:** Prevents multi-window support. Forces single-window assumptions into the architecture that are extremely painful to remove later. Violates the ECS data model.

**Instead:** Every window is an entity. SDL_Window* and SDL_Renderer* live in components on that entity. Systems query for these components. This gives multi-window support for free.

### Anti-Pattern 2: Renderer Creates Its Own Event Loop

**What:** Having the renderer or input system call SDL_PollEvent independently, or having multiple event drain points.

**Why bad:** SDL_PollEvent drains from a single global queue. If two systems both call it, they each get half the events. Events get lost.

**Instead:** Single drain point in SDL3_Window provider. Events are categorized and forwarded to the appropriate handler. Input events go to SDL3_Input accumulator. Window events go to WindowState FSM.

### Anti-Pattern 3: Texture Loading During Render Phase

**What:** Loading textures lazily during the render system (e.g., first time an entity with a sprite path is encountered, load the texture).

**Why bad:** File I/O during render causes frame hitches. SDL3_image loading is synchronous and can take milliseconds. This makes frame timing unpredictable.

**Instead:** Load textures during OnLoad or PreUpdate phase. Use a "needs loading" tag component. A texture loading system processes entities with `SDL3_TextureSource` but without `SDL3_TextureHandle`, loads the texture, and adds the handle component. By the time PostUpdate render runs, all textures are ready.

### Anti-Pattern 4: Direct SDL Calls in Consumer Code

**What:** Consumer application calling SDL_CreateWindow, SDL_PollEvent, SDL_RenderPresent directly instead of going through the CELS providers.

**Why bad:** Bypasses the state machine, lifecycle management, and ECS integration. Creates hidden state that the framework cannot reason about.

**Instead:** All SDL interaction goes through the providers. Consumer code creates window entities with config components, reads input via ECS singleton, and renders via Feature/Provider model. The only SDL header the consumer should need is for constants (scan codes, etc.).

### Anti-Pattern 5: Tight Coupling Between Texture/Text and Renderer Internals

**What:** Texture and text modules directly accessing SDL_Renderer internals or making assumptions about renderer implementation.

**Why bad:** When SDL_GPU provider replaces SDL_Renderer later, tightly coupled texture/text code must be rewritten too.

**Instead:** Texture and text modules interact with the renderer through a thin interface: "give me a renderer handle for this window entity." The renderer provider owns the handle; texture/text modules receive it as a parameter. This way, swapping SDL_Renderer for SDL_GPU only requires changing the renderer provider, not all resource modules.

### Anti-Pattern 6: Blocking in the Event Loop

**What:** Performing heavy computation or synchronous I/O inside the event drain or between drain and present.

**Why bad:** Causes input lag and visual stuttering. SDL3 expects the event loop to run at display refresh rate.

**Instead:** Heavy work goes into ECS systems that can be profiled and budgeted. The frame loop structure (drain -> tick -> present) should be as lean as possible in the surrounding scaffolding.

## Component Boundaries (Dependency Graph)

```
                SDL3_Engine
               /     |     \
              /      |      \
             v       v       v
    SDL3_Window  SDL3_Input  SDL3_Renderer
         |           |            |     \
         |           |            |      \
         +-----+-----+       SDL3_Texture  SDL3_Text
               |
         (all share CELS framework)
```

**Hard dependencies (must exist for the other to work):**
- SDL3_Renderer depends on SDL3_Window (needs SDL_Window* to create SDL_Renderer*)
- SDL3_Texture depends on SDL3_Renderer (needs SDL_Renderer* to create SDL_Texture*)
- SDL3_Text depends on SDL3_Renderer (needs SDL_Renderer* for texture creation from surface)
- SDL3_Input depends on SDL3_Window (events only flow after window exists, event routing uses windowID)

**Soft dependencies (communicates but does not require):**
- SDL3_Input -> consumer systems (input state is an ECS singleton, consumed by any system)
- SDL3_Renderer -> SDL3_Texture/SDL3_Text (renderer dispatches draw calls, but could render primitives without textures/text)

## Suggested Build Order

Based on the dependency graph, the recommended implementation order is:

### Phase 1: Foundation (SDL3_Engine + SDL3_Window)
**Build first because:** Everything depends on having a window. Cannot test anything without SDL_Init and a visible window.

1. **SDL3_Engine** -- SDL_Init(SDL_INIT_VIDEO), SDL_Quit, CEL_DefineModule scaffold, SDL3_Engine_use() registration function
2. **SDL3_Window** -- SDL_CreateWindow, WindowState FSM (at minimum NONE->CREATED->SURFACE_READY->READY->CLOSING->CLOSED), basic frame loop (poll events, present)

**Milestone validation:** A window opens, clears to a color, closes on SDL_EVENT_QUIT. The CELS module system loads it via CEL_Build.

### Phase 2: Input (SDL3_Input)
**Build second because:** Input is needed to interact with the window and test everything else. It only depends on Phase 1.

3. **SDL3_Input** -- Event drain from window provider, summarized state (keyboard + mouse initially), ECS singleton component, OnLoad system registration

**Milestone validation:** Press Escape to close window. Print mouse position. Consumer system reads SDL3_InputState.

### Phase 3: Rendering (SDL3_Renderer)
**Build third because:** Depends on window (Phase 1). Needed before textures/text.

4. **SDL3_Renderer** -- SDL_CreateRenderer per window, clear/present cycle, basic draw primitives (filled rect, line), Feature/Provider model scaffold (CEL_DefineFeature, CEL_Provides)

**Milestone validation:** Colored rectangles drawn on screen. Feature/Provider dispatch works.

### Phase 4: Resources (SDL3_Texture + SDL3_Text)
**Build last because:** Depends on renderer (Phase 3). These are the highest-level features.

5. **SDL3_Texture** -- SDL3_image initialization, PNG/JPG loading, SDL_Texture management, SDL3_Sprite feature + rendering
6. **SDL3_Text** -- SDL3_ttf initialization, font loading, text-to-texture rendering, SDL3_Label feature + rendering

**Milestone validation:** Image displayed on screen. Text rendered with custom font. Full example app works.

### Cross-Cutting Concerns (Throughout All Phases)

- **sdl3_components.h** -- Grows with each phase. Phase 1 adds WindowState/WindowConfig/WindowHandle. Phase 2 adds InputState. Phase 3 adds renderer components. Phase 4 adds Sprite/Label/TextureHandle/FontHandle.
- **Multi-window support** -- Designed from Phase 1 (per-entity windows), but only explicitly tested from Phase 3 onward when rendering to specific windows matters.
- **Error handling** -- Each phase adds SDL3 error checking (SDL_GetError()) with CELS-appropriate error reporting.

## SDL3-Specific Architectural Notes

### SDL3 vs SDL2 Key Differences Affecting Architecture

| Area | SDL2 | SDL3 | Impact on Architecture |
|------|------|------|----------------------|
| Event types | `SDL_QUIT`, `SDL_KEYDOWN` | `SDL_EVENT_QUIT`, `SDL_EVENT_KEY_DOWN` | Namespace change; update all event switch cases |
| Window events | `SDL_WINDOWEVENT` + subtype | Each is top-level event type | Simpler switch, no nested subtype check |
| Renderer creation | `SDL_CreateRenderer(window, -1, flags)` | `SDL_CreateRenderer(window, NULL)` | Simpler API; driver name string instead of index |
| Texture from surface | `SDL_CreateTextureFromSurface` | Same API, but surface format changes | Minor; SDL_Surface uses `SDL_PixelFormat` enum not struct pointer |
| Init flags | `SDL_INIT_EVERYTHING` | Subsystem-specific init | Must init each subsystem explicitly |
| Boolean return | 0 for success | `true` for success (SDL_bool) | Invert all error checks from SDL2 patterns |
| Properties | N/A | SDL_CreateProperties for window/renderer hints | Optional but powerful for advanced configuration |

### SDL3 Threading Model

SDL3's event queue is thread-safe for pushing events, but `SDL_PollEvent` must be called from the main thread (the thread that called SDL_Init). This means:

- The frame loop (in SDL3_Window) MUST run on the main thread
- ECS systems that run during ecs_progress also run on the main thread (flecs single-threaded mode)
- This is compatible with CELS's model -- no special threading consideration needed for v1

### SDL3 Renderer Scaling and DPI

SDL3 handles high-DPI differently from SDL2. `SDL_CreateWindow` with `SDL_WINDOW_HIGH_PIXEL_DENSITY` flag gives a window where logical size differs from pixel size. `SDL_GetWindowSizeInPixels` returns the actual pixel dimensions.

**Architectural implication:** The WindowConfig component should store both logical size and pixel size. Rendering coordinates use logical size; texture operations may need pixel size.

## Confidence Notes

| Topic | Confidence | Rationale |
|-------|------------|-----------|
| Provider Module Pattern | HIGH | Directly derived from cels-ncurses reference architecture described in project context |
| SDL3 event architecture | HIGH | SDL3 event model is well-established; event loop pattern is canonical |
| Multi-window via ECS entities | HIGH | Standard ECS pattern; SDL3 natively supports multi-window with per-window IDs |
| WindowState FSM | HIGH | Defined in project context, maps cleanly to SDL3 window events |
| Build order | HIGH | Follows natural dependency chain; validated by cels-ncurses precedent |
| SDL3 API specifics (function signatures) | MEDIUM | Based on training data through early 2025; SDL3 API was stabilized by 1.0 release but minor changes possible. Verify exact function signatures against SDL3 headers during implementation. |
| Feature/Provider rendering dispatch | MEDIUM | Pattern defined by CELS framework; exact macro signatures (CEL_DefineFeature, CEL_Provides, CEL_Feature) should be verified against current CELS headers during implementation |
| SDL3_image / SDL3_ttf integration | MEDIUM | These companion libraries follow SDL3's API conventions but their exact current API should be verified during implementation phases |

---
*Architecture research for: SDL3 CELS Backend Module*
*Researched: 2026-03-15*
