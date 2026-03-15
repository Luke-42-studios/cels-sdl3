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
 * Module Init
 * ============================================================================ */

CEL_Module(SDL3_Engine, init) {
    cels_register(SDL3_ContextState, SDL3_ContextLC,
                  SDL3_ContextConfig);
}

/* ============================================================================
 * Compositions
 * ============================================================================ */

CEL_Composition(SDL3Context) {
    cel_has(SDL3_ContextConfig, .video = cel.video);
    cels_lifecycle_bind_entity(SDL3_ContextLC_id, cels_get_current_entity());
}
