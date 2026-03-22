# Requirements: cels-sdl3

**Defined:** 2026-03-15
**Core Value:** Developers can build 2D graphical applications using the CELS framework with SDL3 as the backend, using the same declarative ECS patterns they know from cels-ncurses

## v1 Requirements

### Foundation

- [x] **FNDN-01**: SDL3 initializes with correct subsystem flags (VIDEO, EVENTS) and SDL3_ttf via TTF_Init (SDL3_image auto-initializes, no explicit init needed)
- [x] **FNDN-02**: SDL3 shuts down cleanly — TTF_Quit then SDL_Quit in correct reverse order (SDL3_image has no quit function in 3.x)
- [x] **FNDN-03**: Window lifecycle state machine implements full state chain (NONE->CREATED->SURFACE_READY->READY->RESIZING->MINIMIZED->CLOSING->CLOSED)
- [x] **FNDN-04**: Each window is an ECS entity with Window/Renderer components — multi-window from day one
- [x] **FNDN-05**: Frame loop integrates with CELS system scheduling — pumps events, ticks ECS, presents per frame
- [x] **FNDN-06**: Delta time calculated via SDL_GetPerformanceCounter/Frequency, passed to ECS tick

### Input

- [x] **INPT-01**: SDL events are polled once per frame and buffered into a raw event queue
- [x] **INPT-02**: Event queue is accessible as an ECS component for systems to iterate
- [x] **INPT-03**: Window-specific events (close, resize, focus) route to correct window entity's state machine

### Rendering

- [x] **RNDR-01**: SDL_Renderer created per window entity, paired with its SDL_Window
- [x] **RNDR-02**: Clear/present cycle runs per window per frame (clear background, draw, present)
- [x] **RNDR-03**: Feature/Provider model — CEL_DefineFeature(Renderable), CEL_Provides(SDL3, Renderable, ...)
- [x] **RNDR-04**: Developer can draw filled rectangles with color at position/size
- [x] **RNDR-05**: Developer can draw outlined rectangles with color at position/size
- [x] **RNDR-06**: Developer can draw lines with color between two points

### Textures

- [x] **TXTR-01**: Developer can load PNG/JPG image from file path into SDL_Texture via SDL3_image
- [x] **TXTR-02**: Developer can render texture at position/size with optional source rect
- [x] **TXTR-03**: Textures are associated with their window's renderer (renderer-affine)

### Text

- [x] **TEXT-01**: Developer can load TTF font from file path at specified point size via SDL3_ttf
- [x] **TEXT-02**: Developer can render text string at position with color and size
- [x] **TEXT-03**: Rendered text is cached — unchanged text does not re-render every frame

### Integration

- [ ] **INTG-01**: SDL3_Engine_use() bundles all providers via CEL_DefineModule(SDL3_Engine)
- [ ] **INTG-02**: Individual providers available a la carte (SDL3_Window_use, SDL3_Input_use, etc.)
- [ ] **INTG-03**: All SDL resources destroyed in correct order on shutdown (textures/fonts before renderer before window)
- [ ] **INTG-04**: SDL_GetError() surfaced on failures through error reporting mechanism
- [ ] **INTG-05**: Example app demonstrates all v1 features — colored window, input handling, draw primitives, texture, text

## v2 Requirements

### Input Enhancements

- **INPT-04**: Keyboard state snapshot as queryable ECS component (per-frame key state)
- **INPT-05**: Mouse state snapshot as ECS component (position, buttons, scroll)
- **INPT-06**: Gamepad discovery and state as ECS entities with hot-plug support
- **INPT-07**: Touch input normalization as first-class ECS component

### Rendering Enhancements

- **RNDR-07**: Draw command buffering — systems emit draw commands, render system z-sorts and batches
- **RNDR-08**: Render target support — render to texture for post-processing, minimap
- **RNDR-09**: Color modulation and alpha blending — texture color mod, blend modes

### Window Enhancements

- **FNDN-07**: Window configuration reactivity — changing WindowConfig component triggers SDL3 calls at runtime
- **FNDN-08**: High-DPI / display scaling awareness for multi-monitor setups
- **FNDN-09**: Clipboard access — SDL_GetClipboardText / SDL_SetClipboardText

## Out of Scope

| Feature | Reason |
|---------|--------|
| Audio (playback, mixing, recording) | Planned as separate future milestone — zero coupling to window/input/render |
| SDL_GPU renderer backend | Module designed for future swap-in, but v1 targets SDL_Renderer only |
| Cross-backend abstraction (shared interface with cels-ncurses) | Premature abstraction — backends have fundamentally different capabilities |
| Sprite sheet slicing / frame animation | v1 provides source rect; animation is higher-level game engine territory |
| Asset pipeline (caching, atlas packing, hot-reload) | Each is a project unto itself; v1 loads files directly |
| Physics integration | Not an SDL3 concern — separate module using Box2D or similar |
| UI layout / widget system | Clay already exists in workspace for this purpose |
| Scene graph / transform hierarchy | ECS purists use flat transforms with parent reference |
| Networking | Completely independent subsystem |
| 3D rendering | Out of scope for this module entirely |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| FNDN-01 | Phase 1: SDL3 Bootstrap | Complete |
| FNDN-02 | Phase 1: SDL3 Bootstrap | Complete |
| FNDN-03 | Phase 2: Window Provider | Complete |
| FNDN-04 | Phase 2: Window Provider | Complete |
| FNDN-05 | Phase 3: Frame Loop | Complete |
| FNDN-06 | Phase 3: Frame Loop | Complete |
| INPT-01 | Phase 4: Input System | Complete |
| INPT-02 | Phase 4: Input System | Complete |
| INPT-03 | Phase 4: Input System | Complete |
| RNDR-01 | Phase 5: Renderer Core | Pending |
| RNDR-02 | Phase 5: Renderer Core | Pending |
| RNDR-03 | Phase 6: Draw Primitives | Complete |
| RNDR-04 | Phase 6: Draw Primitives | Complete |
| RNDR-05 | Phase 6: Draw Primitives | Complete |
| RNDR-06 | Phase 6: Draw Primitives | Complete |
| TXTR-01 | Phase 7: Textures | Complete |
| TXTR-02 | Phase 7: Textures | Complete |
| TXTR-03 | Phase 7: Textures | Complete |
| TEXT-01 | Phase 8: Text Rendering | Complete |
| TEXT-02 | Phase 8: Text Rendering | Complete |
| TEXT-03 | Phase 8: Text Rendering | Complete |
| INTG-01 | Phase 9: Module Integration | Pending |
| INTG-02 | Phase 9: Module Integration | Pending |
| INTG-03 | Phase 9: Module Integration | Pending |
| INTG-04 | Phase 9: Module Integration | Pending |
| INTG-05 | Phase 10: Example Application | Pending |

**Coverage:**
- v1 requirements: 26 total
- Mapped to phases: 26
- Unmapped: 0

---
*Requirements defined: 2026-03-15*
*Last updated: 2026-03-15 after plan revision (SDL3_image 3.x has no init/quit)*
