/*
 * SDL3 Text Rendering Demo -- Font Loading and Runtime Text Changes
 *
 * Exercises the text rendering system from Phase 8:
 *   - sdl3_font_load() to load TTF fonts at multiple sizes
 *   - SDL3_Text component for declarative text entities
 *   - SDL3_TextHandle for cached TTF_Text objects (managed by system)
 *   - Runtime text content change via SPACE key
 *   - Runtime text color change via 1/2/3 keys
 *   - Multiple text entities at different positions and font sizes
 *   - Clean exit via Escape
 *
 * Font: DejaVuSans loaded from /usr/share/fonts/TTF/DejaVuSans.ttf
 *
 * Runs with a real display for interactive text editing.
 * Under SDL_VIDEODRIVER=dummy: renderer creation fails (pre-existing),
 * so text will not render, but the app exits cleanly.
 */

#include <cels/cels.h>
#include <cels_sdl3.h>
#include <stdio.h>

/* ============================================================================
 * FontLoader system -- loads fonts at startup (runs once via static guard)
 * ============================================================================
 *
 * Loads DejaVuSans at two sizes:
 *   Slot 0: 24pt (title and dynamic text)
 *   Slot 1: 16pt (instructions)
 */

CEL_System(FontLoader, .phase = OnLoad) {
    static bool loaded = false;
    if (loaded) return;
    loaded = true;

    if (!sdl3_font_load(0, "/usr/share/fonts/TTF/DejaVuSans.ttf", 24.0f)) {
        printf("Warning: failed to load font slot 0 (24pt)\n");
    }
    if (!sdl3_font_load(1, "/usr/share/fonts/TTF/DejaVuSans.ttf", 16.0f)) {
        printf("Warning: failed to load font slot 1 (16pt)\n");
    }
    printf("Fonts loaded\n");
}

/* ============================================================================
 * World: window + text entities
 * ============================================================================
 *
 * Text entities are separate from the window entity. The TextRenderSystem
 * finds the first active renderer, then iterates all text entities to
 * create/sync/draw TTF_Text objects.
 */

CEL_Compose(World) {
    SDL3Window(.title = "Text Rendering Demo", .width = 800, .height = 600,
               .context = true, .target_fps = 60) {}

    /* Title text -- large, centered at top */
    cel_entity() {
        cel_has(SDL3_Text,
            .string = "Text Rendering Demo",
            .font_id = 0,
            .color = {255, 255, 255, 255},
            .x = 400.0f, .y = 50.0f,
            .align = SDL3_TEXT_ALIGN_CENTER
        );
        cel_has(SDL3_TextHandle);
    }

    /* Dynamic text -- will be changed at runtime via keyboard */
    cel_entity() {
        cel_has(SDL3_Text,
            .string = "Hello, World!",
            .font_id = 0,
            .color = {100, 255, 100, 255},
            .x = 400.0f, .y = 250.0f,
            .align = SDL3_TEXT_ALIGN_CENTER
        );
        cel_has(SDL3_TextHandle);
    }

    /* Instructions text -- smaller font, centered at bottom */
    cel_entity() {
        cel_has(SDL3_Text,
            .string = "Press 1-3 to change color, SPACE to change text, ESC to quit",
            .font_id = 1,
            .color = {200, 200, 200, 255},
            .x = 400.0f, .y = 550.0f,
            .align = SDL3_TEXT_ALIGN_CENTER
        );
        cel_has(SDL3_TextHandle);
    }
}

/* ============================================================================
 * TextInteraction system -- reads events and changes text properties
 * ============================================================================
 *
 * Demonstrates runtime text modification:
 *   1. First block: query SDL3_EventQueue to collect input
 *   2. Second block: query SDL3_Text to apply changes
 *
 * Two separate cel_query calls require block scoping to avoid
 * _cel_next_field_index redefinition (see 08-01 decision).
 *
 * Keys:
 *   1 - Change dynamic text color to red
 *   2 - Change dynamic text color to green
 *   3 - Change dynamic text color to cornflower blue
 *   SPACE - Toggle text content between "Hello, World!" and "Text changed!"
 *   ESCAPE - Exit
 */

CEL_System(TextInteraction, .phase = OnUpdate) {
    static const char* texts[] = {"Hello, World!", "Text changed!"};
    static int text_idx = 0;
    SDL_Color new_color = {0};
    bool color_change = false;
    bool text_change = false;
    bool quit = false;

    /* First block: read events from the window entity */
    {
        cel_query(SDL3_EventQueue);
        cel_each(SDL3_EventQueue) {
            for (int i = 0; i < SDL3_EventQueue->count; i++) {
                const SDL_Event* ev = &SDL3_EventQueue->events[i];
                if (ev->type != SDL_EVENT_KEY_DOWN || ev->key.repeat) continue;

                switch (ev->key.key) {
                case SDLK_1:
                    new_color = (SDL_Color){255, 80, 80, 255};
                    color_change = true;
                    printf("Color: red\n");
                    break;
                case SDLK_2:
                    new_color = (SDL_Color){80, 255, 80, 255};
                    color_change = true;
                    printf("Color: green\n");
                    break;
                case SDLK_3:
                    new_color = (SDL_Color){100, 149, 237, 255};
                    color_change = true;
                    printf("Color: cornflower blue\n");
                    break;
                case SDLK_SPACE:
                    text_idx = 1 - text_idx;
                    text_change = true;
                    printf("Text: %s\n", texts[text_idx]);
                    break;
                case SDLK_ESCAPE:
                    printf("Escape pressed -- closing\n");
                    quit = true;
                    break;
                default:
                    break;
                }
            }
        }
    }

    if (quit) {
        cel_quit();
        return;
    }

    /* Second block: apply changes to the dynamic text entity (y ~ 250) */
    if (color_change || text_change) {
        cel_query(SDL3_Text);
        cel_each(SDL3_Text) {
            if (SDL3_Text->y > 200.0f && SDL3_Text->y < 300.0f) {
                cel_update(SDL3_Text) {
                    if (color_change) SDL3_Text->color = new_color;
                    if (text_change) SDL3_Text->string = texts[text_idx];
                }
            }
        }
    }
}

/* ============================================================================
 * Main -- canonical consumer loop pattern
 * ============================================================================ */

cels_main() {
    cels_register(SDL3_Engine);
    cels_register(FontLoader);
    cels_register(TextInteraction);
    cels_session(World) {
        while (sdl3_should_run()) {
            float dt = sdl3_delta();
            cels_step(dt);
        }
    }
    printf("Text demo complete -- clean exit\n");
}
