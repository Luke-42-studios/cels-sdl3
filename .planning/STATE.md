# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-15)

**Core value:** Developers can build 2D graphical applications using the CELS framework with SDL3 as the backend, using the same declarative ECS patterns they know from cels-ncurses
**Current focus:** Phase 7 complete -- Textures. Both plans delivered (core + example). Ready for Phase 8.

## Current Position

Phase: 7 of 10 (Textures)
Plan: 2 of 2 in current phase
Status: Phase complete
Last activity: 2026-03-21 -- Completed 07-02-PLAN.md (textures example with sprite controller)

Progress: [██████████████░░░░░░] 70% (14/20 plans complete)

## Performance Metrics

**Velocity:**
- Total plans completed: 14
- Average duration: 7.4 min
- Total execution time: 1.70 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01-sdl3-bootstrap | 2/2 | 7 min | 3.5 min |
| 02-window-provider | 2/2 | 8 min | 4 min |
| 03-frame-loop | 2/2 | 12 min | 6 min |
| 04-input-system | 2/2 | 58 min | 29 min |
| 05-renderer-core | 2/2 | 5 min | 2.5 min |
| 06-draw-primitives | 2/2 | 6 min | 3 min |
| 07-textures | 2/2 | 6 min | 3 min |

**Recent Trend:**
- Last 5 plans: 05-02 (2 min), 06-01 (4 min), 06-02 (2 min), 07-01 (4 min), 07-02 (2 min)
- Trend: Phases 5-7 consistently fast (2-4 min per plan) -- established patterns and reusable infrastructure

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Roadmap]: 10-phase linear dependency chain derived from 26 requirements at comprehensive depth
- [01-01]: SDL3 3.4.2 + SDL3_image 3.4.0 + SDL3_ttf 3.2.2 confirmed compatible
- [01-01]: All CELS declarations in single TU (sdl3_module.c) per per-TU static ID constraint
- [02-01]: State chain NONE->CREATED->SURFACE_READY->READY advances synchronously during creation
- [02-01]: CLOSING->CLOSED driven by per-frame state system (one frame delay for cleanup)
- [02-02]: Wayland requires surface buffer commit + SDL_ShowWindow for window visibility
- [03-01]: SDL3_FrameState data owned by sdl3_loop.c (same pattern as SDL3_ContextState in sdl3_init.c)
- [03-01]: Event pump routes window events inline via cel_query/cel_each/cel_update
- [03-01]: Registration order InputSystem -> WindowState -> FrameState ensures correct OnLoad execution
- [03-02]: .context = true on SDL3Window binds SDL3 context lifecycle to that window
- [03-02]: WindowStateSystem triggers sdl3_shutdown() when context-bound window closes
- [03-02]: Frame rate capping via .target_fps on SDL3Window, wired through sdl3_delta() -> sdl3_cap_frame_rate()
- [04-01]: Fixed 64-event SDL3_EventQueue (8 KiB per window), cleared every frame
- [04-01]: Window table pattern: system builds SDL3_WindowTable via cel_each/cel_update, passes to cross-TU drain function
- [04-01]: cels_ensure_component required for components used in observers (CEL_Component _register is a no-op)
- [04-01]: Focus events routed to sdl3_window_handle_event as no-ops (no state transition)
- [04-02]: Event consumer pattern: raw for-loop + switch on SDL3_EventQueue->events[0..count-1], no convenience macros
- [04-02]: Mouse motion throttled via static counter (every 30th event) for readable console output
- [05-01]: cels_entity_get_component used to read back window pointer in on_create observer for renderer creation
- [05-01]: No cel_update for render clear/present -- SDL-internal backbuffer mutation is outside ECS tracking
- [05-01]: Render systems skip MINIMIZED, CLOSING, CLOSED windows (RESIZING windows still render)
- [05-02]: cel_update(Component) is a block scope macro providing mutable access -- components are const in cel_each
- [05-02]: Deferred mutation pattern: collect changes in locals, apply in single cel_update block
- [06-01]: Draw buffer fields embedded in SDL3_Renderer component (not a separate ECS component)
- [06-01]: SDL3_DrawBufferTable rebuilt each frame in PreRender for renderer-to-buffer lookup from cross-TU vtable code
- [06-01]: Simple per-command flush (no batch coalescing) -- SDL3 batches internally on GPU side
- [06-01]: cel_update required for draw buffer reset and flush (modifies ECS component struct fields, unlike SDL render calls)
- [06-02]: SDL3_Renderable_use() called after cels_register(SDL3_Engine) but before cels_session for vtable population
- [07-01]: Sprites live on window entities (same entity as renderer) for simplest query pattern
- [07-01]: Opaque uint32_t handle (1-based index) for texture references -- survives cache invalidation gracefully
- [07-01]: Cache invalidation zeroes entries without SDL_DestroyTexture -- SDL_DestroyRenderer handles that
- [07-01]: Per-sprite alpha modulation with reset to prevent state leakage between sprites sharing same texture
- [07-01]: Declarative asset loading pattern: set path on component, system detects NONE state and loads automatically
- [07-02]: Single sprite per entity with keyboard mode switching to demonstrate all features
- [07-02]: POST_BUILD copy_directory for asset deployment to build directory

### Pending Todos

None.

### Blockers/Concerns

- [Research]: SDL3_ttf text engine API may have changed significantly from SDL2_ttf (Phase 8)
- [Research]: CELS framework macros (CEL_DefineModule, CEL_DefineFeature, CEL_Provides) need verification against current headers
- [Note]: cels framework test_lifecycle_fsm has pre-existing build failure (missing header) -- does not affect cels-sdl3
- [Note]: Build environment requires FETCHCONTENT_SOURCE_DIR_SDL3 pointing to local SDL3 checkout for CMake reconfiguration (no network access)

## Session Continuity

Last session: 2026-03-21 23:40 UTC
Stopped at: Completed 07-02-PLAN.md (textures example) -- Phase 7 complete
Resume file: None
