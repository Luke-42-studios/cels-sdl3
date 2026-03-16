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
    sdl3_window_create(entity, config, SDL3_WindowComponent_id);
}

CEL_Observe(SDL3_WindowLC, on_destroy) {
    const SDL3_WindowComponent* comp = cel_watch(entity, SDL3_WindowComponent);
    if (comp && comp->window) {
        sdl3_window_destroy(comp->window);
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
            if (SDL3_WindowComponent->window) {
                sdl3_window_destroy(SDL3_WindowComponent->window);
            }
            cel_update(SDL3_WindowComponent) {
                SDL3_WindowComponent->window = NULL;
                SDL3_WindowComponent->state = SDL3_WINDOW_CLOSED;
            }
        }
    }
}

/* ============================================================================
 * Module Init
 * ============================================================================ */

CEL_Module(SDL3_Engine, init) {
    cels_register(SDL3_ContextState, SDL3_ContextLC,
                  SDL3_ContextConfig);
    cels_register(SDL3_WindowConfig, SDL3_WindowComponent,
                  SDL3_WindowLC, SDL3_WindowStateSystem);
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
        .title  = cel.title,
        .width  = cel.width  ? cel.width  : 1280,
        .height = cel.height ? cel.height : 720,
        .flags  = cel.flags | SDL_WINDOW_RESIZABLE
    );
    cels_lifecycle_bind_entity(SDL3_WindowLC_id, cels_get_current_entity());
}
