/*
 * SDL3 Renderer - Creation, Destruction
 *
 * Called by lifecycle observers and systems in sdl3_module.c.
 * CANNOT use cel_watch/cel_has (per-TU static ID constraint).
 * Receives entity + component_id as explicit parameters.
 */

#include <cels_sdl3.h>
#include "../sdl3_internal.h"

void sdl3_renderer_create(cels_entity_t entity,
                            SDL_Window* window,
                            cels_entity_t component_id)
{
    /* Must destroy window surface first -- surface and renderer are mutually
     * exclusive on the same window. SDL_GetWindowSurface was used in
     * sdl3_window_create to commit an initial buffer for Wayland visibility. */
    SDL_DestroyWindowSurface(window);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("SDL3: SDL_CreateRenderer failed: %s", SDL_GetError());
        return;
    }

    SDL3_Renderer comp = {
        .renderer    = renderer,
        .clear_color = { .r = 100, .g = 149, .b = 237, .a = 255 }
    };
    sdl3_draw_buffer_init(&comp);
    cels_entity_set_component(entity, component_id, &comp, sizeof(comp));
}

void sdl3_renderer_destroy(SDL_Renderer* renderer)
{
    if (!renderer) return;
    SDL_DestroyRenderer(renderer);
}
