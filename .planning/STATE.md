# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-15)

**Core value:** Developers can build 2D graphical applications using the CELS framework with SDL3 as the backend, using the same declarative ECS patterns they know from cels-ncurses
**Current focus:** Phase 9 in progress -- Module Integration. Plan 01 complete (error reporting). Plan 02 next (registration API).

## Current Position

Phase: 9 of 10 (Module Integration)
Plan: 1 of 2 in current phase
Status: In progress
Last activity: 2026-03-22 -- Completed 09-01-PLAN.md (error reporting infrastructure)

Progress: [█████████████████░░░] 85% (17/20 plans complete)

## Performance Metrics

**Velocity:**
- Total plans completed: 17
- Average duration: 6.9 min
- Total execution time: 1.9 hours

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
| 08-text-rendering | 2/2 | 9 min | 4.5 min |
| 09-module-integration | 1/2 | 3 min | 3 min |

**Recent Trend:**
- Last 5 plans: 07-01 (4 min), 07-02 (2 min), 08-01 (6 min), 08-02 (3 min), 09-01 (3 min)
- Trend: Consistent 2-6 min per plan with established patterns

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
- [08-01]: TTF_TextEngine stored on SDL3_Renderer component (couples text engine to renderer lifetime)
- [08-01]: Text entities separate from window entities -- system finds first active renderer then iterates text
- [08-01]: Block scope required for multiple cel_query calls in same system body (_cel_next_field_index redefinition)
- [08-01]: Two-pass cleanup: first pass destroys TTF_Text handles, second pass destroys text engine then renderer
- [08-01]: sdl3_fonts_close_all() in sdl3_shutdown() before TTF_Quit() for safe font cleanup
- [08-02]: Text entities created via cel_entity() in CEL_Compose alongside SDL3Window -- same TU avoids per-TU ID issues
- [08-02]: TextInteraction uses dual block-scoped cel_query: first reads events, second modifies text
- [08-02]: Dynamic text identified by y-position range rather than entity ID tracking
- [09-01]: All SDL_Log error calls migrated to sdl3_report_error with callback routing and SDL_Log fallback
- [09-01]: sdl3_report_error context strings are descriptive (e.g., "create_window", "font_load_invalid_id")
- [09-01]: Init flag precedence: s_init_flags (from SDL3_Config) > config->video (from ECS ContextConfig) > no-op

### Pending Todos

None.

### Blockers/Concerns

- [Research]: CELS framework macros (CEL_DefineModule, CEL_DefineFeature, CEL_Provides) need verification against current headers
- [Note]: cels framework test_lifecycle_fsm has pre-existing build failure (missing header) -- does not affect cels-sdl3
- [Note]: Build environment requires FETCHCONTENT_SOURCE_DIR_SDL3 pointing to local SDL3 checkout for CMake reconfiguration (no network access)
- [Note]: SDL_VIDEODRIVER=dummy crashes all examples (pre-existing, dummy driver doesn't support renderer creation)

## Session Continuity

Last session: 2026-03-22 00:04 UTC
Stopped at: Completed 09-01-PLAN.md (error reporting infrastructure) -- Phase 9 plan 1 of 2 done
Resume file: None
