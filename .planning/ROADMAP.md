# Roadmap: cels-sdl3

## Overview

cels-sdl3 delivers an SDL3 backend module for the CELS declarative ECS framework, following the strict dependency chain: SDL3 initialization, then windowing with lifecycle state machine, then input summarization, then 2D rendering with Feature/Provider dispatch, then texture and text loading, and finally module bundling with a demonstration application. The 10 phases follow the linear dependency chain at comprehensive depth, with each phase delivering one verifiable capability that the next phase builds on.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: SDL3 Bootstrap** - CMake scaffold, SDL3 FetchContent, init/shutdown lifecycle
- [x] **Phase 2: Window Provider** - Window creation as ECS entity with full lifecycle state machine
- [x] **Phase 3: Frame Loop** - CELS-integrated frame loop with delta time and event pumping
- [ ] **Phase 4: Input System** - Event buffering, raw queue as ECS component, window event routing
- [ ] **Phase 5: Renderer Core** - SDL_Renderer per window with clear/present cycle
- [ ] **Phase 6: Draw Primitives** - Feature/Provider model with filled rects, outlined rects, and lines
- [ ] **Phase 7: Textures** - SDL3_image loading and rendering with renderer affinity
- [ ] **Phase 8: Text Rendering** - SDL3_ttf font loading, text rendering, and caching
- [ ] **Phase 9: Module Integration** - Engine module bundling, a la carte registration, cleanup ordering, error reporting
- [ ] **Phase 10: Example Application** - Demo app exercising all v1 features end-to-end

## Phase Details

### Phase 1: SDL3 Bootstrap
**Goal**: Developer can build a project that links against CELS and SDL3, initializes all required subsystems, and shuts down cleanly
**Depends on**: Nothing (first phase)
**Requirements**: FNDN-01, FNDN-02
**Success Criteria** (what must be TRUE):
  1. CMakeLists.txt builds successfully, fetching SDL3, SDL3_image, and SDL3_ttf via FetchContent (or system packages)
  2. Consumer project can include cels-sdl3 headers and link against the INTERFACE library target
  3. SDL3 initializes with VIDEO subsystem flag and SDL3_ttf via TTF_Init (SDL3_image auto-initializes, no explicit init needed)
  4. SDL3 shuts down cleanly in correct reverse order (TTF_Quit then SDL_Quit; SDL3_image has no quit function) with no resource leaks
**Plans:** 2 plans

Plans:
- [x] 01-01-PLAN.md -- CMake scaffold, INTERFACE library, public header, module and init/shutdown implementation
- [x] 01-02-PLAN.md -- Minimal example app, full build and run verification

### Phase 2: Window Provider
**Goal**: Developer can create one or more windows as ECS entities, each with a full lifecycle state machine tracking its current state
**Depends on**: Phase 1
**Requirements**: FNDN-03, FNDN-04
**Success Criteria** (what must be TRUE):
  1. Window entity spawns an SDL_Window and transitions through the state chain (NONE -> CREATED -> SURFACE_READY -> READY)
  2. Window responds to minimize, resize, and close events by transitioning to MINIMIZED, RESIZING, or CLOSING -> CLOSED states
  3. Multiple window entities can coexist, each with its own SDL_Window and independent state machine
  4. Window component is queryable via standard CELS ECS queries
**Plans:** 2 plans

Plans:
- [x] 02-01-PLAN.md -- Window types, creation/destruction logic, module wiring, and CMake update
- [x] 02-02-PLAN.md -- Multi-window example app with lifecycle verification

### Phase 3: Frame Loop
**Goal**: Developer has a running frame loop that pumps SDL events, ticks the ECS, and produces accurate delta time each frame
**Depends on**: Phase 2
**Requirements**: FNDN-05, FNDN-06
**Success Criteria** (what must be TRUE):
  1. Frame loop integrates with CELS system scheduling -- SDL event pump runs before ECS tick each frame
  2. Delta time is calculated via SDL_GetPerformanceCounter/Frequency and passed to the ECS tick
  3. Frame loop continues running while window is in READY state and stops when all windows are CLOSED
  4. Events are pumped even when windows are minimized (no frozen event queue)
**Plans:** 2 plans

Plans:
- [x] 03-01-PLAN.md -- Frame loop core: delta time, event pump system, running state, FPS tracking
- [x] 03-02-PLAN.md -- Frame loop example app with FPS reporting and clean exit verification

### Phase 4: Input System
**Goal**: Developer can read input events as ECS components -- both a raw event queue for advanced handling and window-specific event routing for lifecycle management
**Depends on**: Phase 3
**Requirements**: INPT-01, INPT-02, INPT-03
**Success Criteria** (what must be TRUE):
  1. SDL events are polled once per frame from a single drain point and buffered into a raw event queue
  2. Raw event queue is accessible as an ECS component that systems can iterate over
  3. Window-specific events (close request, resize, focus gain/loss) route to the correct window entity and update its state machine
  4. A consumer system can read the event queue and respond to keyboard input (e.g., pressing Escape closes the window)
**Plans:** 2 plans

Plans:
- [ ] 04-01-PLAN.md -- Input system core: SDL3_EventQueue component, single-pass event drain, module wiring
- [ ] 04-02-PLAN.md -- Input example app with keyboard/mouse event reading and console logging

### Phase 5: Renderer Core
**Goal**: Each window entity has a paired SDL_Renderer that clears and presents every frame, producing a visible colored background
**Depends on**: Phase 4
**Requirements**: RNDR-01, RNDR-02
**Success Criteria** (what must be TRUE):
  1. SDL_Renderer is created per window entity and stored as a paired component alongside SDL_Window
  2. Each window runs a clear/present cycle every frame (clear to background color, then present)
  3. Developer can set the clear color for a window and see it reflected immediately
  4. Multiple windows each render independently with their own clear color
**Plans**: TBD

Plans:
- [ ] 05-01: TBD
- [ ] 05-02: TBD

### Phase 6: Draw Primitives
**Goal**: Developer can draw shapes (filled rects, outlined rects, lines) using the CELS Feature/Provider rendering model
**Depends on**: Phase 5
**Requirements**: RNDR-03, RNDR-04, RNDR-05, RNDR-06
**Success Criteria** (what must be TRUE):
  1. Feature/Provider model is established -- CEL_DefineFeature(Renderable) and CEL_Provides(SDL3, Renderable, ...) work
  2. Developer can draw a filled rectangle with a specified color at a given position and size
  3. Developer can draw an outlined rectangle with a specified color at a given position and size
  4. Developer can draw a line with a specified color between two points
  5. Draw calls execute within the clear/present cycle and appear on screen
**Plans**: TBD

Plans:
- [ ] 06-01: TBD
- [ ] 06-02: TBD

### Phase 7: Textures
**Goal**: Developer can load images from disk and render them as textured quads, with textures correctly bound to their window's renderer
**Depends on**: Phase 6
**Requirements**: TXTR-01, TXTR-02, TXTR-03
**Success Criteria** (what must be TRUE):
  1. Developer can load a PNG or JPG image from a file path and receive an SDL_Texture via SDL3_image
  2. Developer can render a texture at a specified position and size, with optional source rectangle for sub-image rendering
  3. Textures are associated with their creating renderer -- attempting to use a texture with the wrong renderer is prevented or produces a clear error
  4. Loaded textures can be reused across multiple frames without reloading
**Plans**: TBD

Plans:
- [ ] 07-01: TBD
- [ ] 07-02: TBD

### Phase 8: Text Rendering
**Goal**: Developer can load fonts and render text strings with specified color and size, with caching to avoid per-frame re-rendering
**Depends on**: Phase 7
**Requirements**: TEXT-01, TEXT-02, TEXT-03
**Success Criteria** (what must be TRUE):
  1. Developer can load a TTF font from a file path at a specified point size via SDL3_ttf
  2. Developer can render a text string at a position with specified color and size, and it appears correctly on screen
  3. Unchanged text (same string, font, color, size) does not re-render every frame -- the cached texture is reused
  4. Changing the text string, color, or size invalidates the cache and produces updated rendered text
**Plans**: TBD

Plans:
- [ ] 08-01: TBD
- [ ] 08-02: TBD

### Phase 9: Module Integration
**Goal**: All providers are bundled into a single engine module with correct registration, ordered cleanup, and error reporting
**Depends on**: Phase 8
**Requirements**: INTG-01, INTG-02, INTG-03, INTG-04
**Success Criteria** (what must be TRUE):
  1. SDL3_Engine_use() registers all providers via CEL_DefineModule(SDL3_Engine) in a single call
  2. Individual providers are available a la carte -- developer can call SDL3_Window_use(), SDL3_Input_use(), etc. independently
  3. On shutdown, all SDL resources are destroyed in correct order: textures and fonts first, then renderers, then windows, then SDL_Quit
  4. SDL_GetError() is surfaced through an error reporting mechanism when SDL operations fail
**Plans**: TBD

Plans:
- [ ] 09-01: TBD
- [ ] 09-02: TBD

### Phase 10: Example Application
**Goal**: A working example application demonstrates every v1 feature, serving as both validation and documentation
**Depends on**: Phase 9
**Requirements**: INTG-05
**Success Criteria** (what must be TRUE):
  1. Example app opens a window that clears to a configured background color
  2. Example app handles keyboard input (Escape to close) and displays mouse-responsive behavior
  3. Example app draws at least one filled rect, one outlined rect, and one line on screen
  4. Example app loads and renders a texture from a PNG file
  5. Example app loads a TTF font and renders text on screen
**Plans**: TBD

Plans:
- [ ] 10-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8 -> 9 -> 10

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. SDL3 Bootstrap | 2/2 | Complete | 2026-03-15 |
| 2. Window Provider | 2/2 | Complete | 2026-03-15 |
| 3. Frame Loop | 2/2 | Complete | 2026-03-16 |
| 4. Input System | 0/2 | Not started | - |
| 5. Renderer Core | 0/TBD | Not started | - |
| 6. Draw Primitives | 0/TBD | Not started | - |
| 7. Textures | 0/TBD | Not started | - |
| 8. Text Rendering | 0/TBD | Not started | - |
| 9. Module Integration | 0/TBD | Not started | - |
| 10. Example Application | 0/TBD | Not started | - |
