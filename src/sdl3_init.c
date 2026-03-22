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

/* Error callback -- NULL means use default SDL_Log handler */
static SDL3_ErrorCallback s_error_callback = NULL;

/* Configurable init flags -- 0 means SDL_INIT_VIDEO */
static Uint32 s_init_flags = 0;

void sdl3_set_error_callback(SDL3_ErrorCallback callback) {
    s_error_callback = callback;
}

void sdl3_set_init_flags(Uint32 flags) {
    s_init_flags = flags;
}

void sdl3_report_error(const char* context) {
    const char* msg = SDL_GetError();
    if (!msg || msg[0] == '\0') {
        msg = "(no SDL error)";
    }
    if (s_error_callback) {
        s_error_callback(context, msg);
    } else {
        SDL_Log("SDL3 error [%s]: %s", context, msg);
    }
}

/* ============================================================================
 * Init
 * ============================================================================ */

void sdl3_init(const SDL3_ContextConfig* config) {
    /* Prevent double-init */
    if (SDL3_ContextState.initialized) return;

    /* Register state with CELS cross-TU pointer registry */
    SDL3_ContextState_register();
    cels_state_bind(SDL3_ContextState);

    /* Determine init flags: explicit flags take priority over config->video */
    Uint32 flags = s_init_flags;
    if (flags == 0 && config->video) {
        flags = SDL_INIT_VIDEO;
    }

    if (flags) {
        if (!SDL_Init(flags)) {
            sdl3_report_error("sdl_init");
            return;
        }
    }

    /* Initialize SDL3_ttf */
    if (!TTF_Init()) {
        sdl3_report_error("ttf_init");
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

    /* Close all fonts before shutting down TTF */
    sdl3_fonts_close_all();

    /* Shutdown in reverse init order */
    TTF_Quit();     /* TTF first -- depends on SDL internals */
    /* NOTE: No IMG_Quit() -- SDL3_image has no quit function */
    SDL_Quit();     /* SDL last */

    SDL3_ContextState.initialized = false;
    SDL3_ContextState.ttf_ready = false;
}
