# Phase 8: Text Rendering - Context

**Gathered:** 2026-03-21
**Status:** Ready for planning

<domain>
## Phase Boundary

Developer can load TTF fonts and render text strings with specified color, size, alignment, and wrapping, with automatic caching that avoids per-frame texture recreation. cels-sdl3 provides the raw SDL3_ttf text rendering capability; Clay integration lives in a separate `cels-clay` library.

</domain>

<decisions>
## Implementation Decisions

### Font resource model
- Clay drives fonts — Clay specifies font family/size/weight, SDL3 resolves and loads the actual TTF files
- Direct fontId registration matching Clay's pattern: `sdl3_font_load(FONT_BODY, "fonts/Roboto.ttf", 16)`
- Fonts loaded during a dedicated Clay init system (OnLoad phase), before any layout runs
- Global font set shared across all windows (single font array)
- Dynamic runtime loading — fonts can be loaded/unloaded at any time, not just at init

### Text component design
- Component-based: SDL3_Text component on entities, system renders all text components each frame
- cels-sdl3 provides raw SDL3_ttf capabilities; Clay renders via separate `cels-clay` lib that consumes cels-sdl3
- SDL3 is the renderer, Clay consumes it — cels-sdl3 does not depend on Clay
- Full property set on SDL3_Text component:
  - Core: string, fontId, fontSize, color, position
  - Alignment: left/center/right
  - Wrapping: max width + wrap mode
  - Letter/line spacing

### Rendering quality
- Blended (antialiased) always — no configurable quality modes
- One style per component — for mixed styles, use multiple entities
- Matches Clay's SDL3 renderer approach (TTF_CreateText + text engine)

### Cache surface
- Fully automatic caching — system detects changes and recreates TTF_Text as needed
- Change detection via ECS reactivity — cel_update(SDL3_Text) signals dirty, unchanged components skip recreation
- Cleanup via lifecycle observer (on_destroy) — TTF_DestroyText called immediately, matching SDL3_Renderer/SDL3_Window patterns

### Claude's Discretion
- Internal TTF_Text management strategy (pooling, inline storage, etc.)
- Exact component struct layout and field types
- Text measurement callback integration details
- How position coordinates interact with the rendering pipeline

</decisions>

<specifics>
## Specific Ideas

- Font API should match Clay's fontId-indexed array pattern exactly — `cels-clay` will bridge these directly
- SDL3_ttf 3.x `TTF_CreateText()` + text engine is the target API (not legacy SDL2_ttf surface rendering)
- Text system should follow established patterns: lifecycle observers for create/destroy, systems for per-frame rendering, cel_update for mutation

</specifics>

<deferred>
## Deferred Ideas

- **cels-clay library** — separate lib bridging Clay layout/render pipeline to cels-sdl3 rendering capabilities. Clay handles layout, wrapping, text measurement; cels-sdl3 provides the rendering backend. This is the user's primary vision for how text (and all rendering) gets consumed in practice.
- **Styled text runs** — bold/italic/color changes within a single text string (v2 candidate)
- **Configurable render quality** — solid/LCD modes for performance-sensitive scenarios (v2 candidate)

</deferred>

---

*Phase: 08-text-rendering*
*Context gathered: 2026-03-21*
