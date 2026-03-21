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
                SDL_Log("SDL3: Failed to load texture '%s': %s",
                        SDL3_Sprite->texture_path, SDL_GetError());
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
 * Window State System -- per-frame state machine driver
 * ============================================================================ */

CEL_System(SDL3_WindowStateSystem, .phase = OnLoad) {
    cel_query(SDL3_WindowComponent, SDL3_Renderer);
    cel_each(SDL3_WindowComponent, SDL3_Renderer) {
        if (SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING) {
            /* CLOSING -> CLOSED: one frame has elapsed since close request.
             * Destroy renderer BEFORE window (SDL requirement). */
            bool owns_context = SDL3_WindowComponent->context_bound;
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
 * Module Init
 * ============================================================================
 *
 * Registration order determines system execution within the same phase:
 *   1.  InputSystem (drains events, routes to windows, fills queues)
 *   1b. TextureLoadSystem (loads textures for sprites with NONE state)
 *   2.  WindowState (CLOSING -> CLOSED transitions, texture cache cleanup)
 *   3.  FrameState (checks if all windows closed)
 *   4.  RenderClear (PreRender phase -- clears each window, resets draw buffers)
 *   4b. SpriteRenderSystem (OnRender phase -- renders READY sprites)
 *   5.  DrawFlush (PostRender phase -- sorts and flushes draw commands)
 *   6.  RenderPresent (PostRender phase -- presents each window)
 */

CEL_Module(SDL3_Engine, init) {
    cels_register(SDL3_ContextState, SDL3_ContextLC,
                  SDL3_ContextConfig);
    cels_register(SDL3_FrameState, SDL3_InputSystem);
    cels_register(SDL3_TextureLoadSystem);
    cels_register(SDL3_WindowConfig, SDL3_WindowComponent, SDL3_EventQueue,
                  SDL3_Renderer, SDL3_Sprite,
                  SDL3_WindowLC, SDL3_WindowStateSystem);
    cels_register(SDL3_FrameStateSystem);
    cels_register(SDL3_RenderClearSystem);
    cels_register(SDL3_SpriteRenderSystem);
    cels_register(SDL3_DrawFlushSystem);
    cels_register(SDL3_RenderPresentSystem);

    /* Eagerly initialize SDL3_EventQueue_id so the window creation observer
     * (CEL_Observe(SDL3_WindowLC, on_create)) can attach an empty queue to
     * each new window entity. CEL_Component _register() is a no-op -- the
     * actual ID is only assigned by cels_ensure_component when cel_has or
     * cel_query first references the type. The observer fires before any
     * system body runs, so we force initialization here. */
    cels_ensure_component(&SDL3_EventQueue_id, "SDL3_EventQueue",
                          sizeof(SDL3_EventQueue), CELS_ALIGNOF(SDL3_EventQueue));

    /* Same pattern for SDL3_Renderer_id -- the on_create observer uses it
     * to attach a renderer component before any system body runs. */
    cels_ensure_component(&SDL3_Renderer_id, "SDL3_Renderer",
                          sizeof(SDL3_Renderer), CELS_ALIGNOF(SDL3_Renderer));

    /* Same pattern for SDL3_Sprite_id -- systems that query SDL3_Sprite
     * need the component ID initialized before any system body runs. */
    cels_ensure_component(&SDL3_Sprite_id, "SDL3_Sprite",
                          sizeof(SDL3_Sprite), CELS_ALIGNOF(SDL3_Sprite));
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
