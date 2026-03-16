/*
 * SDL3 Frame Loop -- Delta Time and FPS Demonstration
 *
 * Exercises the complete frame loop from Phase 3:
 *   - Real delta time via SDL_GetPerformanceCounter (not hardcoded 0.016f)
 *   - FPS reporting from SDL3_FrameState (smoothed EMA)
 *   - Event pump system draining SDL events each frame
 *   - Clean exit via cel_quit() after 300 frames
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
    SDL3Context(.video = true) {}
    SDL3Window(.title = "Frame Loop Demo", .width = 800, .height = 600) {}
}

/* ============================================================================
 * FPS Reporter system -- prints FPS periodically
 * ============================================================================
 *
 * Uses two strategies to ensure output in both capped and uncapped modes:
 *   1. Time-based: print every ~1 second of accumulated delta (for capped)
 *   2. Frame-based: print every 100 frames (for uncapped, where total
 *      runtime may be <1 second)
 */

CEL_System(FPSReporter, .phase = OnUpdate) {
    cel_run {
        static float accumulator  = 0.0f;
        static int   frame_count  = 0;

        float dt = cel_delta();
        accumulator += dt;
        frame_count++;

        bool report = false;

        /* Time-based reporting: every ~1 second */
        if (accumulator >= 1.0f) {
            accumulator -= 1.0f;
            report = true;
        }

        /* Frame-based reporting: every 100 frames (catches uncapped runs) */
        if (frame_count % 100 == 0) {
            report = true;
        }

        if (report) {
            const struct SDL3_FrameState* frame = cel_read(SDL3_FrameState);
            printf("FPS: %.1f (dt: %.6fs)\n",
                   frame->smoothed_fps, frame->delta_time);
        }
    }
}

/* ============================================================================
 * Frame Counter system -- exits after 300 frames
 * ============================================================================ */

CEL_System(FrameCounter, .phase = OnUpdate) {
    cel_run {
        static int frames = 0;
        frames++;

        if (frames >= 300) {
            printf("Frame loop complete: %d frames\n", frames);
            cel_quit();
        }
    }
}

/* ============================================================================
 * Main -- canonical consumer loop pattern
 * ============================================================================ */

cels_main() {
    cels_register(SDL3_Engine);
    cels_register(FPSReporter, FrameCounter);
    cels_session(World) {
        while (sdl3_should_run()) {
            float dt = sdl3_delta();
            cels_step(dt);
        }
    }
    printf("Session complete -- clean exit\n");
}
