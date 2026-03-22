/*
 * cels-sdl3 Demo -- Full Feature Showcase
 *
 * Exercises every v1 feature of cels-sdl3 in a cohesive mini landscape scene:
 *   - Colored window: sky-blue clear color (135, 206, 235)
 *   - Input handling: Escape closes, mouse clicks place shapes
 *   - Draw primitives: ground (filled rect), horizon (line), sun + building
 *     (outlined rect + filled rect)
 *   - Texture: bundled tree.png loaded and rendered via SDL3_Sprite
 *   - Text: title, live FPS counter, input instructions via SDL3_Text
 *
 * Interactive: click anywhere to plant colored shapes (up to 256).
 * FPS counter updates each frame, exercising text cache invalidation.
 *
 * Run from project root: ./cmake-build-debug/demo
 */

#include <cels/cels.h>
#include <cels_sdl3.h>
#include <stdio.h>

/* ============================================================================
 * Scene State -- click-placed shapes
 * ============================================================================ */

typedef struct PlacedShape {
    float x, y;
    SDL_Color color;
} PlacedShape;

#define MAX_PLACED_SHAPES 256
static PlacedShape s_placed[MAX_PLACED_SHAPES];
static int s_placed_count = 0;
static int s_color_index = 0;

static const SDL_Color SHAPE_COLORS[] = {
    {255, 100, 100, 255},  /* soft red */
    {100, 255, 100, 255},  /* soft green */
    {100, 100, 255, 255},  /* soft blue */
    {255, 255, 100, 255},  /* yellow */
    {255, 100, 255, 255},  /* magenta */
    {100, 255, 255, 255},  /* cyan */
};
#define NUM_SHAPE_COLORS (sizeof(SHAPE_COLORS) / sizeof(SHAPE_COLORS[0]))

/* ============================================================================
 * FPS string buffer -- double-buffered for pointer-change detection
 * ============================================================================
 *
 * The text system detects changes by comparing the string pointer with
 * last_string. By alternating between two buffers, the pointer changes
 * each frame, triggering TTF_SetTextString to sync content.
 */

static char s_fps_bufs[2][32] = { "FPS: --", "FPS: --" };
static int s_fps_buf_idx = 0;

/* ============================================================================
 * FontLoader system -- loads fonts at startup (runs once via static guard)
 * ============================================================================ */

CEL_System(FontLoader, .phase = OnLoad) {
    static bool loaded = false;
    if (loaded) return;
    loaded = true;

    if (!sdl3_font_load(0, "assets/PressStart2P-Regular.ttf", 16.0f)) {
        printf("Warning: failed to load font slot 0\n");
    }
    printf("Demo fonts loaded\n");
}

/* ============================================================================
 * World -- window + sprite + text entities
 * ============================================================================
 *
 * Window entity gets the sprite component (texture system queries
 * SDL3_Sprite + SDL3_Renderer + SDL3_WindowComponent on the same entity).
 * Text entities are separate from the window entity.
 */

CEL_Compose(World) {
    SDL3Window(.title = "cels-sdl3 Demo", .width = 1280, .height = 720,
               .context = true, .target_fps = 60) {
        /* Sprite on the window entity (texture system queries Sprite+Renderer+Window) */
        cel_has(SDL3_Sprite,
            .texture_path = "assets/tree.png",
            .dst_rect = { .x = 500, .y = 380, .w = 48, .h = 64 },
            .flip = SDL_FLIP_NONE,
            .alpha = 255
        );
    }

    /* Title text entity */
    cel_entity() {
        cel_has(SDL3_Text,
            .string = "cels-sdl3 Demo",
            .font_id = 0,
            .color = {255, 255, 255, 255},
            .x = 16, .y = 16
        );
        cel_has(SDL3_TextHandle);
    }

    /* FPS counter text entity -- string points to double-buffered static */
    cel_entity() {
        cel_has(SDL3_Text,
            .string = s_fps_bufs[0],
            .font_id = 0,
            .color = {255, 255, 0, 255},
            .x = 16, .y = 40
        );
        cel_has(SDL3_TextHandle);
    }

    /* Instructions text entity */
    cel_entity() {
        cel_has(SDL3_Text,
            .string = "Click to plant / ESC to quit",
            .font_id = 0,
            .color = {200, 200, 200, 255},
            .x = 16, .y = 64
        );
        cel_has(SDL3_TextHandle);
    }
}

/* ============================================================================
 * DemoInit system -- one-shot: set sky-blue clear color on all renderers
 * ============================================================================ */

CEL_System(DemoInit, .phase = OnLoad) {
    static bool done = false;
    if (done) return;
    done = true;

    cel_query(SDL3_Renderer);
    cel_each(SDL3_Renderer) {
        cel_update(SDL3_Renderer) {
            SDL3_Renderer->clear_color = (SDL_Color){135, 206, 235, 255};
        }
    }
}

/* ============================================================================
 * DemoInput system -- ESC to quit, click to place shapes
 * ============================================================================ */

CEL_System(DemoInput, .phase = OnUpdate) {
    cel_query(SDL3_EventQueue);
    cel_each(SDL3_EventQueue) {
        for (int i = 0; i < SDL3_EventQueue->count; i++) {
            const SDL_Event* ev = &SDL3_EventQueue->events[i];

            switch (ev->type) {

            case SDL_EVENT_KEY_DOWN:
                if (ev->key.key == SDLK_ESCAPE) {
                    cel_quit();
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (s_placed_count < MAX_PLACED_SHAPES) {
                    s_placed[s_placed_count] = (PlacedShape){
                        .x = ev->button.x,
                        .y = ev->button.y,
                        .color = SHAPE_COLORS[s_color_index % NUM_SHAPE_COLORS]
                    };
                    s_placed_count++;
                    s_color_index++;
                }
                break;

            default:
                break;
            }
        }
    }
}

/* ============================================================================
 * DemoRenderer system -- landscape scene + click-placed shapes
 * ============================================================================
 *
 * All drawing uses the sdl3_renderable() vtable. Draw commands buffer
 * during OnRender and flush at PostRender (DrawFlushSystem), sorted by
 * z-index. Sprites and text render directly at OnRender via their own
 * systems (SpriteRenderSystem, TextRenderSystem).
 *
 * Z-layers:
 *   0 = ground, horizon
 *   1 = building, sun (static scene elements)
 *   3 = click-placed shapes
 */

CEL_System(DemoRenderer, .phase = OnRender) {
    const SDL3_Renderable* draw = sdl3_renderable();

    cel_query(SDL3_Renderer, SDL3_WindowComponent);
    cel_each(SDL3_Renderer, SDL3_WindowComponent) {
        if (SDL3_WindowComponent->state == SDL3_WINDOW_MINIMIZED ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSED) continue;

        SDL_Renderer* r = SDL3_Renderer->renderer;
        if (!r) continue;

        /* ----------------------------------------------------------------
         * z=0: Ground -- grass green filled rect (bottom third)
         * ---------------------------------------------------------------- */
        draw->filled_rect(r,
            (SDL_FRect){0, 480, 1280, 240},
            (SDL_Color){76, 153, 0, 255}, 0);

        /* ----------------------------------------------------------------
         * z=0: Horizon line -- dark green separating sky and ground
         * ---------------------------------------------------------------- */
        draw->line(r,
            (SDL_FPoint){0, 480},
            (SDL_FPoint){1280, 480},
            (SDL_Color){60, 120, 0, 255}, 0);

        /* ----------------------------------------------------------------
         * z=1: Building body -- brown filled rect
         * ---------------------------------------------------------------- */
        draw->filled_rect(r,
            (SDL_FRect){200, 380, 120, 100},
            (SDL_Color){160, 82, 45, 255}, 1);

        /* ----------------------------------------------------------------
         * z=1: Building outline -- darker brown
         * ---------------------------------------------------------------- */
        draw->outlined_rect(r,
            (SDL_FRect){200, 380, 120, 100},
            (SDL_Color){100, 50, 25, 255}, 1);

        /* ----------------------------------------------------------------
         * z=1: Sun -- golden yellow outlined rect
         * ---------------------------------------------------------------- */
        draw->outlined_rect(r,
            (SDL_FRect){1050, 80, 80, 80},
            (SDL_Color){255, 220, 50, 255}, 1);

        /* ----------------------------------------------------------------
         * z=3: Click-placed shapes -- 16x16 filled rects at click positions
         * ---------------------------------------------------------------- */
        for (int i = 0; i < s_placed_count; i++) {
            draw->filled_rect(r,
                (SDL_FRect){s_placed[i].x - 8, s_placed[i].y - 8, 16, 16},
                s_placed[i].color, 3);
        }
    }
}

/* ============================================================================
 * FPSUpdater system -- updates FPS text string each frame
 * ============================================================================
 *
 * Reads smoothed_fps from SDL3_FrameState, formats into a double-buffered
 * static string, then finds the FPS text entity (y ~ 40) and swaps its
 * string pointer. The pointer change triggers the text system to call
 * TTF_SetTextString for cache invalidation.
 */

CEL_System(FPSUpdater, .phase = OnUpdate) {
    /* Format FPS into the next buffer */
    {
        const struct SDL3_FrameState* frame = cel_read(SDL3_FrameState);
        if (frame) {
            int next = 1 - s_fps_buf_idx;
            snprintf(s_fps_bufs[next], sizeof(s_fps_bufs[next]),
                     "FPS: %.0f", frame->smoothed_fps);
            s_fps_buf_idx = next;
        }
    }

    /* Find the FPS text entity (y ~ 40) and update its string pointer */
    {
        cel_query(SDL3_Text);
        cel_each(SDL3_Text) {
            if (SDL3_Text->y > 35.0f && SDL3_Text->y < 45.0f) {
                cel_update(SDL3_Text) {
                    SDL3_Text->string = s_fps_bufs[s_fps_buf_idx];
                }
            }
        }
    }
}

/* ============================================================================
 * Main -- register all systems and run the demo
 * ============================================================================ */

cels_main() {
    cels_register(SDL3_Engine);
    SDL3_Renderable_use();
    cels_register(FontLoader);
    cels_register(DemoInit);
    cels_register(DemoInput);
    cels_register(DemoRenderer);
    cels_register(FPSUpdater);
    cels_session(World) {
        while (sdl3_should_run()) {
            float dt = sdl3_delta();
            cels_step(dt);
        }
    }
    printf("cels-sdl3 Demo -- clean exit\n");
}
