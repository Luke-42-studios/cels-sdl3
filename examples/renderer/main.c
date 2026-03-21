/*
 * SDL3 Renderer Demo -- Clear Color Control via Keyboard
 *
 * Exercises the renderer core from Phase 5:
 *   - SDL3_Renderer per-window renderer with clear_color
 *   - RenderClearSystem (PreRender) and RenderPresentSystem (PostRender)
 *   - Consumer system reads SDL3_EventQueue to change clear_color at runtime
 *   - R/G/B/C keys set clear color (Red, Green, Blue, Cornflower blue)
 *   - Escape key closes the window via cel_quit()
 *   - Clean exit when window is closed
 *
 * Runs with a real display for interactive color changing.
 * Under SDL_VIDEODRIVER=dummy: starts, loops, exits without crashes
 * (but no input events are generated).
 */

#include <cels/cels.h>
#include <cels_sdl3.h>
#include <stdio.h>

/* ============================================================================
 * World: single window for renderer demo
 * ============================================================================ */

CEL_Compose(World) {
    SDL3Window(.title = "Renderer Demo", .width = 800, .height = 600,
               .context = true, .target_fps = 60) {}
}

/* ============================================================================
 * ColorChanger system -- reads events and changes renderer clear color
 * ============================================================================
 *
 * Demonstrates the consumer pattern for modifying renderer state:
 *   1. Query both SDL3_EventQueue and SDL3_Renderer
 *   2. Iterate events[0..count-1] with a plain for-loop
 *   3. On key press, set clear_color and cel_update(SDL3_Renderer)
 *
 * The clear_color change takes effect next frame when RenderClearSystem
 * reads it during the PreRender phase.
 */

CEL_System(ColorChanger, .phase = OnUpdate) {
    cel_query(SDL3_EventQueue, SDL3_Renderer);
    cel_each(SDL3_EventQueue, SDL3_Renderer) {
        SDL_Color new_color = {0};
        const char* color_name = NULL;
        bool should_quit = false;

        for (int i = 0; i < SDL3_EventQueue->count; i++) {
            const SDL_Event* ev = &SDL3_EventQueue->events[i];

            if (ev->type != SDL_EVENT_KEY_DOWN || ev->key.repeat) {
                continue;
            }

            switch (ev->key.key) {

            case SDLK_R:
                new_color = (SDL_Color){255, 0, 0, 255};
                color_name = "red";
                break;

            case SDLK_G:
                new_color = (SDL_Color){0, 255, 0, 255};
                color_name = "green";
                break;

            case SDLK_B:
                new_color = (SDL_Color){0, 0, 255, 255};
                color_name = "blue";
                break;

            case SDLK_C:
                new_color = (SDL_Color){100, 149, 237, 255};
                color_name = "cornflower blue";
                break;

            case SDLK_ESCAPE:
                printf("Escape pressed -- closing\n");
                should_quit = true;
                break;

            default:
                break;
            }
        }

        if (color_name) {
            cel_update(SDL3_Renderer) {
                SDL3_Renderer->clear_color = new_color;
            }
            printf("Color: %s\n", color_name);
        }

        if (should_quit) {
            cel_quit();
        }
    }
}

/* ============================================================================
 * Main -- canonical consumer loop pattern
 * ============================================================================ */

cels_main() {
    cels_register(SDL3_Engine);
    cels_register(ColorChanger);
    cels_session(World) {
        while (sdl3_should_run()) {
            float dt = sdl3_delta();
            cels_step(dt);
        }
    }
    printf("Renderer demo complete -- clean exit\n");
}
