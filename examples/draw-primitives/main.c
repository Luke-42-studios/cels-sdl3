/*
 * SDL3 Draw Primitives Demo -- Z-Ordered Shape Rendering
 *
 * Exercises the draw primitives pipeline from Phase 6:
 *   - SDL3_Renderable vtable (filled_rect, outlined_rect, line)
 *   - Per-window draw buffer with z-index sorting
 *   - DrawFlushSystem sorts by (z ascending, creation order ascending)
 *
 * Draws overlapping shapes at z-indices 0 through 3:
 *   z=0: Dark background panel + grid lines (grid after panel, same z)
 *   z=1: Colored filled rectangles (red, green, blue -- overlapping)
 *   z=2: White outlined rectangles around the filled ones
 *   z=3: Yellow diagonal crosshair lines across the full window
 *
 * Visual proof of z-ordering:
 *   - Grid lines render over the background panel (same z, later creation)
 *   - Filled rects render over the grid (z=1 > z=0)
 *   - Outlines render over the filled rects (z=2 > z=1)
 *   - Crosshair lines render on top of everything (z=3 > z=2)
 *
 * Escape key exits cleanly.
 * Under SDL_VIDEODRIVER=dummy: starts, loops, exits without crashes.
 */

#include <cels/cels.h>
#include <cels_sdl3.h>
#include <stdio.h>

/* ============================================================================
 * World: single window for draw primitives demo
 * ============================================================================ */

CEL_Compose(World) {
    SDL3Window(.title = "Draw Primitives", .width = 800, .height = 600,
               .context = true, .target_fps = 60) {}
}

/* ============================================================================
 * ShapeRenderer system -- draws shapes through the Renderable vtable
 * ============================================================================
 *
 * Demonstrates the draw primitive API:
 *   1. Get vtable via sdl3_renderable()
 *   2. Get SDL_Renderer* from SDL3_Renderer component
 *   3. Call draw->filled_rect(), draw->outlined_rect(), draw->line()
 *   4. Each call buffers a draw command with z-index
 *   5. DrawFlushSystem sorts by z and issues SDL3 calls before present
 *
 * No cel_update needed -- vtable functions handle buffering internally.
 */

CEL_System(ShapeRenderer, .phase = OnRender) {
    const SDL3_Renderable* draw = sdl3_renderable();

    cel_query(SDL3_Renderer);
    cel_each(SDL3_Renderer) {
        SDL_Renderer* r = SDL3_Renderer->renderer;
        if (!r) continue;

        /* ----------------------------------------------------------------
         * z=0: Background panel -- large dark gray filled rect
         * ---------------------------------------------------------------- */
        draw->filled_rect(r,
            (SDL_FRect){100, 50, 600, 500},
            (SDL_Color){40, 40, 40, 255}, 0);

        /* ----------------------------------------------------------------
         * z=0: Grid lines -- same z as background, but created after,
         *      so they render on top (creation order tiebreaker)
         * ---------------------------------------------------------------- */
        SDL_Color grid_color = {60, 60, 60, 255};

        /* Horizontal grid lines */
        for (int y = 100; y <= 500; y += 50) {
            draw->line(r,
                (SDL_FPoint){100, (float)y},
                (SDL_FPoint){700, (float)y},
                grid_color, 0);
        }

        /* Vertical grid lines */
        for (int x = 150; x <= 650; x += 50) {
            draw->line(r,
                (SDL_FPoint){(float)x, 50},
                (SDL_FPoint){(float)x, 550},
                grid_color, 0);
        }

        /* ----------------------------------------------------------------
         * z=1: Colored filled rectangles -- overlapping to show z-order
         *      within the same z-level (creation order decides)
         * ---------------------------------------------------------------- */
        draw->filled_rect(r,
            (SDL_FRect){150, 100, 180, 120},
            (SDL_Color){220, 50, 50, 255}, 1);   /* Red */

        draw->filled_rect(r,
            (SDL_FRect){350, 200, 180, 120},
            (SDL_Color){50, 100, 220, 255}, 1);   /* Blue */

        draw->filled_rect(r,
            (SDL_FRect){250, 150, 180, 120},
            (SDL_Color){50, 180, 50, 255}, 1);    /* Green -- overlaps red and blue */

        /* ----------------------------------------------------------------
         * z=2: Outlined rectangles -- white borders around each filled rect
         *      Renders on top of filled rects (z=2 > z=1)
         * ---------------------------------------------------------------- */
        SDL_Color outline_color = {255, 255, 255, 255};

        draw->outlined_rect(r,
            (SDL_FRect){150, 100, 180, 120},
            outline_color, 2);   /* Around red */

        draw->outlined_rect(r,
            (SDL_FRect){350, 200, 180, 120},
            outline_color, 2);   /* Around blue */

        draw->outlined_rect(r,
            (SDL_FRect){250, 150, 180, 120},
            outline_color, 2);   /* Around green */

        /* ----------------------------------------------------------------
         * z=3: Crosshair lines -- yellow diagonals across the full window
         *      Renders on top of everything (highest z)
         * ---------------------------------------------------------------- */
        SDL_Color crosshair_color = {255, 220, 0, 255};

        draw->line(r,
            (SDL_FPoint){0, 0},
            (SDL_FPoint){800, 600},
            crosshair_color, 3);   /* Top-left to bottom-right */

        draw->line(r,
            (SDL_FPoint){800, 0},
            (SDL_FPoint){0, 600},
            crosshair_color, 3);   /* Top-right to bottom-left */
    }
}

/* ============================================================================
 * InputHandler system -- reads events, exits on Escape
 * ============================================================================ */

CEL_System(InputHandler, .phase = OnUpdate) {
    cel_query(SDL3_EventQueue);
    cel_each(SDL3_EventQueue) {
        for (int i = 0; i < SDL3_EventQueue->count; i++) {
            const SDL_Event* ev = &SDL3_EventQueue->events[i];

            if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat &&
                ev->key.key == SDLK_ESCAPE) {
                printf("Escape pressed -- closing\n");
                cel_quit();
                return;
            }
        }
    }
}

/* ============================================================================
 * Main -- canonical consumer loop pattern
 * ============================================================================ */

cels_main() {
    cels_register(SDL3_Engine);
    SDL3_Renderable_use();
    cels_register(ShapeRenderer);
    cels_register(InputHandler);
    cels_session(World) {
        while (sdl3_should_run()) {
            float dt = sdl3_delta();
            cels_step(dt);
        }
    }
    printf("Draw primitives demo complete -- clean exit\n");
}
