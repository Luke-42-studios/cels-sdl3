/*
 * CELS SDL3 - Developer API
 *
 * Single include for application developers using the SDL3 module:
 *
 *   #include <cels/cels.h>
 *   #include <cels_sdl3.h>
 *
 * Provides:
 *   - SDL3 module registration (cels_register(SDL3_Engine))
 *   - Component types (SDL3_ContextConfig)
 *   - State singletons (SDL3_ContextState)
 *   - Composition (SDL3Context call macro)
 *
 * Does NOT re-export <cels/cels.h> -- include that separately.
 */

#ifndef CELS_SDL3_H
#define CELS_SDL3_H
#include <cels/cels.h>
#include <SDL3/SDL.h>
#include <stdbool.h>

CEL_Module(SDL3_Engine);

/* ============================================================================
 * Component Types
 * ============================================================================ */

/*
 * Developer sets this on an entity to configure SDL3 initialization.
 * An observer reacts to this component being added and initializes SDL3.
 *
 *   SDL3Context(.video = true) {}
 *
 * video: initialize SDL_INIT_VIDEO (implies SDL_INIT_EVENTS)
 */
CEL_Component(SDL3_ContextConfig) {
    bool video;
};

/* ============================================================================
 * State Singletons
 * ============================================================================
 *
 * CEL_State type with cross-TU pointer registry.
 * Writer TU (sdl3_init.c) owns the data.
 * Consumers read via cel_read(SDL3_ContextState).
 */

/*
 * SDL3 context state singleton. Tracks initialization status.
 * Read via cel_read(SDL3_ContextState) in consumer systems.
 */
CEL_Define_State(SDL3_ContextState) {
    bool initialized;
    bool ttf_ready;
};

/* ============================================================================
 * Composition: SDL3Context
 * ============================================================================
 *
 * Public composition exported via CEL_Define_Composition. Developer creates
 * SDL3 context entities with natural syntax:
 *
 *   SDL3Context(.video = true) {}
 *
 * Implementation in sdl3_module.c via CEL_Composition(SDL3Context).
 */
CEL_Define_Composition(SDL3Context, bool video;);

/* Call macro for natural syntax */
#define SDL3Context(...) cel_init(SDL3Context, __VA_ARGS__)

/* ============================================================================
 * Window Types
 * ============================================================================ */

/*
 * Window lifecycle state machine.
 *
 * Creation chain (synchronous): NONE -> CREATED -> SURFACE_READY -> READY
 * Close chain (one-frame delay): READY -> CLOSING -> CLOSED
 * Minimize/restore: READY -> MINIMIZED -> SURFACE_READY -> READY
 * Resize: READY -> RESIZING -> READY (when stable)
 */
typedef enum SDL3_WindowState {
    SDL3_WINDOW_NONE = 0,
    SDL3_WINDOW_CREATED,
    SDL3_WINDOW_SURFACE_READY,
    SDL3_WINDOW_READY,
    SDL3_WINDOW_RESIZING,
    SDL3_WINDOW_MINIMIZED,
    SDL3_WINDOW_CLOSING,
    SDL3_WINDOW_CLOSED
} SDL3_WindowState;

/*
 * Developer sets this on an entity to configure window creation.
 *
 *   SDL3Window(.title = "My Game", .width = 800, .height = 600) {}
 *
 * Defaults: 1280x720, resizable, empty title, no special flags.
 */
CEL_Component(SDL3_WindowConfig) {
    const char*     title;
    int             width;
    int             height;
    SDL_WindowFlags flags;
    bool            context;    /* bind SDL3 context lifecycle to this window */
    int             target_fps; /* 0 = uncapped, >0 = cap to this FPS */
};

/*
 * Runtime window state -- attached by lifecycle observer after creation.
 * Read via cel_watch(entity, SDL3_WindowComponent) in consumer systems.
 *
 * The SDL_Window* is exposed for advanced users who need direct SDL3 access.
 */
CEL_Component(SDL3_WindowComponent) {
    SDL_Window*      window;
    SDL_WindowID     window_id;
    SDL3_WindowState state;
    int              width;
    int              height;
    bool             context_bound;  /* true if this window owns the SDL3 context */
};

/* ============================================================================
 * Composition: SDL3Window
 * ============================================================================
 *
 * Creates a window entity with natural syntax:
 *
 *   SDL3Window(.title = "My Game", .width = 800, .height = 600) {}
 *   SDL3Window(.title = "Debug") {}  // gets 1280x720 default
 *
 * Implementation in sdl3_module.c via CEL_Composition(SDL3Window).
 */
CEL_Define_Composition(SDL3Window,
    const char* title;
    int width;
    int height;
    SDL_WindowFlags flags;
    bool context;
    int target_fps;
);

/* Call macro for natural syntax */
#define SDL3Window(...) cel_init(SDL3Window, __VA_ARGS__)

/* ============================================================================
 * Frame Loop Types
 * ============================================================================ */

/*
 * Frame loop configuration. Set target_fps to 0 for uncapped / VSync-only.
 */
typedef struct SDL3_FrameConfig {
    int target_fps;     /* 0 = uncapped, >0 = cap to this FPS */
} SDL3_FrameConfig;

/*
 * Frame loop state singleton. Tracks running status and timing data.
 * Read via cel_read(SDL3_FrameState) in consumer systems.
 *
 * running:      false when all windows CLOSED or SDL_QUIT received
 * delta_time:   seconds elapsed since last frame
 * fps:          raw frames per second (1/dt)
 * smoothed_fps: exponential moving average FPS for display
 */
CEL_Define_State(SDL3_FrameState) {
    bool  running;
    float delta_time;
    float fps;
    float smoothed_fps;
};

/* Frame loop public API */
extern bool  sdl3_should_run(void);
extern float sdl3_delta(void);

/* ============================================================================
 * Input System Types
 * ============================================================================ */

#define SDL3_EVENT_QUEUE_CAPACITY 64

/*
 * Per-window raw event queue. Attached to each window entity alongside
 * SDL3_WindowComponent. Cleared every frame before new events are polled.
 *
 * Consumer systems iterate events[] from 0 to count-1 and switch on
 * SDL_Event.type. No convenience macros -- write your own for-loop.
 *
 * Window events (close, resize, minimize, restore, focus) are routed
 * directly to the window state machine and NEVER appear in this queue.
 * Only input events (keyboard, mouse, text, etc.) are buffered here.
 */
CEL_Component(SDL3_EventQueue) {
    SDL_Event events[SDL3_EVENT_QUEUE_CAPACITY];
    int       count;
};

/* ============================================================================
 * Renderer Types
 * ============================================================================ */

/*
 * Per-window renderer component. Attached alongside SDL3_WindowComponent
 * on each window entity. Created automatically in the window lifecycle
 * observer; destroyed before the window during CLOSING->CLOSED transition.
 *
 * clear_color: background color used by RenderClearSystem each frame.
 * Default: cornflower blue (100, 149, 237, 255). Changeable at runtime
 * by any system -- takes effect next frame.
 */
CEL_Component(SDL3_Renderer) {
    SDL_Renderer* renderer;
    SDL_Color     clear_color;
};

#endif /* CELS_SDL3_H */
