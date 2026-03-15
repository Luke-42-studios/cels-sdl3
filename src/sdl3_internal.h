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

#endif /* CELS_SDL3_INTERNAL_H */
