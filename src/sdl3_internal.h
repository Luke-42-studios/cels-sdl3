/*
 * CELS SDL3 - Internal Declarations
 *
 * NOT for consumers. Used by sdl3_module.c to call init/shutdown
 * functions implemented in sdl3_init.c.
 */

#ifndef CELS_SDL3_INTERNAL_H
#define CELS_SDL3_INTERNAL_H
#include <cels_sdl3.h>

extern void sdl3_init(const SDL3_ContextConfig* config);
extern void sdl3_ensure_init(void);
extern void sdl3_shutdown(void);

/* Error reporting -- all SDL failure sites call this */
extern void sdl3_report_error(const char* context);

/* Init configuration -- called by SDL3_use() before registration */
extern void sdl3_set_init_flags(Uint32 flags);

/* Window lifecycle -- called from sdl3_module.c observers */
extern void sdl3_window_create(cels_entity_t entity,
                                const SDL3_WindowConfig* config,
                                cels_entity_t component_id);
extern void sdl3_window_destroy(SDL_Window* window);

/* State transitions -- called from event routing (Phase 4) and systems */
extern void sdl3_window_handle_event(SDL3_WindowComponent* comp,
                                      Uint32 event_type,
                                      int data1, int data2);

/* Loop lifecycle -- called from sdl3_module.c systems */
extern void  sdl3_loop_init(void);
extern float sdl3_compute_delta(void);
extern float sdl3_compute_fps(float dt);
extern void  sdl3_cap_frame_rate(Uint64 frame_start_ns, int target_fps);
extern void  sdl3_frame_set_running(bool running);
extern void  sdl3_set_target_fps(int fps);

/* Renderer lifecycle -- called from sdl3_module.c observer/systems */
extern void sdl3_renderer_create(cels_entity_t entity,
                                  SDL_Window* window,
                                  cels_entity_t component_id);
extern void sdl3_renderer_destroy(SDL_Renderer* renderer);

/* Input system -- called from sdl3_module.c systems
 *
 * The drain function cannot use cel_query/cel_each (per-TU static ID
 * constraint), so the caller in sdl3_module.c collects window entity
 * info into a small table and passes it here.
 */
#define SDL3_MAX_WINDOWS 8

typedef struct SDL3_WindowEntry {
    SDL_WindowID          window_id;
    SDL3_WindowComponent* comp;       /* mutable pointer for event routing */
    SDL3_EventQueue*      queue;      /* mutable pointer for event buffering */
} SDL3_WindowEntry;

typedef struct SDL3_WindowTable {
    SDL3_WindowEntry entries[SDL3_MAX_WINDOWS];
    int              count;
    int              minimized_count;
    bool             window_dirty;  /* set by drain when window events modify component */
} SDL3_WindowTable;

extern void sdl3_input_drain_events(SDL3_WindowTable* table);

/* ============================================================================
 * Draw primitives -- draw buffer lifecycle and renderer-to-buffer lookup
 * ============================================================================ */

#define SDL3_DRAW_BUFFER_INITIAL_CAPACITY 256

typedef struct SDL3_DrawBufferEntry {
    SDL_Renderer*  renderer;
    SDL3_Renderer* comp;    /* mutable pointer to the component (has draw buffer fields) */
} SDL3_DrawBufferEntry;

typedef struct SDL3_DrawBufferTable {
    SDL3_DrawBufferEntry entries[SDL3_MAX_WINDOWS];
    int count;
} SDL3_DrawBufferTable;

/* Draw buffer lifecycle */
extern void sdl3_draw_buffer_init(SDL3_Renderer* comp);
extern void sdl3_draw_buffer_clear(SDL3_Renderer* comp);
extern void sdl3_draw_buffer_push(SDL3_Renderer* comp, SDL3_DrawCmd cmd);
extern void sdl3_draw_buffer_flush(SDL3_Renderer* comp);
extern void sdl3_draw_buffer_destroy(SDL3_Renderer* comp);

/* Renderer-to-buffer lookup */
extern void sdl3_draw_table_clear(void);
extern void sdl3_draw_table_add(SDL_Renderer* renderer, SDL3_Renderer* comp);
extern SDL3_Renderer* sdl3_draw_table_find(SDL_Renderer* renderer);

/* ============================================================================
 * Texture cache -- per-renderer texture caching with reference counting
 * ============================================================================ */

#define SDL3_TEXTURE_CACHE_CAPACITY 128
#define SDL3_TEXTURE_PATH_MAX 256

typedef struct SDL3_TextureCacheEntry {
    char          path[SDL3_TEXTURE_PATH_MAX];  /* Resolved file path (key) */
    SDL_Texture*  texture;                       /* GPU texture (NULL if slot empty) */
    float         width;                         /* Texture width in pixels */
    float         height;                        /* Texture height in pixels */
    int           ref_count;                     /* Number of sprites using this texture */
} SDL3_TextureCacheEntry;

typedef struct SDL3_TextureCache {
    SDL3_TextureCacheEntry entries[SDL3_TEXTURE_CACHE_CAPACITY];
    int                    count;  /* Number of occupied slots */
} SDL3_TextureCache;

/* Texture cache lifecycle */
extern SDL3_TextureCache* sdl3_texture_cache_for_renderer(SDL_Renderer* renderer);
extern uint32_t sdl3_texture_cache_load(SDL3_TextureCache* cache,
                                         SDL_Renderer* renderer,
                                         const char* path);
extern SDL_Texture* sdl3_texture_cache_get(const SDL3_TextureCache* cache,
                                            uint32_t handle);
extern void sdl3_texture_cache_get_size(const SDL3_TextureCache* cache,
                                         uint32_t handle,
                                         float* w, float* h);
extern void sdl3_texture_cache_release(SDL3_TextureCache* cache, uint32_t handle);
extern void sdl3_texture_cache_invalidate(SDL3_TextureCache* cache);
extern void sdl3_texture_cache_remove_renderer(SDL_Renderer* renderer);

/* ============================================================================
 * Text system -- called from sdl3_module.c systems
 * ============================================================================ */

extern TTF_Font* sdl3_font_get(int font_id);
extern TTF_Text* sdl3_text_create(TTF_TextEngine* engine, int font_id,
                                   const char* string);
extern bool      sdl3_text_sync(TTF_Text* ttf_text, const SDL3_Text* text,
                                 const SDL3_TextHandle* handle);
extern void      sdl3_text_destroy(TTF_Text* ttf_text);

#endif /* CELS_SDL3_INTERNAL_H */
