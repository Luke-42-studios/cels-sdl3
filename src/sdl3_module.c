/*
 * SDL3 Module - CEL_Module(SDL3_Engine) Definition
 *
 * All CELS declarations for the SDL3 module: lifecycles, observers,
 * and compositions. Implementation details (SDL3 init/shutdown) are
 * delegated to helper functions in sdl3_init.c.
 *
 * IMPORTANT: All code that uses component _id variables MUST live in
 * this file. CEL_Component generates static per-TU _id vars, and
 * cels_register only initializes THIS TU's copies.
 */

#include <cels_sdl3.h>
#include "sdl3_internal.h"

CEL_State(SDL3_ContextState);
CEL_State(SDL3_FrameState);

/* ============================================================================
 * Context Lifecycle
 * ============================================================================ */

CEL_Lifecycle(SDL3_ContextLC);

CEL_Observe(SDL3_ContextLC, on_create) {
    const SDL3_ContextConfig* config = cel_watch(entity, SDL3_ContextConfig);
    if (!config) return;
    sdl3_init(config);
}

CEL_Observe(SDL3_ContextLC, on_destroy) {
    (void)entity;
    sdl3_shutdown();
}

/* ============================================================================
 * Window Lifecycle
 * ============================================================================ */

CEL_Lifecycle(SDL3_WindowLC);

CEL_Observe(SDL3_WindowLC, on_create) {
    const SDL3_WindowConfig* config = cel_watch(entity, SDL3_WindowConfig);
    if (!config) return;
    if (config->context) {
        sdl3_ensure_init();
        if (config->target_fps > 0) {
            sdl3_set_target_fps(config->target_fps);
        }
    }
    sdl3_window_create(entity, config, SDL3_WindowComponent_id);
}

CEL_Observe(SDL3_WindowLC, on_destroy) {
    const SDL3_WindowComponent* comp = cel_watch(entity, SDL3_WindowComponent);
    if (comp && comp->window) {
        sdl3_window_destroy(comp->window);
    }
}

/* ============================================================================
 * Event Pump System -- drains SDL events each frame
 * ============================================================================
 *
 * Runs at OnLoad phase BEFORE WindowStateSystem. Pumps all pending SDL
 * events, routes window events to sdl3_window_handle_event, and handles
 * SDL_EVENT_QUIT. When all windows are minimized, blocks on SDL_WaitEvent
 * for zero CPU usage.
 */

CEL_System(SDL3_EventPumpSystem, .phase = OnLoad) {
    cel_run {
        /* --- Minimized pause: block with zero CPU when all windows minimized --- */
        int total_windows = 0;
        int minimized_windows = 0;

        cel_query(SDL3_WindowComponent);
        cel_each(SDL3_WindowComponent) {
            total_windows++;
            if (SDL3_WindowComponent->state == SDL3_WINDOW_MINIMIZED) {
                minimized_windows++;
            }
        }

        SDL_Event event;

        if (total_windows > 0 && total_windows == minimized_windows) {
            /* All windows minimized -- block until waking event */
            if (SDL_WaitEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    cel_quit();
                    return;
                }
                /* Route window events from the waking event */
                switch (event.type) {
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                case SDL_EVENT_WINDOW_MINIMIZED:
                case SDL_EVENT_WINDOW_RESTORED:
                case SDL_EVENT_WINDOW_RESIZED: {
                    cel_query(SDL3_WindowComponent);
                    cel_each(SDL3_WindowComponent) {
                        if (SDL3_WindowComponent->window_id == event.window.windowID) {
                            cel_update(SDL3_WindowComponent) {
                                sdl3_window_handle_event(
                                    SDL3_WindowComponent, event.type,
                                    event.window.data1, event.window.data2);
                            }
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }
            return;  /* Skip normal poll loop this frame */
        }

        /* --- Normal path: drain all pending events --- */
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                cel_quit();
                return;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            case SDL_EVENT_WINDOW_MINIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_RESIZED: {
                /* Find matching window entity by windowID */
                cel_query(SDL3_WindowComponent);
                cel_each(SDL3_WindowComponent) {
                    if (SDL3_WindowComponent->window_id == event.window.windowID) {
                        cel_update(SDL3_WindowComponent) {
                            sdl3_window_handle_event(
                                SDL3_WindowComponent, event.type,
                                event.window.data1, event.window.data2);
                        }
                    }
                }
                break;
            }

            default:
                /* Ignore other events (Phase 4 will add raw event buffer) */
                break;
            }
        }
    }
}

/* ============================================================================
 * Window State System -- per-frame state machine driver
 * ============================================================================ */

CEL_System(SDL3_WindowStateSystem, .phase = OnLoad) {
    cel_query(SDL3_WindowComponent);
    cel_each(SDL3_WindowComponent) {
        if (SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING) {
            /* CLOSING -> CLOSED: one frame has elapsed since close request.
             * Destroy the SDL_Window and mark terminal state. */
            bool owns_context = SDL3_WindowComponent->context_bound;
            if (SDL3_WindowComponent->window) {
                sdl3_window_destroy(SDL3_WindowComponent->window);
            }
            cel_update(SDL3_WindowComponent) {
                SDL3_WindowComponent->window = NULL;
                SDL3_WindowComponent->state = SDL3_WINDOW_CLOSED;
            }
            /* Context-bound window closed: shut down SDL3 and exit loop */
            if (owns_context) {
                sdl3_shutdown();
                sdl3_frame_set_running(false);
            }
        }
    }
}

/* ============================================================================
 * Frame State System -- manages running flag based on window states
 * ============================================================================
 *
 * Runs at OnLoad phase AFTER WindowStateSystem. Checks if all windows
 * have reached CLOSED state and sets running = false to exit the loop.
 */

CEL_System(SDL3_FrameStateSystem, .phase = OnLoad) {
    cel_run {
        int total_windows = 0;
        int closed_windows = 0;

        cel_query(SDL3_WindowComponent);
        cel_each(SDL3_WindowComponent) {
            total_windows++;
            if (SDL3_WindowComponent->state == SDL3_WINDOW_CLOSED) {
                closed_windows++;
            }
        }

        /* All windows closed -> stop running */
        if (total_windows > 0 && total_windows == closed_windows) {
            sdl3_frame_set_running(false);
        }
    }
}

/* ============================================================================
 * Module Init
 * ============================================================================
 *
 * Registration order determines system execution within the same phase:
 *   1. EventPump (drains events, routes to windows)
 *   2. WindowState (CLOSING -> CLOSED transitions)
 *   3. FrameState (checks if all windows closed)
 */

CEL_Module(SDL3_Engine, init) {
    cels_register(SDL3_ContextState, SDL3_ContextLC,
                  SDL3_ContextConfig);
    cels_register(SDL3_FrameState, SDL3_EventPumpSystem);
    cels_register(SDL3_WindowConfig, SDL3_WindowComponent,
                  SDL3_WindowLC, SDL3_WindowStateSystem);
    cels_register(SDL3_FrameStateSystem);
}

/* ============================================================================
 * Compositions
 * ============================================================================ */

CEL_Composition(SDL3Context) {
    cel_has(SDL3_ContextConfig, .video = cel.video);
    cels_lifecycle_bind_entity(SDL3_ContextLC_id, cels_get_current_entity());
}

CEL_Composition(SDL3Window) {
    cel_has(SDL3_WindowConfig,
        .title      = cel.title,
        .width      = cel.width  ? cel.width  : 1280,
        .height     = cel.height ? cel.height : 720,
        .flags      = cel.flags | SDL_WINDOW_RESIZABLE,
        .context    = cel.context,
        .target_fps = cel.target_fps
    );
    cels_lifecycle_bind_entity(SDL3_WindowLC_id, cels_get_current_entity());
}
