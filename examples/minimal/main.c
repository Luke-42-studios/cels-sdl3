/*
 * SDL3 Window Watch Demo -- cel_watch Reactivity
 *
 * Demonstrates using cel_watch(entity, Component) in a composition to
 * reactively observe window state changes and resize events:
 *
 *   - cel_watch(entity, SDL3_WindowComponent) triggers recomposition
 *     when the window component changes (resize, state transition)
 *   - cel_remember tracks previous values for change detection
 *   - Debug logs show exactly when state transitions and resizes occur
 *
 * Try: resize the window, minimize/restore, close with Escape or X button.
 */

#include <cels/cels.h>
#include <cels_sdl3.h>
#include <stdio.h>

/* ============================================================================
 * Window state name helper
 * ============================================================================ */

static const char* state_name(int state)
{
    switch (state) {
    case SDL3_WINDOW_NONE:          return "NONE";
    case SDL3_WINDOW_CREATED:       return "CREATED";
    case SDL3_WINDOW_SURFACE_READY: return "SURFACE_READY";
    case SDL3_WINDOW_READY:         return "READY";
    case SDL3_WINDOW_RESIZING:      return "RESIZING";
    case SDL3_WINDOW_MINIMIZED:     return "MINIMIZED";
    case SDL3_WINDOW_CLOSING:       return "CLOSING";
    case SDL3_WINDOW_CLOSED:        return "CLOSED";
    default:                        return "UNKNOWN";
    }
}

/* ============================================================================
 * World composition -- watches window entity for reactive debug logging
 * ============================================================================ */

CEL_Compose(World) {
    cels_entity_t* win_id   = cel_remember(cels_entity_t, 0);
    int*           prev_w   = cel_remember(int, 0);
    int*           prev_h   = cel_remember(int, 0);
    int*           prev_st  = cel_remember(int, -1);

    SDL3Window(.title = "Window Watch Demo", .width = 800, .height = 600,
               .context = true, .target_fps = 60) {
        *win_id = cels_get_current_entity();
    }

    if (*win_id) {
        const SDL3_WindowComponent* wc =
            cel_watch(*win_id, SDL3_WindowComponent);

        if (wc) {
            /* Log state transitions */
            if ((int)wc->state != *prev_st) {
                printf("[watch] state: %s -> %s\n",
                       *prev_st < 0 ? "(init)" : state_name(*prev_st),
                       state_name((int)wc->state));
                *prev_st = (int)wc->state;
            }

            /* Log resize events */
            if (wc->width != *prev_w || wc->height != *prev_h) {
                if (*prev_w > 0) {
                    printf("[watch] resize: %dx%d -> %dx%d\n",
                           *prev_w, *prev_h, wc->width, wc->height);
                } else {
                    printf("[watch] initial size: %dx%d\n",
                           wc->width, wc->height);
                }
                *prev_w = wc->width;
                *prev_h = wc->height;
            }
        }
    }
}

/* ============================================================================
 * InputHandler -- Escape to close
 * ============================================================================ */

CEL_System(InputHandler, .phase = OnUpdate) {
    cel_query(SDL3_EventQueue);
    cel_each(SDL3_EventQueue) {
        for (int i = 0; i < SDL3_EventQueue->count; i++) {
            const SDL_Event* ev = &SDL3_EventQueue->events[i];
            if (ev->type == SDL_EVENT_KEY_DOWN &&
                ev->key.key == SDLK_ESCAPE) {
                printf("[input] Escape pressed -- closing\n");
                cel_quit();
            }
        }
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

cels_main() {
    cels_register(SDL3_Engine);
    cels_register(InputHandler);
    cels_session(World) {
        while (sdl3_should_run()) {
            float dt = sdl3_delta();
            cels_step(dt);
        }
    }
    printf("Session complete -- clean exit\n");
}
