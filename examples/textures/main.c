/*
 * SDL3 Textures Demo -- Texture Loading and Sprite Rendering
 *
 * Exercises the texture system from Phase 7:
 *   - SDL3_Sprite with declarative texture_path (auto-loaded by TextureLoadSystem)
 *   - sdl3_set_asset_base_dir for relative path resolution
 *   - Full texture rendering, source rect sub-image, rotation, flip, alpha
 *   - Keyboard keys 1-5 switch between rendering modes
 *   - Escape key closes the window via cel_quit()
 *
 * The test asset (assets/test.png) is a 64x64 PNG with 4 colored quadrants:
 *   Top-left: Red     | Top-right: Green
 *   Bot-left: Blue    | Bot-right: Yellow (50% alpha)
 *
 * Pressing 2 (source rect mode) renders only the top-left red quadrant,
 * demonstrating spritesheet-style sub-image rendering.
 *
 * Runs with a real display for interactive mode switching.
 * Under SDL_VIDEODRIVER=dummy: starts, handles texture load gracefully, exits.
 */

#include <cels/cels.h>
#include <cels_sdl3.h>
#include <stdio.h>

/* ============================================================================
 * World: single window with one sprite for texture demo
 * ============================================================================ */

CEL_Compose(World) {
    SDL3Window(.title = "Textures Demo", .width = 800, .height = 600,
               .context = true, .target_fps = 60) {
        cel_has(SDL3_Sprite,
            .texture_path = "test.png",
            .dst_rect = { .x = 200, .y = 100, .w = 256, .h = 256 },
            .flip = SDL_FLIP_NONE,
            .alpha = 255
        );
    }
}

/* ============================================================================
 * SpriteController system -- reads events and changes sprite parameters
 * ============================================================================
 *
 * Demonstrates the consumer pattern for modifying sprite state:
 *   1. Query both SDL3_EventQueue and SDL3_Sprite
 *   2. Iterate events[0..count-1] with a plain for-loop
 *   3. On key press, collect changes in locals (deferred mutation)
 *   4. Apply changes via cel_update(SDL3_Sprite)
 *
 * Keys:
 *   1 - Full texture, no effects (reset)
 *   2 - Source rect: top-left quadrant only (32x32 sub-image)
 *   3 - Rotated 45 degrees
 *   4 - Flipped horizontally
 *   5 - Semi-transparent (alpha=128)
 *   Escape - close
 */

CEL_System(SpriteController, .phase = OnUpdate) {
    cel_query(SDL3_EventQueue, SDL3_Sprite);
    cel_each(SDL3_EventQueue, SDL3_Sprite) {
        bool modified = false;
        bool should_quit = false;

        /* Temporary copies of sprite params to modify */
        SDL_FRect src = {0, 0, 0, 0};
        double angle = 0;
        SDL_FlipMode flip = SDL_FLIP_NONE;
        Uint8 alpha = 255;

        for (int i = 0; i < SDL3_EventQueue->count; i++) {
            const SDL_Event* ev = &SDL3_EventQueue->events[i];
            if (ev->type != SDL_EVENT_KEY_DOWN || ev->key.repeat) continue;

            switch (ev->key.key) {

            case SDLK_1:  /* Full texture, no effects */
                src = (SDL_FRect){0, 0, 0, 0};
                angle = 0; flip = SDL_FLIP_NONE; alpha = 255;
                modified = true;
                printf("Mode: full texture\n");
                break;

            case SDLK_2:  /* Top-left quadrant (source rect) */
                src = (SDL_FRect){0, 0, 32, 32};
                angle = 0; flip = SDL_FLIP_NONE; alpha = 255;
                modified = true;
                printf("Mode: source rect (top-left red quadrant)\n");
                break;

            case SDLK_3:  /* Rotated 45 degrees */
                src = (SDL_FRect){0, 0, 0, 0};
                angle = 45.0; flip = SDL_FLIP_NONE; alpha = 255;
                modified = true;
                printf("Mode: rotated 45 degrees\n");
                break;

            case SDLK_4:  /* Flipped horizontally */
                src = (SDL_FRect){0, 0, 0, 0};
                angle = 0; flip = SDL_FLIP_HORIZONTAL; alpha = 255;
                modified = true;
                printf("Mode: flipped horizontally\n");
                break;

            case SDLK_5:  /* Semi-transparent */
                src = (SDL_FRect){0, 0, 0, 0};
                angle = 0; flip = SDL_FLIP_NONE; alpha = 128;
                modified = true;
                printf("Mode: semi-transparent (alpha=128)\n");
                break;

            case SDLK_ESCAPE:
                printf("Escape pressed -- closing\n");
                should_quit = true;
                break;

            default:
                break;
            }
        }

        if (modified) {
            cel_update(SDL3_Sprite) {
                SDL3_Sprite->src_rect = src;
                SDL3_Sprite->angle = angle;
                SDL3_Sprite->flip = flip;
                SDL3_Sprite->alpha = alpha;
            }
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
    cels_register(SpriteController);

    sdl3_set_asset_base_dir("assets");

    cels_session(World) {
        while (sdl3_should_run()) {
            float dt = sdl3_delta();
            cels_step(dt);
        }
    }
    printf("Textures demo complete -- clean exit\n");
}
