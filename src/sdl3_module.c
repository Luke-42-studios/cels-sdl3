/*
 * SDL3 Module - CEL_Module(SDL3_Engine) Definition
 *
 * All CELS declarations for the SDL3 module: lifecycles, observers,
 * and compositions. Implementation details (SDL3 init/shutdown) are
 * delegated to helper functions in sdl3_init.c.
 *
 * IMPORTANT: All code that uses component _id variables MUST live in
 * this file. CEL_Component generates static per-TU _id vars, and
 * cels_register only initializes THIS TU's copies.
 */

#include <cels_sdl3.h>
#include "sdl3_internal.h"

CEL_State(SDL3_ContextState);
CEL_State(SDL3_FrameState);

/* ============================================================================
 * Registration Guards -- prevent double-registration
 * ============================================================================ */

static bool s_init_registered      = false;
static bool s_frameloop_registered = false;
static bool s_input_registered     = false;
static bool s_textures_registered  = false;
static bool s_window_registered    = false;
static bool s_text_registered      = false;
static bool s_renderer_registered  = false;

/* ============================================================================
 * Context Lifecycle
 * ============================================================================ */

CEL_Lifecycle(SDL3_ContextLC);

CEL_Observe(SDL3_ContextLC, on_create) {
    const SDL3_ContextConfig* config = cel_watch(entity, SDL3_ContextConfig);
    if (!config) return;
    sdl3_init(config);
}

CEL_Observe(SDL3_ContextLC, on_destroy) {
    (void)entity;
    sdl3_shutdown();
}

/* ============================================================================
 * Window Lifecycle
 * ============================================================================ */

CEL_Lifecycle(SDL3_WindowLC);

CEL_Observe(SDL3_WindowLC, on_create) {
    const SDL3_WindowConfig* config = cel_watch(entity, SDL3_WindowConfig);
    if (!config) return;
    if (config->context) {
        sdl3_ensure_init();
        if (config->target_fps > 0) {
            sdl3_set_target_fps(config->target_fps);
        }
    }
    sdl3_window_create(entity, config, SDL3_WindowComponent_id);

    /* Attach empty event queue so InputSystem can buffer events per-window */
    SDL3_EventQueue empty_queue = { .count = 0 };
    cels_entity_set_component(entity, SDL3_EventQueue_id,
                              &empty_queue, sizeof(empty_queue));

    /* Create renderer for this window -- must happen after window creation
     * since sdl3_renderer_create needs the SDL_Window* from the component.
     * Re-read the window component to get the created window pointer. */
    const SDL3_WindowComponent* win_comp =
        (const SDL3_WindowComponent*)cels_entity_get_component(
            entity, SDL3_WindowComponent_id);
    if (win_comp && win_comp->window) {
        sdl3_renderer_create(entity, win_comp->window, SDL3_Renderer_id);
    }
}

CEL_Observe(SDL3_WindowLC, on_destroy) {
    const SDL3_WindowComponent* comp = cel_watch(entity, SDL3_WindowComponent);
    if (comp && comp->window) {
        sdl3_window_destroy(comp->window);
    }
}

/* ============================================================================
 * Input System -- drains SDL events, routes to windows, fills queues
 * ============================================================================
 *
 * Runs at OnLoad phase BEFORE WindowStateSystem. Replaces Phase 3's
 * EventPumpSystem with a unified input pipeline:
 *
 *   1. Clear all per-window event queues (count = 0)
 *   2. Build a window table with mutable pointers for the drain function
 *   3. Call sdl3_input_drain_events which:
 *      - Handles minimized-pause (SDL_WaitEvent when all windows minimized)
 *      - Polls all pending SDL events (SDL_PollEvent)
 *      - Routes window events to sdl3_window_handle_event (never queued)
 *      - Routes SDL_EVENT_QUIT to cels_request_quit
 *      - Buffers all other events into per-window SDL3_EventQueue by windowID
 *
 * The drain function lives in sdl3_input.c and receives the window table
 * because it cannot use cel_query/cel_each (per-TU static ID constraint).
 */

CEL_System(SDL3_InputSystem, .phase = OnLoad) {
    SDL3_WindowTable table = { .count = 0, .minimized_count = 0 };

    cel_query(SDL3_WindowComponent, SDL3_EventQueue);
    cel_each(SDL3_WindowComponent, SDL3_EventQueue) {
        cel_update(SDL3_WindowComponent) {
            cel_update(SDL3_EventQueue) {
                /* Clear queue for this frame */
                SDL3_EventQueue->count = 0;

                /* Add to window table for drain function */
                if (table.count < SDL3_MAX_WINDOWS) {
                    table.entries[table.count] = (SDL3_WindowEntry){
                        .window_id = SDL3_WindowComponent->window_id,
                        .comp      = SDL3_WindowComponent,
                        .queue     = SDL3_EventQueue
                    };
                    if (SDL3_WindowComponent->state == SDL3_WINDOW_MINIMIZED) {
                        table.minimized_count++;
                    }
                    table.count++;
                }
            }
        }
    }

    sdl3_input_drain_events(&table);

    /* Notify reactivity system so cel_watch(entity, SDL3_WindowComponent)
     * triggers recomposition when window events change state/size */
    if (table.window_dirty) {
        cels_component_notify_change(SDL3_WindowComponent_id);
    }
}

/* ============================================================================
 * Texture Load System -- loads textures for sprites with NONE state
 * ============================================================================
 *
 * Runs at OnLoad phase AFTER InputSystem. Scans for sprites with a
 * texture_path set but state == NONE, loads via IMG_LoadTexture into
 * the per-renderer texture cache, and transitions to READY or FAILED.
 *
 * Sprites MUST live on window entities (same entity that has the renderer).
 */

CEL_System(SDL3_TextureLoadSystem, .phase = OnLoad) {
    cel_query(SDL3_Sprite, SDL3_Renderer, SDL3_WindowComponent);
    cel_each(SDL3_Sprite, SDL3_Renderer, SDL3_WindowComponent) {
        if (SDL3_Sprite->state != SDL3_TEXTURE_NONE) continue;
        if (!SDL3_Sprite->texture_path) continue;
        if (!SDL3_Renderer->renderer) continue;

        SDL3_TextureCache* cache = sdl3_texture_cache_for_renderer(
            SDL3_Renderer->renderer);
        if (!cache) continue;

        cel_update(SDL3_Sprite) {
            SDL3_Sprite->state = SDL3_TEXTURE_LOADING;
            uint32_t handle = sdl3_texture_cache_load(
                cache, SDL3_Renderer->renderer, SDL3_Sprite->texture_path);

            if (handle != 0) {
                SDL3_Sprite->texture_handle = handle;
                SDL3_Sprite->state = SDL3_TEXTURE_READY;
            } else {
                SDL3_Sprite->state = SDL3_TEXTURE_FAILED;
                sdl3_report_error("texture_load");
            }
        }
    }
}

/* ============================================================================
 * Sprite Render System -- renders READY sprites via SDL_RenderTextureRotated
 * ============================================================================
 *
 * Runs at OnRender phase. Draws all sprites in READY state with support
 * for source/destination rects, rotation, flip, and per-sprite alpha.
 */

CEL_System(SDL3_SpriteRenderSystem, .phase = OnRender) {
    cel_query(SDL3_Sprite, SDL3_Renderer, SDL3_WindowComponent);
    cel_each(SDL3_Sprite, SDL3_Renderer, SDL3_WindowComponent) {
        if (SDL3_WindowComponent->state == SDL3_WINDOW_MINIMIZED ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSED) continue;
        if (SDL3_Sprite->state != SDL3_TEXTURE_READY) continue;

        SDL3_TextureCache* cache = sdl3_texture_cache_for_renderer(
            SDL3_Renderer->renderer);
        if (!cache) continue;

        SDL_Texture* tex = sdl3_texture_cache_get(
            cache, SDL3_Sprite->texture_handle);
        if (!tex) continue;

        /* Apply per-sprite alpha modulation */
        if (SDL3_Sprite->alpha < 255) {
            SDL_SetTextureAlphaMod(tex, SDL3_Sprite->alpha);
        }

        /* Determine source rect: NULL = entire texture */
        const SDL_FRect* src = NULL;
        if (SDL3_Sprite->src_rect.w > 0 && SDL3_Sprite->src_rect.h > 0) {
            src = &SDL3_Sprite->src_rect;
        }

        /* Determine rotation center: NULL = center of dst_rect */
        const SDL_FPoint* center = NULL;
        if (SDL3_Sprite->center.x != 0 || SDL3_Sprite->center.y != 0) {
            center = &SDL3_Sprite->center;
        }

        SDL_RenderTextureRotated(
            SDL3_Renderer->renderer, tex,
            src, &SDL3_Sprite->dst_rect,
            SDL3_Sprite->angle, center,
            SDL3_Sprite->flip);

        /* Reset alpha mod to avoid state leakage to other sprites using same texture */
        if (SDL3_Sprite->alpha < 255) {
            SDL_SetTextureAlphaMod(tex, 255);
        }
    }
}

/* ============================================================================
 * Text Renderer Lookup System -- finds active renderer for text rendering
 * ============================================================================
 *
 * Runs at OnRender phase BEFORE TextRenderSystem (registration order).
 * Finds the first READY window's renderer and text engine, stores them
 * in file-scoped statics for TextRenderSystem to consume.
 *
 * Split from TextRenderSystem to obey the one-cel_query-per-system rule.
 */

static SDL_Renderer*  s_text_active_renderer = NULL;
static TTF_TextEngine* s_text_active_engine  = NULL;

CEL_System(SDL3_TextRendererLookupSystem, .phase = OnRender) {
    s_text_active_renderer = NULL;
    s_text_active_engine   = NULL;

    cel_query(SDL3_WindowComponent, SDL3_Renderer);
    cel_each(SDL3_WindowComponent, SDL3_Renderer) {
        if (SDL3_WindowComponent->state == SDL3_WINDOW_READY ||
            SDL3_WindowComponent->state == SDL3_WINDOW_RESIZING) {
            if (SDL3_Renderer->renderer && SDL3_Renderer->text_engine) {
                s_text_active_renderer = SDL3_Renderer->renderer;
                s_text_active_engine   = SDL3_Renderer->text_engine;
                /* Note: no break -- cel_each uses nested for-loops where
                 * break only exits the innermost loop, causing an infinite loop.
                 * Instead, we just let the loop finish (single-window is typical). */
            }
        }
    }
}

/* ============================================================================
 * Text Render System -- renders text entities each frame
 * ============================================================================
 *
 * Runs at OnRender phase AFTER TextRendererLookupSystem (registration order).
 * Uses s_text_active_renderer / s_text_active_engine set by the lookup system.
 *
 * Single-window text assignment: all text draws to the first READY window.
 */

CEL_System(SDL3_TextRenderSystem, .phase = OnRender) {
    if (!s_text_active_renderer || !s_text_active_engine) return;

    cel_query(SDL3_Text, SDL3_TextHandle);
    cel_each(SDL3_Text, SDL3_TextHandle) {
        /* Create TTF_Text on first encounter */
        if (!SDL3_TextHandle->ttf_text && SDL3_Text->string) {
            TTF_Text* created = sdl3_text_create(s_text_active_engine,
                SDL3_Text->font_id, SDL3_Text->string);
            if (created) {
                TTF_SetTextColor(created, SDL3_Text->color.r,
                    SDL3_Text->color.g, SDL3_Text->color.b,
                    SDL3_Text->color.a);
                if (SDL3_Text->wrap_width > 0) {
                    TTF_SetTextWrapWidth(created, SDL3_Text->wrap_width);
                }
                cel_update(SDL3_TextHandle) {
                    SDL3_TextHandle->ttf_text = created;
                    SDL3_TextHandle->last_string = SDL3_Text->string;
                    SDL3_TextHandle->last_color = SDL3_Text->color;
                    SDL3_TextHandle->last_wrap = SDL3_Text->wrap_width;
                    SDL3_TextHandle->last_font_id = SDL3_Text->font_id;
                }
            }
        }

        /* Sync changes if properties changed */
        if (SDL3_TextHandle->ttf_text) {
            bool dirty = sdl3_text_sync(SDL3_TextHandle->ttf_text,
                SDL3_Text, SDL3_TextHandle);
            if (dirty) {
                cel_update(SDL3_TextHandle) {
                    SDL3_TextHandle->last_string = SDL3_Text->string;
                    SDL3_TextHandle->last_color = SDL3_Text->color;
                    SDL3_TextHandle->last_wrap = SDL3_Text->wrap_width;
                    SDL3_TextHandle->last_font_id = SDL3_Text->font_id;
                }
            }

            /* Compute draw position based on alignment */
            float draw_x = SDL3_Text->x;
            float draw_y = SDL3_Text->y;

            if (SDL3_Text->align != SDL3_TEXT_ALIGN_LEFT) {
                int w = 0, h = 0;
                TTF_GetTextSize(SDL3_TextHandle->ttf_text, &w, &h);
                if (SDL3_Text->align == SDL3_TEXT_ALIGN_CENTER) {
                    draw_x -= (float)w / 2.0f;
                } else if (SDL3_Text->align == SDL3_TEXT_ALIGN_RIGHT) {
                    draw_x -= (float)w;
                }
            }

            TTF_DrawRendererText(SDL3_TextHandle->ttf_text, draw_x, draw_y);
        }
    }
}

/* ============================================================================
 * Text Cleanup Check System -- detects CLOSING windows for text cleanup
 * ============================================================================
 *
 * Runs at OnLoad phase BEFORE TextCleanupSystem (registration order).
 * Sets s_any_window_closing so TextCleanupSystem knows whether to
 * destroy TTF_Text handles. Split from WindowStateSystem to obey
 * the one-cel_query-per-system rule.
 */

static bool s_any_window_closing = false;

CEL_System(SDL3_TextCleanupCheckSystem, .phase = OnLoad) {
    s_any_window_closing = false;

    cel_query(SDL3_WindowComponent);
    cel_each(SDL3_WindowComponent) {
        if (SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING) {
            s_any_window_closing = true;
        }
    }
}

/* ============================================================================
 * Text Cleanup System -- destroys TTF_Text handles when windows close
 * ============================================================================
 *
 * Runs at OnLoad phase AFTER TextCleanupCheckSystem, BEFORE WindowStateSystem.
 * TTF_Text objects MUST be destroyed before the text engine and renderer.
 * Destruction order: TTF_Text -> TTF_TextEngine -> SDL_Renderer -> SDL_Window
 */

CEL_System(SDL3_TextCleanupSystem, .phase = OnLoad) {
    if (!s_any_window_closing) return;

    cel_query(SDL3_TextHandle);
    cel_each(SDL3_TextHandle) {
        if (SDL3_TextHandle->ttf_text) {
            sdl3_text_destroy(SDL3_TextHandle->ttf_text);
            cel_update(SDL3_TextHandle) {
                SDL3_TextHandle->ttf_text = NULL;
            }
        }
    }
}

/* ============================================================================
 * Window State System -- per-frame state machine driver
 * ============================================================================
 *
 * Runs at OnLoad phase AFTER TextCleanupCheckSystem and TextCleanupSystem.
 * Handles CLOSING -> CLOSED transitions: destroys text engine, renderer,
 * and window in correct order. Text handles are already cleaned up by
 * TextCleanupSystem above.
 */

CEL_System(SDL3_WindowStateSystem, .phase = OnLoad) {
    cel_query(SDL3_WindowComponent, SDL3_Renderer);
    cel_each(SDL3_WindowComponent, SDL3_Renderer) {
        if (SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING) {
            /* CLOSING -> CLOSED: one frame has elapsed since close request.
             * Destroy text engine BEFORE renderer, renderer BEFORE window. */
            bool owns_context = SDL3_WindowComponent->context_bound;

            /* Destroy text engine after text handles (TextCleanupSystem above) */
            if (SDL3_Renderer->text_engine) {
                TTF_DestroyRendererTextEngine(SDL3_Renderer->text_engine);
                cel_update(SDL3_Renderer) {
                    SDL3_Renderer->text_engine = NULL;
                }
            }

            if (SDL3_Renderer->renderer) {
                /* Invalidate all cached textures for this renderer.
                 * Does NOT call SDL_DestroyTexture -- SDL_DestroyRenderer
                 * handles that automatically. Removes renderer from cache table. */
                sdl3_texture_cache_remove_renderer(SDL3_Renderer->renderer);
                sdl3_draw_buffer_destroy(SDL3_Renderer);
                sdl3_renderer_destroy(SDL3_Renderer->renderer);
                cel_update(SDL3_Renderer) {
                    SDL3_Renderer->renderer = NULL;
                    SDL3_Renderer->draw_cmds = NULL;
                    SDL3_Renderer->draw_count = 0;
                    SDL3_Renderer->draw_capacity = 0;
                }
            }
            if (SDL3_WindowComponent->window) {
                sdl3_window_destroy(SDL3_WindowComponent->window);
            }
            cel_update(SDL3_WindowComponent) {
                SDL3_WindowComponent->window = NULL;
                SDL3_WindowComponent->state = SDL3_WINDOW_CLOSED;
            }
            cels_component_notify_change(SDL3_WindowComponent_id);
            /* Context-bound window closed: shut down SDL3 and exit loop */
            if (owns_context) {
                sdl3_shutdown();
                sdl3_frame_set_running(false);
            }
        }
    }
}

/* ============================================================================
 * Frame State System -- manages running flag based on window states
 * ============================================================================
 *
 * Runs at OnLoad phase AFTER WindowStateSystem. Checks if all windows
 * have reached CLOSED state and sets running = false to exit the loop.
 */

CEL_System(SDL3_FrameStateSystem, .phase = OnLoad) {
    int total_windows = 0;
    int closed_windows = 0;

    cel_query(SDL3_WindowComponent);
    cel_each(SDL3_WindowComponent) {
        total_windows++;
        if (SDL3_WindowComponent->state == SDL3_WINDOW_CLOSED) {
            closed_windows++;
        }
    }

    /* All windows closed -> stop running */
    if (total_windows > 0 && total_windows == closed_windows) {
        sdl3_frame_set_running(false);
    }
}

/* ============================================================================
 * Render Clear System -- clears each window's renderer to its clear color
 * ============================================================================
 *
 * Runs at PreRender phase. Developer draw systems slot in at OnRender
 * between this clear and the present system.
 */

CEL_System(SDL3_RenderClearSystem, .phase = PreRender) {
    sdl3_draw_table_clear();

    cel_query(SDL3_WindowComponent, SDL3_Renderer);
    cel_each(SDL3_WindowComponent, SDL3_Renderer) {
        /* Skip windows that should not render */
        if (SDL3_WindowComponent->state == SDL3_WINDOW_MINIMIZED ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSED) continue;

        /* Register renderer-to-buffer mapping for vtable lookups during OnRender,
         * and reset draw buffer for this frame */
        cel_update(SDL3_Renderer) {
            sdl3_draw_table_add(SDL3_Renderer->renderer, SDL3_Renderer);
            SDL3_Renderer->draw_count = 0;
            SDL3_Renderer->draw_next_order = 0;
        }

        SDL_SetRenderDrawColor(SDL3_Renderer->renderer,
            SDL3_Renderer->clear_color.r,
            SDL3_Renderer->clear_color.g,
            SDL3_Renderer->clear_color.b,
            SDL3_Renderer->clear_color.a);
        SDL_RenderClear(SDL3_Renderer->renderer);
    }
}

/* ============================================================================
 * Draw Flush System -- sorts draw buffer by z-index and flushes to SDL3
 * ============================================================================
 *
 * Runs at PostRender phase BEFORE RenderPresentSystem (registration order).
 * Sorts buffered draw commands by (z ascending, creation order ascending)
 * and issues the actual SDL3 draw calls.
 */

CEL_System(SDL3_DrawFlushSystem, .phase = PostRender) {
    cel_query(SDL3_WindowComponent, SDL3_Renderer);
    cel_each(SDL3_WindowComponent, SDL3_Renderer) {
        if (SDL3_WindowComponent->state == SDL3_WINDOW_MINIMIZED ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSED) continue;

        if (SDL3_Renderer->draw_count == 0) continue;

        cel_update(SDL3_Renderer) {
            sdl3_draw_buffer_flush(SDL3_Renderer);
        }
    }
}

/* ============================================================================
 * Render Present System -- presents each window's backbuffer
 * ============================================================================
 *
 * Runs at PostRender phase, after all developer draw systems have run.
 */

CEL_System(SDL3_RenderPresentSystem, .phase = PostRender) {
    cel_query(SDL3_WindowComponent, SDL3_Renderer);
    cel_each(SDL3_WindowComponent, SDL3_Renderer) {
        if (SDL3_WindowComponent->state == SDL3_WINDOW_MINIMIZED ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSED) continue;

        SDL_RenderPresent(SDL3_Renderer->renderer);
    }
}

/* ============================================================================
 * A La Carte Registration
 * ============================================================================
 *
 * Each _use() function registers its own CELS declarations (components,
 * states, systems, lifecycles). Guard flags prevent double-registration.
 *
 * No auto-dependency pulling: SDL3_Window_use() does NOT register Init.
 * The developer is responsible for calling the _use() functions they need,
 * or using SDL3_use() for all-in-one registration.
 *
 * System execution order (determined by registration order within each phase):
 *   OnLoad:     InputSystem -> TextureLoadSystem -> TextCleanupCheckSystem
 *               -> TextCleanupSystem -> WindowStateSystem -> FrameStateSystem
 *   PreRender:  RenderClearSystem
 *   OnRender:   SpriteRenderSystem -> TextRendererLookupSystem -> TextRenderSystem
 *   PostRender: DrawFlushSystem -> RenderPresentSystem
 */

void SDL3_Init_use(void) {
    if (s_init_registered) return;
    s_init_registered = true;
    cels_register(SDL3_ContextState, SDL3_ContextLC, SDL3_ContextConfig);
}

void SDL3_FrameLoop_use(void) {
    if (s_frameloop_registered) return;
    s_frameloop_registered = true;
    cels_register(SDL3_FrameState);
}

void SDL3_Input_use(void) {
    if (s_input_registered) return;
    s_input_registered = true;
    cels_register(SDL3_InputSystem);
}

void SDL3_Textures_use(void) {
    if (s_textures_registered) return;
    s_textures_registered = true;
    cels_register(SDL3_TextureLoadSystem);
    cels_register(SDL3_Sprite, SDL3_SpriteRenderSystem);

    cels_ensure_component(&SDL3_Sprite_id, "SDL3_Sprite",
                          sizeof(SDL3_Sprite), CELS_ALIGNOF(SDL3_Sprite));
}

void SDL3_Window_use(void) {
    if (s_window_registered) return;
    s_window_registered = true;
    /* TextCleanupCheckSystem and TextCleanupSystem must run BEFORE
     * WindowStateSystem within OnLoad phase (registration order = execution
     * order). They detect CLOSING windows and destroy TTF_Text handles before
     * WindowStateSystem destroys the text engine and renderer. */
    cels_register(SDL3_WindowConfig, SDL3_WindowComponent, SDL3_EventQueue,
                  SDL3_Renderer, SDL3_WindowLC,
                  SDL3_TextCleanupCheckSystem, SDL3_TextCleanupSystem,
                  SDL3_WindowStateSystem);
    /* FrameStateSystem monitors window states (checks if all windows closed).
     * It must register AFTER WindowStateSystem so that within OnLoad phase,
     * WindowStateSystem runs before FrameStateSystem. */
    cels_register(SDL3_FrameStateSystem);

    /* Eagerly initialize component IDs so the window creation observer
     * (CEL_Observe(SDL3_WindowLC, on_create)) can attach components
     * before any system body runs. */
    cels_ensure_component(&SDL3_EventQueue_id, "SDL3_EventQueue",
                          sizeof(SDL3_EventQueue), CELS_ALIGNOF(SDL3_EventQueue));
    cels_ensure_component(&SDL3_Renderer_id, "SDL3_Renderer",
                          sizeof(SDL3_Renderer), CELS_ALIGNOF(SDL3_Renderer));
}

void SDL3_Text_use(void) {
    if (s_text_registered) return;
    s_text_registered = true;
    /* TextRendererLookupSystem must register BEFORE TextRenderSystem so it
     * runs first within OnRender phase, populating s_text_active_renderer
     * and s_text_active_engine for TextRenderSystem to consume. */
    cels_register(SDL3_Text, SDL3_TextHandle,
                  SDL3_TextRendererLookupSystem, SDL3_TextRenderSystem);

    cels_ensure_component(&SDL3_Text_id, "SDL3_Text",
                          sizeof(SDL3_Text), CELS_ALIGNOF(SDL3_Text));
    cels_ensure_component(&SDL3_TextHandle_id, "SDL3_TextHandle",
                          sizeof(SDL3_TextHandle), CELS_ALIGNOF(SDL3_TextHandle));
}

void SDL3_Renderer_use(void) {
    if (s_renderer_registered) return;
    s_renderer_registered = true;
    cels_register(SDL3_RenderClearSystem);
    cels_register(SDL3_DrawFlushSystem);
    cels_register(SDL3_RenderPresentSystem);
}

void SDL3_use(const SDL3_Config* config) {
    /* Apply config before registration */
    if (config) {
        if (config->on_error) {
            sdl3_set_error_callback(config->on_error);
        }
        if (config->sdl_init_flags) {
            sdl3_set_init_flags(config->sdl_init_flags);
        }
    }

    /* Register all providers in correct order.
     * Call order determines system execution within the same CELS phase. */
    SDL3_Init_use();
    SDL3_FrameLoop_use();
    SDL3_Input_use();
    SDL3_Textures_use();
    SDL3_Window_use();
    SDL3_Text_use();
    SDL3_Renderer_use();
}

/* ============================================================================
 * Module Init -- backward compatibility shim
 * ============================================================================
 *
 * cels_register(SDL3_Engine) delegates to SDL3_use(NULL) which registers
 * all providers with default configuration. For custom configuration,
 * call SDL3_use(&config) directly instead of cels_register(SDL3_Engine).
 *
 * System execution order (determined by registration order within each phase):
 *   OnLoad:     InputSystem -> TextureLoadSystem -> TextCleanupCheckSystem
 *               -> TextCleanupSystem -> WindowStateSystem -> FrameStateSystem
 *   PreRender:  RenderClearSystem
 *   OnRender:   SpriteRenderSystem -> TextRendererLookupSystem -> TextRenderSystem
 *   PostRender: DrawFlushSystem -> RenderPresentSystem
 */

CEL_Module(SDL3_Engine, init) {
    SDL3_use(NULL);
}

/* ============================================================================
 * Compositions
 * ============================================================================ */

CEL_Composition(SDL3Context) {
    cel_has(SDL3_ContextConfig, .video = cel.video);
    cels_lifecycle_bind_entity(SDL3_ContextLC_id, cels_get_current_entity());
}

CEL_Composition(SDL3Window) {
    cel_has(SDL3_WindowConfig,
        .title      = cel.title,
        .width      = cel.width  ? cel.width  : 1280,
        .height     = cel.height ? cel.height : 720,
        .flags      = cel.flags | SDL_WINDOW_RESIZABLE,
        .context    = cel.context,
        .target_fps = cel.target_fps
    );
    cels_lifecycle_bind_entity(SDL3_WindowLC_id, cels_get_current_entity());
}
