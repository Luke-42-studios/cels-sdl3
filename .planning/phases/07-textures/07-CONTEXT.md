# Phase 7: Textures - Context

**Gathered:** 2026-03-21
**Status:** Ready for planning

<domain>
## Phase Boundary

Load images from disk and render them as textured quads, with textures correctly bound to their creating renderer. Establishes the asset loading pattern that future asset types (fonts, glTFs, etc.) will follow.

</domain>

<decisions>
## Implementation Decisions

### Texture ECS representation
- Texture is a component on the entity that draws it (not a standalone entity)
- Underlying SDL_Texture is loaded once and shared via a cache — multiple entities referencing the same image share one GPU texture
- Reference-counted: when the last entity using a texture is destroyed, the cached SDL_Texture is freed
- Async loading with state machine: entity spawns immediately, texture transitions through loading states

### Sprite component design
- Single unified `SDL3_Sprite` component holds everything: texture ref, source rect, destination rect, rotation angle, flip flags, alpha
- Defined as a CEL_Component
- Source rect support included for spritesheet/sub-image rendering
- Rotation (angle in degrees) and horizontal/vertical flip supported
- Render system queries entities with SDL3_Sprite + SDL3_Renderer

### Texture state machine
- Texture state is a CEL_State so recompositions and observers can watch transitions
- States: NONE -> LOADING -> READY / FAILED / UNLOADED
- Render system only draws textures in READY state
- UNLOADED state set when renderer is destroyed (window closes)

### Renderer affinity
- Sprite automatically resolves its renderer from the window entity it's associated with (zero boilerplate)
- Entities without a window use the context window's renderer
- Wrong-renderer usage: silent skip + warning log (doesn't crash, developer sees issue in logs)
- Per-renderer texture cache — each renderer has its own cache; same image loaded on two windows = two SDL_Textures
- Renderer destruction immediately frees all its cached textures; associated sprites transition to UNLOADED state

### Asset loading pattern
- Declarative loading: developer sets a path in the sprite component, a loading system detects it and triggers load automatically
- Configurable base asset directory — set once, then use relative paths everywhere (portable across platforms)
- Load failure: entity stays in FAILED state, nothing renders, warning logged with path and error detail
- This pattern establishes the precedent for all future asset types (fonts, models, etc.)

### Claude's Discretion
- Component data representation for texture ref (opaque handle vs pointer — choose what's best for cross-platform including web/Android)
- Whether to include per-sprite alpha field
- Exact cache data structure and lookup mechanism
- Async loading implementation details (thread pool, SDL async IO, etc.)
- Asset base directory configuration mechanism

</decisions>

<specifics>
## Specific Ideas

- "Asset loading needs custom phases in lifecycles — think about the best way as everything will follow this pattern (glTFs, etc.)"
- Texture state should be a CEL_State that recompositions can watch, same pattern as window lifecycle
- Components must use CEL_Component declarations
- Cross-platform portability matters for the handle/reference design (web, Android, other targets)

</specifics>

<deferred>
## Deferred Ideas

- glTF/3D model loading — future asset type following the same loading pattern established here
- Async preloading API (imperative `sdl3_texture_preload()`) — could supplement declarative loading later
- Placeholder/error textures for development — opted for nothing + FAILED state, could revisit

</deferred>

---

*Phase: 07-textures*
*Context gathered: 2026-03-21*
