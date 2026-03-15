/*
 * CELS SDL3 - Developer API
 *
 * Single include for application developers using the SDL3 module:
 *
 *   #include <cels/cels.h>
 *   #include <cels_sdl3.h>
 *
 * Provides:
 *   - SDL3 module registration (cels_register(SDL3_Engine))
 *   - Component types (SDL3_ContextConfig)
 *   - State singletons (SDL3_ContextState)
 *   - Composition (SDL3Context call macro)
 *
 * Does NOT re-export <cels/cels.h> -- include that separately.
 */

#ifndef CELS_SDL3_H
#define CELS_SDL3_H
#include <cels/cels.h>
#include <stdbool.h>

CEL_Module(SDL3_Engine);

/* ============================================================================
 * Component Types
 * ============================================================================ */

/*
 * Developer sets this on an entity to configure SDL3 initialization.
 * An observer reacts to this component being added and initializes SDL3.
 *
 *   SDL3Context(.video = true) {}
 *
 * video: initialize SDL_INIT_VIDEO (implies SDL_INIT_EVENTS)
 */
CEL_Component(SDL3_ContextConfig) {
    bool video;
};

/* ============================================================================
 * State Singletons
 * ============================================================================
 *
 * CEL_State type with cross-TU pointer registry.
 * Writer TU (sdl3_init.c) owns the data.
 * Consumers read via cel_read(SDL3_ContextState).
 */

/*
 * SDL3 context state singleton. Tracks initialization status.
 * Read via cel_read(SDL3_ContextState) in consumer systems.
 */
CEL_Define_State(SDL3_ContextState) {
    bool initialized;
    bool ttf_ready;
};

/* ============================================================================
 * Composition: SDL3Context
 * ============================================================================
 *
 * Public composition exported via CEL_Define_Composition. Developer creates
 * SDL3 context entities with natural syntax:
 *
 *   SDL3Context(.video = true) {}
 *
 * Implementation in sdl3_module.c via CEL_Composition(SDL3Context).
 */
CEL_Define_Composition(SDL3Context, bool video;);

/* Call macro for natural syntax */
#define SDL3Context(...) cel_init(SDL3Context, __VA_ARGS__)

#endif /* CELS_SDL3_H */
