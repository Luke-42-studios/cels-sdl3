/*
 * SDL3 Frame Loop -- Delta Time and FPS Demonstration
 *
 * Exercises the complete frame loop from Phase 3:
 *   - Real delta time via SDL_GetPerformanceCounter (not hardcoded 0.016f)
 *   - FPS reporting from SDL3_FrameState (smoothed EMA)
 *   - Event pump system draining SDL events each frame
 *   - Clean exit when window is closed (X button or SDL_QUIT)
 *   - Consumer loop pattern: sdl3_should_run() + sdl3_delta() + cels_step(dt)
 *
 * Runs under SDL_VIDEODRIVER=dummy (headless) or with a real display.
 * No SDL_Delay -- loop runs uncapped to demonstrate real timing measurement.
 */

#include <cels/cels.h>
#include <cels_sdl3.h>
#include <stdio.h>

/* ============================================================================
 * World: single window for frame loop demo
 * ============================================================================ */

CEL_Compose(World) {
    SDL3Window(.title = "Frame Loop Demo", .width = 800, .height = 600, .context = true, .target_fps = 240) {}
}

/* ============================================================================
 * FPS Reporter system -- prints FPS periodically
 * ============================================================================
 *
 * Prints FPS every ~1 second of accumulated delta time.
 */

CEL_System(FPSReporter, .phase = OnUpdate) {
    cel_run {
        static float accumulator = 0.0f;

        float dt = cel_delta();
        accumulator += dt;

        if (accumulator >= 1.0f) {
            accumulator -= 1.0f;
            const struct SDL3_FrameState* frame = cel_read(SDL3_FrameState);
            printf("FPS: %.1f (dt: %.6fs)\n",
                   frame->smoothed_fps, frame->delta_time);
        }
    }
}

/* ============================================================================
 * Main -- canonical consumer loop pattern
 * ============================================================================ */

cels_main() {
    cels_register(SDL3_Engine);
    cels_register(FPSReporter);
    cels_session(World) {
        while (sdl3_should_run()) {
            float dt = sdl3_delta();
            cels_step(dt);
        }
    }
    printf("Session complete -- clean exit\n");
}
