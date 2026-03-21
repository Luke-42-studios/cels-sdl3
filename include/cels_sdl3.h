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
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
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
 * Draw Primitive Types
 * ============================================================================
 *
 * Draw commands are buffered during OnRender, sorted by z-index, and flushed
 * to SDL3 before present. Developer calls through the SDL3_Renderable vtable:
 *
 *   const SDL3_Renderable* draw = sdl3_renderable();
 *   draw->filled_rect(r, rect, color, z);
 *
 * Alpha blending is OFF by default. Enable with:
 *   sdl3_set_blend_mode(renderer, SDL_BLENDMODE_BLEND);
 */

typedef enum SDL3_DrawCmdType {
    SDL3_DRAW_FILLED_RECT,
    SDL3_DRAW_OUTLINED_RECT,
    SDL3_DRAW_LINE
} SDL3_DrawCmdType;

typedef struct SDL3_DrawCmd {
    SDL3_DrawCmdType type;
    int z;          /* z-index for sorting */
    int order;      /* creation order tiebreaker */
    SDL_Color color;
    union {
        SDL_FRect rect;                     /* filled/outlined rect */
        struct { SDL_FPoint a, b; } line;   /* line endpoints */
    };
} SDL3_DrawCmd;

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
 *
 * draw_cmds/draw_count/draw_capacity/draw_next_order: per-window draw
 * command buffer. Embedded here (not a separate component) for tight
 * coupling with the renderer.
 */
CEL_Component(SDL3_Renderer) {
    SDL_Renderer*   renderer;
    TTF_TextEngine* text_engine;
    SDL_Color       clear_color;
    /* Draw buffer -- embedded for tight coupling with renderer */
    SDL3_DrawCmd*   draw_cmds;
    int             draw_count;
    int             draw_capacity;
    int             draw_next_order;
};

/* ============================================================================
 * Renderable Vtable
 * ============================================================================ */

typedef struct SDL3_Renderable {
    void (*filled_rect)(SDL_Renderer* r, SDL_FRect rect, SDL_Color color, int z);
    void (*filled_rects)(SDL_Renderer* r, const SDL_FRect* rects,
                         const SDL_Color* colors, int count, int z);
    void (*outlined_rect)(SDL_Renderer* r, SDL_FRect rect, SDL_Color color, int z);
    void (*outlined_rects)(SDL_Renderer* r, const SDL_FRect* rects,
                           const SDL_Color* colors, int count, int z);
    void (*line)(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_Color color, int z);
} SDL3_Renderable;

extern const SDL3_Renderable* sdl3_renderable(void);
extern void SDL3_Renderable_use(void);

extern void sdl3_set_blend_mode(SDL_Renderer* renderer, SDL_BlendMode mode);

/* ============================================================================
 * Texture Types
 * ============================================================================
 *
 * Texture loading state machine and sprite component for image rendering.
 * Developer sets texture_path on an SDL3_Sprite; a loading system detects
 * NONE state and loads via IMG_LoadTexture into a per-renderer cache.
 */

/*
 * Texture loading state machine.
 *
 * NONE     -> LOADING -> READY   (success)
 * NONE     -> LOADING -> FAILED  (file not found, format error)
 * READY    -> UNLOADED           (renderer destroyed)
 */
typedef enum SDL3_TextureState {
    SDL3_TEXTURE_NONE = 0,      /* No texture path set */
    SDL3_TEXTURE_LOADING,       /* Path set, loading in progress */
    SDL3_TEXTURE_READY,         /* Texture loaded, renderable */
    SDL3_TEXTURE_FAILED,        /* Load failed (path invalid, format unsupported) */
    SDL3_TEXTURE_UNLOADED       /* Renderer destroyed, texture invalidated */
} SDL3_TextureState;

/*
 * Sprite component for image-based rendering.
 *
 * Developer sets texture_path and dst_rect; the TextureLoadSystem handles
 * the rest. texture_handle is an opaque 1-based index into the per-renderer
 * texture cache (0 = none). State tracks loading progress.
 *
 * Rendering parameters support sub-image (spritesheet), rotation, flip,
 * and per-sprite alpha modulation.
 */
CEL_Component(SDL3_Sprite) {
    /* Texture reference */
    const char*        texture_path;    /* File path (relative to asset base dir, or absolute) */
    uint32_t           texture_handle;  /* Opaque 1-based index into per-renderer cache (0 = none) */
    SDL3_TextureState  state;           /* Loading state machine */

    /* Rendering parameters */
    SDL_FRect          src_rect;        /* Source rect (0,0,0,0 = entire texture) */
    SDL_FRect          dst_rect;        /* Destination rect (position + size in pixels) */
    double             angle;           /* Rotation in degrees (clockwise) */
    SDL_FPoint         center;          /* Rotation center (0,0 = use dst_rect center) */
    SDL_FlipMode       flip;            /* SDL_FLIP_NONE / HORIZONTAL / VERTICAL */
    Uint8              alpha;           /* Per-sprite alpha (255 = opaque, 0 = invisible) */
};

/* Texture asset directory configuration */
extern void sdl3_set_asset_base_dir(const char* path);

/* ============================================================================
 * Text Rendering Types
 * ============================================================================
 *
 * Developer sets SDL3_Text on an entity to render text each frame.
 * SDL3_TextHandle is managed by the text system -- not set by developers.
 *
 * Each fontId encodes a specific family+size combination. Load fonts with
 * sdl3_font_load(). String pointers must be stable (static or heap) -- the
 * component stores a pointer, not a copy.
 *
 *   sdl3_font_load(0, "fonts/Roboto.ttf", 16.0f);
 *   cel_has(SDL3_Text, .string = "Hello", .font_id = 0,
 *           .color = {255,255,255,255}, .x = 100, .y = 50);
 */

/* SDL3_Text alignment */
typedef enum SDL3_TextAlign {
    SDL3_TEXT_ALIGN_LEFT = 0,
    SDL3_TEXT_ALIGN_CENTER,
    SDL3_TEXT_ALIGN_RIGHT,
} SDL3_TextAlign;

/* Developer-facing text component */
CEL_Component(SDL3_Text) {
    const char*     string;     /* text content -- must be stable pointer (static or heap) */
    int             font_id;    /* index into global font array */
    SDL_Color       color;      /* text color (default: white {255,255,255,255}) */
    float           x, y;       /* render position (top-left) */
    SDL3_TextAlign  align;      /* horizontal alignment relative to x position */
    int             wrap_width; /* 0 = no wrap, >0 = wrap at this pixel width */
};

/* Internal cached handle -- managed by text system, not set by developer */
CEL_Component(SDL3_TextHandle) {
    TTF_Text*    ttf_text;     /* cached TTF_Text object */
    const char*  last_string;  /* last synced string pointer for change detection */
    SDL_Color    last_color;   /* last synced color */
    int          last_wrap;    /* last synced wrap width */
    int          last_font_id; /* last synced font_id */
};

/* Font management */
#define SDL3_MAX_FONTS 32
extern bool sdl3_font_load(int font_id, const char* path, float pt_size);
extern void sdl3_font_close(int font_id);
extern void sdl3_fonts_close_all(void);

#endif /* CELS_SDL3_H */
