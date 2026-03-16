/*
 * SDL3 Init/Shutdown Implementation
 *
 * Provides sdl3_init() and sdl3_shutdown() called by lifecycle observers
 * in sdl3_module.c. Owns the canonical SDL3_ContextState data.
 *
 * Init order:  SDL_Init(SDL_INIT_VIDEO) -> TTF_Init()
 * Shutdown order: TTF_Quit() -> SDL_Quit()  (reverse)
 *
 * NOTE: SDL3_image has removed IMG_Init()/IMG_Quit() entirely.
 * The library auto-initializes when loading images. Do NOT call them.
 */

#include <cels_sdl3.h>
#include "sdl3_internal.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

/* ============================================================================
 * Static State
 * ============================================================================ */

/* CEL_State(SDL3_ContextState) -- owned by this TU */
static struct SDL3_ContextState SDL3_ContextState = {
    .initialized = false,
    .ttf_ready = false
};

/* ============================================================================
 * Init
 * ============================================================================ */

void sdl3_init(const SDL3_ContextConfig* config) {
    /* Prevent double-init */
    if (SDL3_ContextState.initialized) return;

    /* Register state with CELS cross-TU pointer registry */
    SDL3_ContextState_register();
    cels_state_bind(SDL3_ContextState);

    /* Initialize SDL3 VIDEO subsystem (implies EVENTS) */
    if (config->video) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            SDL_Log("SDL_Init failed: %s", SDL_GetError());
            return;
        }
    }

    /* Initialize SDL3_ttf */
    if (!TTF_Init()) {
        SDL_Log("TTF_Init failed: %s", SDL_GetError());
        SDL_Quit();
        return;
    }

    SDL3_ContextState.initialized = true;
    SDL3_ContextState.ttf_ready = true;
}

/* ============================================================================
 * Ensure Init -- auto-init with defaults if not yet initialized
 * ============================================================================ */

void sdl3_ensure_init(void) {
    if (SDL3_ContextState.initialized) return;
    SDL3_ContextConfig default_config = { .video = true };
    sdl3_init(&default_config);
}

/* ============================================================================
 * Shutdown
 * ============================================================================ */

void sdl3_shutdown(void) {
    if (!SDL3_ContextState.initialized) return;

    /* Shutdown in reverse init order */
    TTF_Quit();     /* TTF first -- depends on SDL internals */
    /* NOTE: No IMG_Quit() -- SDL3_image has no quit function */
    SDL_Quit();     /* SDL last */

    SDL3_ContextState.initialized = false;
    SDL3_ContextState.ttf_ready = false;
}
