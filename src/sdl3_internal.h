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

#endif /* CELS_SDL3_INTERNAL_H */
