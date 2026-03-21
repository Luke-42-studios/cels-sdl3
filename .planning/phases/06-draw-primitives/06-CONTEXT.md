# Phase 6: Draw Primitives - Context

**Gathered:** 2026-03-21
**Status:** Ready for planning

<domain>
## Phase Boundary

Draw shapes (filled rects, outlined rects, lines) using the CELS Feature/Provider rendering model. Developer issues draw calls from within systems, and shapes render within the existing clear/present cycle. Textures (Phase 7) and text (Phase 8) are separate features.

</domain>

<decisions>
## Implementation Decisions

### Draw API surface
- Imperative draw calls inside systems (not component-based entities)
- Functions use SDL3 struct types: SDL_FRect for rects, SDL_FPoint for line endpoints, SDL_Color for colors
- Both single-shape and batch variants: sdl3_draw_filled_rect() and sdl3_draw_filled_rects()
- Batch variants take parallel arrays (rects[] + colors[] + count)
- Z-index is a per-call parameter on every draw function (not state-based)

### Feature/Provider wiring
- Single CEL_DefineFeature(Renderable) covers all draw primitives (rects, lines)
- Developer calls through feature dispatch vtable: Renderable->filled_rect(r, rect, color, z)
- Enables backend swapping without changing user code
- Explicit registration via SDL3_Renderable_use(world) — not auto-registered on module init
- Textures and text get their own separate features in Phases 7-8 (not added to Renderable)

### Color and coordinate model
- SDL_Color (0-255 RGBA uint8) for all color parameters — matches SDL3 native types
- Pixel coordinates only via SDL_FRect (float pixels) — no normalized/relative coordinate helpers
- Per-shape color in batch calls (parallel color array, not single color for batch)
- Alpha blending OFF by default — developer must explicitly enable via sdl3_set_blend_mode()

### Draw ordering
- Global z-index across all systems — shapes sort by z regardless of which system issued the call
- Call order (creation order) breaks ties within same z-index
- Draw calls are buffered during the frame, sorted by z-index, then flushed at present time
- Buffering enables optimization: same-type/same-color draws can be batched after sorting

### Claude's Discretion
- How the developer obtains the renderer reference inside draw systems (query pattern)
- Draw call buffer implementation (fixed vs dynamic, capacity)
- Batch optimization strategy during flush (grouping by type, color, blend mode)
- Painter's algorithm vs z-index threshold for the default z=0 case

</decisions>

<specifics>
## Specific Ideas

- Guiding principle: easy for the developer, optimized draw calls under the hood
- Draw buffer + sort + flush model enables both correct z-ordering and batch optimization
- SDL3's native types (SDL_FRect, SDL_FPoint, SDL_Color) used throughout — no custom wrappers

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 06-draw-primitives*
*Context gathered: 2026-03-21*
