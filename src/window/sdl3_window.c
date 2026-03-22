/*
 * SDL3 Window - Creation, Destruction, State Machine
 *
 * Called by lifecycle observers in sdl3_module.c.
 * CANNOT use cel_watch/cel_has (per-TU static ID constraint).
 * Receives entity + component_id as explicit parameters.
 */

#include <cels_sdl3.h>
#include "../sdl3_internal.h"
#include <SDL3/SDL.h>

void sdl3_window_create(cels_entity_t entity,
                         const SDL3_WindowConfig* config,
                         cels_entity_t component_id)
{
    const char* title = (config->title && config->title[0])
                        ? config->title : "";
    int w = config->width  > 0 ? config->width  : 1280;
    int h = config->height > 0 ? config->height : 720;
    SDL_WindowFlags flags = config->flags;

    /* Resizable by default unless borderless is set */
    if (!(flags & SDL_WINDOW_BORDERLESS)) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    SDL_Window* window = SDL_CreateWindow(title, w, h, flags);
    if (!window) {
        sdl3_report_error("create_window");
        return;
    }

    /* Commit an initial surface buffer so Wayland compositors display the window,
     * then explicitly show it. Without a committed buffer, Wayland will
     * not composite the window even though SDL marks it as "shown". */
    SDL_Surface* surface = SDL_GetWindowSurface(window);
    if (surface) {
        SDL_FillSurfaceRect(surface, NULL, SDL_MapSurfaceRGB(surface, 0, 0, 0));
        SDL_UpdateWindowSurface(window);
    }
    SDL_ShowWindow(window);

    /* Build runtime component.
     * State advances synchronously: NONE -> CREATED -> SURFACE_READY -> READY.
     * SURFACE_READY is an instant pass-through for the SDL_Renderer path
     * (becomes meaningful in a future SDL_GPU path). */
    SDL3_WindowComponent comp = {
        .window        = window,
        .window_id     = SDL_GetWindowID(window),
        .state         = SDL3_WINDOW_READY,
        .width         = w,
        .height        = h,
        .context_bound = config->context
    };

    cels_entity_set_component(entity, component_id, &comp, sizeof(comp));
    cels_component_notify_change(component_id);
}

void sdl3_window_destroy(SDL_Window* window)
{
    if (!window) return;
    SDL_DestroyWindow(window);
}

void sdl3_window_handle_event(SDL3_WindowComponent* comp,
                               Uint32 event_type,
                               int data1, int data2)
{
    switch (event_type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (comp->state != SDL3_WINDOW_CLOSED) {
            comp->state = SDL3_WINDOW_CLOSING;
        }
        break;

    case SDL_EVENT_WINDOW_MINIMIZED:
        if (comp->state == SDL3_WINDOW_READY) {
            comp->state = SDL3_WINDOW_MINIMIZED;
        }
        break;

    case SDL_EVENT_WINDOW_RESTORED:
        if (comp->state == SDL3_WINDOW_MINIMIZED) {
            /* Synchronous restore chain: MINIMIZED -> SURFACE_READY -> READY */
            comp->state = SDL3_WINDOW_SURFACE_READY;
            comp->state = SDL3_WINDOW_READY;
        }
        break;

    case SDL_EVENT_WINDOW_RESIZED:
        if (comp->state == SDL3_WINDOW_READY ||
            comp->state == SDL3_WINDOW_RESIZING) {
            comp->state = SDL3_WINDOW_RESIZING;
            comp->width  = data1;
            comp->height = data2;
        }
        break;

    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        /* Routed for completeness; no state machine transition */
        break;

    default:
        /* Informational events (expose, etc.) -- no state change */
        break;
    }
}
