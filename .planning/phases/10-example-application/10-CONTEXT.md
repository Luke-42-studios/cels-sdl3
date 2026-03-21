# Phase 10: Example Application - Context

**Gathered:** 2026-03-21
**Status:** Ready for planning

<domain>
## Phase Boundary

A working example application that demonstrates every v1 feature (window, input, draw primitives, textures, text) in a cohesive mini scene, serving as both validation and living documentation. No new capabilities — this composes what phases 1-9 built.

</domain>

<decisions>
## Implementation Decisions

### App concept
- Mini landscape scene — static daytime mood (blue sky, green ground, sun)
- Full-window scene with no UI chrome or section borders
- 1280x720 widescreen window
- Claude composes specific scene elements (shapes, placement, colors)

### Interactivity
- Click to place shapes in the scene — shapes accumulate freely with no limit
- Escape to close — no other keyboard interactions
- Mouse click is the primary interaction, demonstrating both input system and draw primitives

### Visual composition
- Sky is the clear color (background), ground is a filled rect
- All three primitive types are scene elements: filled rect (ground), outlined rect (building/sun), line (horizon or similar)
- Texture (bundled sprite) placed as a scene element within the landscape
- Text overlays in top-left corner: title, live FPS counter, input instructions

### Text content
- "cels-sdl3 Demo" as title
- Live FPS counter (demonstrates dynamic text updates / cache invalidation)
- "Click to plant / ESC to quit" as instructions

### Assets
- Bundled pixel-art PNG sprite in `examples/assets/` (e.g., tree, cloud, or house — fits landscape)
- Pixel/retro TTF font in `examples/assets/` (freely licensed)
- Assets directory: `examples/assets/` — self-contained alongside example source

### Claude's Discretion
- Specific scene element arrangement and colors
- Which pixel-art sprite to bundle (tree, cloud, house, etc.)
- Specific retro font choice (any freely-licensed pixel font)
- Shape colors for click-to-place mechanic
- Exact text positioning and size
- How placed shapes render (color, size, variety)

</decisions>

<specifics>
## Specific Ideas

- Landscape feel: sky gradient implied by clear color, ground as filled rect, horizon line separating them
- Placed shapes should feel like "planting" things in the scene — small and colorful
- HUD-style text overlay in top-left: title, FPS, instructions stacked vertically
- The FPS counter exercises text cache invalidation (changes every frame)

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 10-example-application*
*Context gathered: 2026-03-21*
