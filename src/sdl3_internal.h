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

#endif /* CELS_SDL3_INTERNAL_H */
