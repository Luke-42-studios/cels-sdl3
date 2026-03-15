# Requirements: cels-sdl3

**Defined:** 2026-03-15
**Core Value:** Developers can build 2D graphical applications using the CELS framework with SDL3 as the backend, using the same declarative ECS patterns they know from cels-ncurses

## v1 Requirements

### Foundation

- [ ] **FNDN-01**: SDL3 initializes with correct subsystem flags (VIDEO, EVENTS) and SDL3_image/SDL3_ttf libraries init
- [ ] **FNDN-02**: SDL3 shuts down cleanly — SDL3_ttf, SDL3_image, SDL3 quit in correct reverse order
- [ ] **FNDN-03**: Window lifecycle state machine implements full state chain (NONE->CREATED->SURFACE_READY->READY->RESIZING->MINIMIZED->CLOSING->CLOSED)
- [ ] **FNDN-04**: Each window is an ECS entity with Window/Renderer components — multi-window from day one
- [ ] **FNDN-05**: Frame loop integrates with CELS system scheduling — pumps events, ticks ECS, presents per frame
- [ ] **FNDN-06**: Delta time calculated via SDL_GetPerformanceCounter/Frequency, passed to ECS tick

### Input

- [ ] **INPT-01**: SDL events are polled once per frame and buffered into a raw event queue
- [ ] **INPT-02**: Event queue is accessible as an ECS component for systems to iterate
- [ ] **INPT-03**: Window-specific events (close, resize, focus) route to correct window entity's state machine

### Rendering

- [ ] **RNDR-01**: SDL_Renderer created per window entity, paired with its SDL_Window
- [ ] **RNDR-02**: Clear/present cycle runs per window per frame (clear background, draw, present)
- [ ] **RNDR-03**: Feature/Provider model — CEL_DefineFeature(Renderable), CEL_Provides(SDL3, Renderable, ...)
- [ ] **RNDR-04**: Developer can draw filled rectangles with color at position/size
- [ ] **RNDR-05**: Developer can draw outlined rectangles with color at position/size
- [ ] **RNDR-06**: Developer can draw lines with color between two points

### Textures

- [ ] **TXTR-01**: Developer can load PNG/JPG image from file path into SDL_Texture via SDL3_image
- [ ] **TXTR-02**: Developer can render texture at position/size with optional source rect
- [ ] **TXTR-03**: Textures are associated with their window's renderer (renderer-affine)

### Text

- [ ] **TEXT-01**: Developer can load TTF font from file path at specified point size via SDL3_ttf
- [ ] **TEXT-02**: Developer can render text string at position with color and size
- [ ] **TEXT-03**: Rendered text is cached — unchanged text does not re-render every frame

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
| FNDN-01 | | Pending |
| FNDN-02 | | Pending |
| FNDN-03 | | Pending |
| FNDN-04 | | Pending |
| FNDN-05 | | Pending |
| FNDN-06 | | Pending |
| INPT-01 | | Pending |
| INPT-02 | | Pending |
| INPT-03 | | Pending |
| RNDR-01 | | Pending |
| RNDR-02 | | Pending |
| RNDR-03 | | Pending |
| RNDR-04 | | Pending |
| RNDR-05 | | Pending |
| RNDR-06 | | Pending |
| TXTR-01 | | Pending |
| TXTR-02 | | Pending |
| TXTR-03 | | Pending |
| TEXT-01 | | Pending |
| TEXT-02 | | Pending |
| TEXT-03 | | Pending |
| INTG-01 | | Pending |
| INTG-02 | | Pending |
| INTG-03 | | Pending |
| INTG-04 | | Pending |
| INTG-05 | | Pending |

**Coverage:**
- v1 requirements: 26 total
- Mapped to phases: 0
- Unmapped: 26

---
*Requirements defined: 2026-03-15*
*Last updated: 2026-03-15 after initial definition*
