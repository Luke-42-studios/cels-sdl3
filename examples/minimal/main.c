/*
 * SDL3 Window Provider -- Lifecycle Verification
 *
 * Exercises the complete window provider from Phase 2:
 *   - Two windows via SDL3Window composition (multi-window coexistence)
 *   - Consumer system queries SDL3_WindowComponent entities
 *   - State verification (both windows reach READY)
 *   - Clean destruction via session scope exit
 *
 * Runs under SDL_VIDEODRIVER=dummy (headless, no display required).
 */

#include <cels/cels.h>
#include <cels_sdl3.h>
#include <SDL3/SDL.h>
#include <stdio.h>

/* ============================================================================
 * World: two windows with different configurations
 * ============================================================================ */

CEL_Compose(World) {
    SDL3Context(.video = true) {}
    SDL3Window(.title = "Main Window", .width = 800, .height = 600) {}
    SDL3Window(.title = "Debug View") {}  /* gets 1280x720 default */
}

/* ============================================================================
 * Verification system -- queries window entities and validates state
 * ============================================================================ */

static int s_verified = 0;

CEL_System(WindowVerifier, .phase = OnLoad) {
    if (s_verified) return;

    cel_query(SDL3_WindowComponent);

    /* First, print header before iterating */
    int window_count = 0;
    int all_ready = 1;
    int header_printed = 0;

    cel_each(SDL3_WindowComponent) {
        if (!header_printed) {
            printf("\n=== SDL3 Window Provider Test ===\n");
            header_printed = 1;
        }

        window_count++;
        const char* title = SDL_GetWindowTitle(SDL3_WindowComponent->window);
        int w = SDL3_WindowComponent->width;
        int h = SDL3_WindowComponent->height;
        SDL_WindowID wid = SDL3_WindowComponent->window_id;
        SDL3_WindowState state = SDL3_WindowComponent->state;

        printf("Window %d: \"%s\" %dx%d id=%u state=%s\n",
               window_count,
               title ? title : "(null)",
               w, h,
               (unsigned)wid,
               state == SDL3_WINDOW_READY ? "READY" : "NOT_READY");

        if (state != SDL3_WINDOW_READY) {
            all_ready = 0;
        }
    }

    if (window_count > 0) {
        if (window_count == 2 && all_ready) {
            printf("PASS: 2 windows created, all READY\n");
        } else {
            printf("FAIL: expected 2 READY windows, got %d windows (all_ready=%d)\n",
                   window_count, all_ready);
        }
        s_verified = 1;
    }
}

/* ============================================================================
 * Frame counter system -- runs fixed number of frames then exits
 * ============================================================================ */

static int s_frames = 0;

CEL_System(FrameCounter, .phase = OnUpdate) {
    cel_run {
        s_frames++;
        SDL_Delay(16);  /* ~60 fps pacing */
        if (s_frames >= 180) {  /* ~3 seconds visible */
            cels_quit();
        }
    }
}

/* ============================================================================
 * Main -- wire up and run
 * ============================================================================ */

cels_main() {
    cels_register(SDL3_Engine);
    cels_register(WindowVerifier, FrameCounter);
    cels_session(World) {
        while (cels_running()) {
            cels_step(0.016f);
        }
    }
    printf("Session complete -- windows destroyed via scope exit\n");
}
