/*
 * SDL3 Input Demo -- Keyboard and Mouse Event Reading
 *
 * Exercises the input system from Phase 4:
 *   - SDL3_EventQueue per-window event buffer
 *   - Consumer system reads raw SDL_Event structs via for-loop + switch
 *   - Escape key closes the window via cel_quit()
 *   - Key press/release events logged to console
 *   - Mouse button events logged with position
 *   - Mouse motion logged periodically (not every event)
 *   - Clean exit when window is closed
 *
 * Runs with a real display for interactive input testing.
 * Under SDL_VIDEODRIVER=dummy: starts, loops, exits without crashes
 * (but no input events are generated).
 */

#include <cels/cels.h>
#include <cels_sdl3.h>
#include <stdio.h>

/* ============================================================================
 * World: single window for input demo
 * ============================================================================ */

CEL_Compose(World) {
    SDL3Window(.title = "Input Demo", .width = 800, .height = 600,
               .context = true, .target_fps = 60) {}
}

/* ============================================================================
 * InputLogger system -- reads SDL3_EventQueue and logs events
 * ============================================================================
 *
 * Demonstrates the consumer pattern for reading input events:
 *   1. Query SDL3_EventQueue component
 *   2. Iterate events[0..count-1] with a plain for-loop
 *   3. Switch on event.type for each event of interest
 *
 * No convenience macros -- this IS the intended consumer pattern.
 */

CEL_System(InputLogger, .phase = OnUpdate) {
    static int motion_counter = 0;

    cel_query(SDL3_EventQueue);
    cel_each(SDL3_EventQueue) {
        for (int i = 0; i < SDL3_EventQueue->count; i++) {
            const SDL_Event* ev = &SDL3_EventQueue->events[i];

            switch (ev->type) {

            case SDL_EVENT_KEY_DOWN:
                if (ev->key.key == SDLK_ESCAPE) {
                    printf("Escape pressed -- closing\n");
                    cel_quit();
                } else if (!ev->key.repeat) {
                    printf("Key pressed: %s\n",
                           SDL_GetKeyName(ev->key.key));
                }
                break;

            case SDL_EVENT_KEY_UP:
                if (!ev->key.repeat) {
                    printf("Key released: %s\n",
                           SDL_GetKeyName(ev->key.key));
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                printf("Mouse button %d pressed at (%.0f, %.0f)\n",
                       ev->button.button, ev->button.x, ev->button.y);
                break;

            case SDL_EVENT_MOUSE_MOTION:
                motion_counter++;
                if (motion_counter % 30 == 0) {
                    printf("Mouse position: (%.0f, %.0f)\n",
                           ev->motion.x, ev->motion.y);
                }
                break;

            default:
                break;
            }
        }
    }
}

/* ============================================================================
 * Main -- canonical consumer loop pattern
 * ============================================================================ */

cels_main() {
    cels_register(SDL3_Engine);
    cels_register(InputLogger);
    cels_session(World) {
        while (sdl3_should_run()) {
            float dt = sdl3_delta();
            cels_step(dt);
        }
    }
    printf("Input demo complete -- clean exit\n");
}
