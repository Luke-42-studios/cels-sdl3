/*
 * SDL3 Loop - Delta Time, FPS Tracking, Frame Rate Capping
 *
 * Provides frame timing helpers called by sdl3_module.c systems.
 * Owns the canonical SDL3_FrameState data (same pattern as
 * SDL3_ContextState in sdl3_init.c).
 *
 * CANNOT use CEL macros (per-TU static ID constraint).
 * All CELS declarations live in sdl3_module.c.
 */

#include <cels_sdl3.h>
#include "../sdl3_internal.h"
#include <SDL3/SDL.h>

/* ============================================================================
 * SDL3_FrameState -- owned by this TU
 * ============================================================================ */

static struct SDL3_FrameState SDL3_FrameState = {
    .running      = true,
    .delta_time   = 0.0f,
    .fps          = 0.0f,
    .smoothed_fps = 0.0f
};

/* ============================================================================
 * Delta Time Computation
 * ============================================================================ */

/* Maximum delta clamp: 250ms (4 FPS minimum) -- survives debugger pauses */
#define SDL3_MAX_DELTA 0.25f

static Uint64 s_perf_frequency = 0;
static Uint64 s_last_counter   = 0;
static bool   s_initialized    = false;

void sdl3_loop_init(void) {
    s_perf_frequency = SDL_GetPerformanceFrequency();
    s_last_counter   = SDL_GetPerformanceCounter();
    s_initialized    = true;

    /* Register state with CELS cross-TU pointer registry */
    SDL3_FrameState_register();
    cels_state_bind(SDL3_FrameState);
}

float sdl3_compute_delta(void) {
    if (!s_initialized) {
        sdl3_loop_init();
        return 0.0f;  /* first frame: zero delta */
    }

    Uint64 now = SDL_GetPerformanceCounter();
    /* Cast to float BEFORE dividing to avoid integer division producing 0 */
    float dt = (float)(now - s_last_counter) / (float)s_perf_frequency;
    s_last_counter = now;

    /* Clamp to prevent explosion after debugger pause */
    if (dt > SDL3_MAX_DELTA) dt = SDL3_MAX_DELTA;
    if (dt < 0.0f) dt = 0.0f;  /* guard against counter wraparound */

    return dt;
}

/* ============================================================================
 * FPS Tracking
 * ============================================================================ */

#define SDL3_FPS_SMOOTHING 0.1f  /* 10% new, 90% old -- smooth for display */

static float s_smoothed_fps = 0.0f;

float sdl3_compute_fps(float dt) {
    /* Guard only against zero/negative dt (first frame, counter wraparound).
     * FLT_MIN (~1.2e-38) allows FPS tracking at any real frame rate. */
    float raw_fps = (dt > 0.0f) ? (1.0f / dt) : 0.0f;
    s_smoothed_fps = SDL3_FPS_SMOOTHING * raw_fps
                   + (1.0f - SDL3_FPS_SMOOTHING) * s_smoothed_fps;
    return s_smoothed_fps;
}

/* ============================================================================
 * Frame Rate Capping
 * ============================================================================ */

void sdl3_cap_frame_rate(Uint64 frame_start_ns, int target_fps) {
    if (target_fps <= 0) return;  /* no cap / VSync mode */

    Uint64 target_ns  = SDL_NS_PER_SECOND / (Uint64)target_fps;
    Uint64 elapsed_ns = SDL_GetTicksNS() - frame_start_ns;

    if (elapsed_ns < target_ns) {
        SDL_DelayNS(target_ns - elapsed_ns);
    }
}

/* ============================================================================
 * Public API -- Consumer Loop Functions
 * ============================================================================ */

bool sdl3_should_run(void) {
    return SDL3_FrameState.running && cels_running();
}

float sdl3_delta(void) {
    float dt  = sdl3_compute_delta();
    float fps = sdl3_compute_fps(dt);

    /* Update frame state for systems to read via cel_read(SDL3_FrameState) */
    SDL3_FrameState.delta_time   = dt;
    SDL3_FrameState.fps          = (dt > 0.0f) ? (1.0f / dt) : 0.0f;
    SDL3_FrameState.smoothed_fps = fps;

    return dt;
}

/* ============================================================================
 * Frame State Mutators -- called from sdl3_module.c systems
 * ============================================================================ */

void sdl3_frame_set_running(bool running) {
    SDL3_FrameState.running = running;
}
