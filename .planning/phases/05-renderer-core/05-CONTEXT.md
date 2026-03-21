# Phase 5: Renderer Core - Context

**Gathered:** 2026-03-21
**Status:** Ready for planning

<domain>
## Phase Boundary

Each window entity gets a paired SDL_Renderer that runs a clear/present cycle every frame, producing a visible colored background. Developer can set and change the clear color at runtime. Multiple windows render independently. This phase does NOT include draw primitives, textures, or text — those are Phases 6-8.

</domain>

<decisions>
## Implementation Decisions

### Renderer-window pairing
- SDL_Renderer stored as a separate ECS component on the window entity (not embedded in SDL3Window)
- Renderer created automatically when window reaches READY state — every window gets one
- Renderer stays alive when minimized but clear/present cycle is skipped (matches existing frame loop pause)
- Renderer destroyed automatically during CLOSING->CLOSED transition, before SDL_Window destruction

### Clear color API
- Clear color is a field on the SDL3_Renderer component using SDL_Color type (RGBA uint8, 0-255)
- Default clear color: cornflower blue (100, 149, 237, 255) — immediately visible that rendering works
- Runtime-changeable: any system can write to clear_color and it takes effect next frame
- Per-window: each window's renderer has its own independent clear color

### Render cycle boundaries
- Use CELS system phases for the render cycle — clear and present as separate system phases
- Developer draw systems slot in between clear and present using standard CELS system phase ordering
- Render cycle runs every frame unconditionally (no dirty flag optimization)

### Multi-window rendering
- Each window renders independently with its own clear color
- No defined render order between windows — ECS iteration order is fine
- Closing one window has no effect on other windows' render cycles

### Claude's Discretion
- Renderer component storage design (whether to use SDL3_Renderer as component name or another convention)
- Exact CELS system phase names for clear/present
- How the system ordering approach works with existing systems (InputSystem, WindowStateSystem, FrameStateSystem)

</decisions>

<specifics>
## Specific Ideas

- Example app: single window with interactive clear color changes via keyboard input
- Cornflower blue default — the classic game dev "it's working" indicator
- Must use CELS system phases (the CELS way) for render cycle ordering, not callbacks

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 05-renderer-core*
*Context gathered: 2026-03-21*
