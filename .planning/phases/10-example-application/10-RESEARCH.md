# Phase 10: Example Application - Research

**Researched:** 2026-03-21
**Domain:** Demo application composing all cels-sdl3 v1 features (window, input, draw primitives, textures, text) into a cohesive mini landscape scene
**Confidence:** HIGH

## Summary

Phase 10 is a pure composition phase -- no new library APIs, no new CELS patterns, no new systems in the engine. The demo app exercises every v1 feature end-to-end as a single `examples/demo/main.c` file alongside bundled assets in `examples/assets/`. The app renders a mini landscape scene (blue sky via clear color, green ground via filled rect, horizon line, sun/building via outlined rect) at 1280x720, loads a pixel-art PNG sprite and a retro TTF font, displays text overlays (title, live FPS counter, instructions), and implements click-to-place interaction where mouse clicks accumulate colored shapes on screen.

The technical challenge is minimal: this is a consumer app using established APIs. The main risk is getting the asset paths correct (relative to the working directory), correctly combining draw primitives with textures and text in the right z-order, and making the FPS counter exercise text cache invalidation (updating text string every frame or on a timer).

All APIs used are from phases 6-9: `sdl3_renderable()` vtable for draw primitives, `SDL3_Sprite` component for textures, `sdl3_font_load()` + `SDL3_Text` component for text, `SDL3_EventQueue` for input handling, and `SDL3_use(NULL)` or `cels_register(SDL3_Engine)` for engine registration. The demo app follows the exact same patterns as the existing examples (minimal, frame-loop, input, renderer) but composes all of them together.

**Primary recommendation:** Single-file demo at `examples/demo/main.c` with assets in `examples/assets/`. Use `cels_register(SDL3_Engine)` for backward-compatible registration. Define one CEL_Compose(World) for setup, one input system, and one render system. Bundle a CC0/OFL pixel-art tree PNG and Press Start 2P TTF font. The FPS counter updates its string every frame, exercising text cache invalidation.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| cels-sdl3 | 0.1.0 | All v1 APIs: window, input, draw primitives, textures, text | This IS the library being demoed |
| CELS | v0.5.3 | ECS framework: cels_main, cels_register, cels_session, cels_step, CEL_System, CEL_Compose | Foundation framework |
| SDL3 | 3.4.2 | SDL_Event types, SDL_Color, SDL_FRect, SDL_FPoint, SDLK_ESCAPE | Underlying platform |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| stdio.h | C99 | snprintf for FPS string formatting, printf for console output | FPS counter text, exit message |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Single main.c | Multi-file example | Single file is the established pattern for all existing examples; keeps demo self-contained |
| cels_register(SDL3_Engine) | SDL3_use(NULL) | cels_register(SDL3_Engine) is backward-compatible and matches all existing examples |
| Bundled assets in examples/assets/ | Download at build time | Bundled assets are self-contained, no network dependency, version-controlled |

## Architecture Patterns

### Recommended Project Structure
```
examples/
  demo/
    main.c                 # NEW: full demo app (~250-350 lines)
  assets/
    tree.png               # NEW: bundled pixel-art sprite (CC0)
    PressStart2P.ttf       # NEW: bundled retro font (OFL)
    LICENSE-assets.txt      # NEW: license attribution for bundled assets
```

### Pattern 1: Single World Composition with All Features
**What:** One `CEL_Compose(World)` creates the SDL3 context and window, loads fonts, and captures the window entity ID for use by systems. This matches the existing example pattern but adds font loading.
**When to use:** Always -- the demo needs exactly one window.
**Example:**
```c
// Follows existing examples/minimal/main.c pattern
CEL_Compose(World) {
    cels_entity_t* win_id = cel_remember(cels_entity_t, 0);

    SDL3Window(.title = "cels-sdl3 Demo", .width = 1280, .height = 720,
               .context = true, .target_fps = 60) {
        *win_id = cels_get_current_entity();
    }

    // Font loading happens once (cel_remember guards re-execution)
    bool* fonts_loaded = cel_remember(bool, false);
    if (!*fonts_loaded) {
        sdl3_font_load(0, "examples/assets/PressStart2P.ttf", 16.0f);
        *fonts_loaded = true;
    }
}
```

### Pattern 2: Combined Input + Draw System
**What:** A single CEL_System at OnUpdate handles input (reading SDL3_EventQueue) and manages game state (click positions). A separate CEL_System at OnRender handles all drawing. This separates input/logic from rendering, following the established phase convention.
**When to use:** Always -- OnUpdate for logic, OnRender for drawing.
**Example:**
```c
// Input system reads events, manages click state
CEL_System(DemoInput, .phase = OnUpdate) {
    cel_query(SDL3_EventQueue);
    cel_each(SDL3_EventQueue) {
        for (int i = 0; i < SDL3_EventQueue->count; i++) {
            const SDL_Event* ev = &SDL3_EventQueue->events[i];
            switch (ev->type) {
            case SDL_EVENT_KEY_DOWN:
                if (ev->key.key == SDLK_ESCAPE) cel_quit();
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                // Record click position for shape placement
                break;
            }
        }
    }
}

// Render system draws everything
CEL_System(DemoRenderer, .phase = OnRender) {
    const SDL3_Renderable* draw = sdl3_renderable();
    cel_query(SDL3_Renderer);
    cel_each(SDL3_Renderer) {
        SDL_Renderer* r = SDL3_Renderer->renderer;
        // Draw landscape: ground, horizon, sun, building, placed shapes
        // Texture rendering handled by SpriteRenderSystem automatically
        // Text rendering handled by TextRenderSystem automatically
    }
}
```

### Pattern 3: Static Global State for Click-to-Place Shapes
**What:** Placed shapes are stored in a static array in the render system. Each click appends a new entry with position and a cycling color. The array has a generous fixed capacity (e.g., 256 shapes).
**When to use:** For accumulating click-placed shapes without ECS entity overhead.
**Example:**
```c
typedef struct PlacedShape {
    float x, y;
    SDL_Color color;
} PlacedShape;

#define MAX_PLACED_SHAPES 256
static PlacedShape s_placed[MAX_PLACED_SHAPES];
static int s_placed_count = 0;
static int s_color_index = 0;

static const SDL_Color SHAPE_COLORS[] = {
    {255, 100, 100, 255},  // soft red
    {100, 255, 100, 255},  // soft green
    {100, 100, 255, 255},  // soft blue
    {255, 255, 100, 255},  // yellow
    {255, 100, 255, 255},  // magenta
    {100, 255, 255, 255},  // cyan
};
```

### Pattern 4: Live FPS Counter via Text Update
**What:** The FPS counter text is an ECS entity with an SDL3_Text component. Each frame (or each second), the string is updated via `cel_update(SDL3_Text)` which triggers cache invalidation in the text render system. This exercises TEXT-03 (caching -- unchanged text is not re-rendered) and demonstrates cache invalidation when text changes.
**When to use:** For the FPS counter display.
**Example:**
```c
// In render system or a dedicated FPS update system
static char fps_buf[32];
const struct SDL3_FrameState* frame = cel_read(SDL3_FrameState);
snprintf(fps_buf, sizeof(fps_buf), "FPS: %.0f", frame->smoothed_fps);
// Update SDL3_Text component string to fps_buf
```

### Pattern 5: Scene Z-Ordering
**What:** All draw primitive calls use z-index to establish correct visual layering. Background elements use low z, interactive/placed shapes use mid z, and text overlays use high z.
**When to use:** Always -- the landscape scene needs correct layering.
**Recommended z-layers:**
```
z=0: Ground (filled rect)
z=0: Horizon line (same layer as ground is fine)
z=1: Static scene elements (building, sun outlines)
z=2: Bundled sprite (tree/house)
z=3: Click-placed shapes
z=10: Text overlays (title, FPS, instructions)
```

### Pattern 6: Clear Color as Sky
**What:** The window's clear color doubles as the sky background. Instead of drawing a sky rectangle, set `SDL3_Renderer->clear_color` to a sky blue color in the World composition or via `cel_update(SDL3_Renderer)`.
**When to use:** Always -- per the context decision "sky is the clear color (background)."
**Example:**
```c
// In World composition, after window creation
if (*win_id) {
    const SDL3_Renderer* rend = cel_watch(*win_id, SDL3_Renderer);
    // Or set via cel_update in a system
}
// Alternatively, the renderer default is cornflower blue (100, 149, 237)
// which is already a sky blue -- may be usable as-is or tweaked
```

### Anti-Patterns to Avoid
- **Creating ECS entities for each placed shape:** Overhead for what are simple draw calls. Use a static array and draw them in the render system via the Renderable vtable.
- **Loading fonts every frame:** Font loading must happen once (guarded by cel_remember or a static flag). Re-loading creates duplicate font slots.
- **Hardcoding absolute asset paths:** Use paths relative to the working directory (e.g., `examples/assets/tree.png`). The build system runs from the project root.
- **Allocating text strings on the stack then storing pointers in SDL3_Text:** String pointers must be stable. Use static buffers (for FPS counter) or string literals (for title/instructions).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| FPS calculation | Manual frame counting | `cel_read(SDL3_FrameState)->smoothed_fps` | Already computed by the frame loop system with exponential moving average |
| Delta time | `SDL_GetTicks()` math | `cel_delta()` or `sdl3_delta()` | Already computed via SDL_GetPerformanceCounter |
| Event polling | `SDL_PollEvent()` loop | `SDL3_EventQueue` component | Already buffered by InputSystem |
| Texture loading | `IMG_Load()` + `SDL_CreateTextureFromSurface()` | `SDL3_Sprite` component with `texture_path` | Declarative loading system handles it |
| Text caching | Manual surface-to-texture conversion | `SDL3_Text` component | Text render system handles caching |
| Font management | Direct `TTF_OpenFont()` | `sdl3_font_load(id, path, size)` | Global font registry with cleanup |

**Key insight:** The demo app should exercise cels-sdl3's PUBLIC API exclusively. Every feature should go through the library's designed consumer interface. If the demo needs to call raw SDL functions, that's a signal the library API is incomplete.

## Common Pitfalls

### Pitfall 1: Asset Path Resolution
**What goes wrong:** The demo runs but textures/fonts fail to load with "file not found" errors.
**Why it happens:** Working directory differs between running from project root vs. build directory. `examples/assets/tree.png` works from project root but fails from `build/`.
**How to avoid:** Document that the demo must be run from the project root. In CMakeLists.txt, consider setting the working directory for the demo target, or use `CMAKE_CURRENT_SOURCE_DIR` to construct absolute paths at compile time. The simplest approach: document "run from project root" and use relative paths.
**Warning signs:** Black rectangles where textures should be, missing text, SDL error messages about file paths.

### Pitfall 2: FPS String Buffer Lifetime
**What goes wrong:** The FPS counter displays garbage text or crashes.
**Why it happens:** The FPS string is formatted into a local buffer that goes out of scope. The SDL3_Text component stores a `const char*` pointer that dangles.
**How to avoid:** Use a `static char fps_buf[32]` so the buffer persists across frames. The pointer stored in SDL3_Text remains valid as long as the buffer is static.
**Warning signs:** Garbled FPS text, intermittent crashes in text rendering.

### Pitfall 3: Missing Font Load Before Text Use
**What goes wrong:** Text entities are created but nothing renders, or crashes occur in the text system.
**Why it happens:** `sdl3_font_load()` was not called before creating SDL3_Text entities that reference the fontId.
**How to avoid:** Load fonts in the World composition (guarded by cel_remember) before any system that creates text entities runs. Font loading happens at OnLoad-equivalent time (during composition), before OnRender systems.
**Warning signs:** No text visible, NULL font errors in text system.

### Pitfall 4: Sprite Entity Needs Window Association
**What goes wrong:** Sprite loads but doesn't render, or renders on the wrong window.
**Why it happens:** The sprite's SDL3_Sprite component is on an entity that isn't associated with any window's renderer.
**How to avoid:** Place the sprite entity as a child of the window entity, or ensure the texture load system can find the appropriate renderer. The exact mechanism depends on Phase 7's implementation -- either the sprite is co-located on the window entity or there's a window reference field.
**Warning signs:** Sprite in READY state but not visible.

### Pitfall 5: Draw Primitive Z-Order with Sprite/Text
**What goes wrong:** Draw primitives render on top of sprites/text or vice versa unexpectedly.
**Why it happens:** Draw primitives go through the z-ordered draw buffer and flush at PostRender. Sprites and text may render at OnRender phase. The ordering depends on whether sprites/text also participate in the z-ordering system or render independently.
**How to avoid:** Understand the rendering pipeline order: PreRender (clear) -> OnRender (developer draws + sprite render + text render) -> PostRender (draw buffer flush + present). If sprites/text render at OnRender and draw primitives flush at PostRender, primitives always draw ON TOP of sprites/text. Design the scene knowing this ordering, or ensure all rendering goes through the draw buffer.
**Warning signs:** Filled rects covering sprites, or sprites covering all drawn shapes.

### Pitfall 6: Text Entity Lifecycle for Dynamic FPS
**What goes wrong:** FPS counter text entity is created every frame, or text updates don't trigger cache invalidation.
**Why it happens:** Creating new text entities every frame leaks TTF_Text objects. Not using cel_update when changing the string doesn't signal the text system.
**How to avoid:** Create text entities once (in World composition or guarded by a static flag). Update the string via `cel_update(SDL3_Text)` which signals the text render system to re-sync with TTF_SetTextString.
**Warning signs:** Memory growth over time, FPS text frozen at initial value.

## Code Examples

### Complete Demo App Skeleton
```c
// examples/demo/main.c
// Source: composed from existing example patterns (minimal, input, renderer, frame-loop)

#include <cels/cels.h>
#include <cels_sdl3.h>
#include <stdio.h>

/* ============================================================================
 * Scene State -- click-placed shapes
 * ============================================================================ */

typedef struct PlacedShape {
    float x, y;
    SDL_Color color;
} PlacedShape;

#define MAX_PLACED_SHAPES 256
static PlacedShape s_placed[MAX_PLACED_SHAPES];
static int s_placed_count = 0;

/* ============================================================================
 * World -- window + fonts + scene entities
 * ============================================================================ */

CEL_Compose(World) {
    cels_entity_t* win_id = cel_remember(cels_entity_t, 0);
    bool* init_done = cel_remember(bool, false);

    SDL3Window(.title = "cels-sdl3 Demo", .width = 1280, .height = 720,
               .context = true, .target_fps = 60) {
        *win_id = cels_get_current_entity();
    }

    if (*win_id && !*init_done) {
        *init_done = true;

        // Load font
        sdl3_font_load(0, "examples/assets/PressStart2P.ttf", 16.0f);

        // Set sky-blue clear color
        // (renderer default is cornflower blue -- tweak if needed)

        // Create sprite entity for tree
        // Create text entities for title, FPS, instructions
    }
}

/* ============================================================================
 * Input System -- ESC to quit, click to place shapes
 * ============================================================================ */

CEL_System(DemoInput, .phase = OnUpdate) {
    cel_query(SDL3_EventQueue);
    cel_each(SDL3_EventQueue) {
        for (int i = 0; i < SDL3_EventQueue->count; i++) {
            const SDL_Event* ev = &SDL3_EventQueue->events[i];
            switch (ev->type) {
            case SDL_EVENT_KEY_DOWN:
                if (ev->key.key == SDLK_ESCAPE) {
                    cel_quit();
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (s_placed_count < MAX_PLACED_SHAPES) {
                    // Add shape at click position with cycling color
                    s_placed[s_placed_count++] = (PlacedShape){
                        .x = ev->button.x,
                        .y = ev->button.y,
                        .color = /* cycling color */
                    };
                }
                break;
            }
        }
    }
}

/* ============================================================================
 * Render System -- landscape scene + placed shapes
 * ============================================================================ */

CEL_System(DemoRenderer, .phase = OnRender) {
    const SDL3_Renderable* draw = sdl3_renderable();

    cel_query(SDL3_Renderer);
    cel_each(SDL3_Renderer) {
        SDL_Renderer* r = SDL3_Renderer->renderer;

        // Ground (bottom third of screen)
        draw->filled_rect(r,
            (SDL_FRect){0, 480, 1280, 240},
            (SDL_Color){76, 153, 0, 255}, 0);  // grass green

        // Horizon line
        draw->line(r,
            (SDL_FPoint){0, 480}, (SDL_FPoint){1280, 480},
            (SDL_Color){60, 120, 0, 255}, 0);

        // Sun (outlined circle approximation -- outlined rect)
        draw->outlined_rect(r,
            (SDL_FRect){1050, 80, 80, 80},
            (SDL_Color){255, 220, 50, 255}, 1);

        // Building
        draw->filled_rect(r,
            (SDL_FRect){200, 380, 120, 100},
            (SDL_Color){160, 82, 45, 255}, 1);
        draw->outlined_rect(r,
            (SDL_FRect){200, 380, 120, 100},
            (SDL_Color){100, 50, 25, 255}, 1);

        // Click-placed shapes
        for (int i = 0; i < s_placed_count; i++) {
            draw->filled_rect(r,
                (SDL_FRect){s_placed[i].x - 8, s_placed[i].y - 8, 16, 16},
                s_placed[i].color, 3);
        }
    }
}

/* ============================================================================
 * FPS Update System -- updates FPS text string
 * ============================================================================ */

CEL_System(FPSUpdater, .phase = OnUpdate) {
    static char fps_buf[32] = "FPS: --";

    cel_run {
        const struct SDL3_FrameState* frame = cel_read(SDL3_FrameState);
        snprintf(fps_buf, sizeof(fps_buf), "FPS: %.0f", frame->smoothed_fps);
        // Update the SDL3_Text entity's string to fps_buf
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

cels_main() {
    cels_register(SDL3_Engine);
    cels_register(DemoInput);
    cels_register(DemoRenderer);
    cels_register(FPSUpdater);
    cels_session(World) {
        while (sdl3_should_run()) {
            float dt = sdl3_delta();
            cels_step(dt);
        }
    }
    printf("cels-sdl3 Demo complete -- clean exit\n");
}
```

### CMakeLists.txt Addition
```cmake
# In the examples section of CMakeLists.txt
add_executable(demo examples/demo/main.c)
target_link_libraries(demo PRIVATE cels-sdl3)
set_target_properties(demo PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
```

### Asset License File
```text
# examples/assets/LICENSE-assets.txt

## PressStart2P.ttf
Font: Press Start 2P
Author: codeman38 (cody@zone38.net)
License: SIL Open Font License, Version 1.1
Source: https://fonts.google.com/specimen/Press+Start+2P

## tree.png
[Specific attribution based on chosen asset]
License: CC0 1.0 Universal (Public Domain)
Source: [OpenGameArt.org URL]
```

### Scene Layout Diagram (1280x720)
```
+--------------------------------------------------+  y=0
|                                                    |
|  "cels-sdl3 Demo"        (text, z=10)             |
|  "FPS: 60"                (text, z=10)             |
|  "Click to plant / ESC"   (text, z=10)      [SUN] |  outlined rect ~(1050,80)
|                                               80x80|
|                                                    |
|            [TREE]                                  |
|            sprite                                  |
|   [BUILDING]                                       |  y~380
|   filled+outlined rect                             |
|----horizon line (z=0)--------------------------------|  y=480
|   GROUND - filled rect (z=0)                       |
|   grass green                                      |
|                                                    |
+--------------------------------------------------+  y=720

Sky = clear color (cornflower blue or lighter sky blue)
Placed shapes appear at click positions (z=3)
```

## Bundled Assets

### Font: Press Start 2P
**License:** SIL Open Font License 1.1 (allows bundling, modification, redistribution)
**Source:** https://fonts.google.com/specimen/Press+Start+2P
**Why:** Iconic retro pixel font, widely recognized, high quality TTF, OFL-licensed (safe to bundle), maintained by Google Fonts. 8x8 pixel grid design matches the landscape pixel-art aesthetic.
**File:** `PressStart2P-Regular.ttf` (~30KB)
**Confidence:** HIGH -- OFL is well-understood, Google Fonts hosts it

### Sprite: Pixel Art Tree (or similar landscape element)
**License:** CC0 1.0 (public domain) or OGA-BY
**Source options:**
- OpenGameArt.org "Pixel Art - Simple Trees" (CC0)
- OpenGameArt.org "trees" collection (CC0)
- A hand-created simple 32x32 or 48x48 pixel tree PNG
**Why:** Fits the landscape scene concept, small file size, appropriate licensing.
**Recommendation:** Create a simple hand-pixeled tree sprite (16x32 or 32x48 pixels) as a project asset. This avoids third-party attribution requirements entirely and is simple enough to create in any pixel editor. Alternatively, download a CC0 tree from OpenGameArt.
**Confidence:** HIGH -- either hand-create or use CC0 asset

### Asset Directory
Assets live in `examples/assets/` (shared across all examples that need assets, not inside `examples/demo/`). This is per the context decision: "Assets directory: examples/assets/ -- self-contained alongside example source."

## Feature Checklist (INTG-05 Traceability)

The demo must exercise ALL v1 features. Map to requirement text:

| Feature | How Demonstrated | Requirement |
|---------|-----------------|-------------|
| Colored window | Clear color set to sky blue | INTG-05 "colored window" |
| Input handling | ESC to quit + mouse click to place shapes | INTG-05 "input handling" |
| Filled rect | Ground, building body, click-placed shapes | INTG-05 "draw primitives" |
| Outlined rect | Sun, building outline | INTG-05 "draw primitives" |
| Line | Horizon separating sky and ground | INTG-05 "draw primitives" |
| Texture | Bundled pixel-art tree/house sprite in scene | INTG-05 "texture" |
| Text | Title, FPS counter, instructions overlay | INTG-05 "text" |

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Separate examples per feature | Single demo exercising all features together | Phase 10 | Validates integration, serves as living documentation |
| Direct SDL calls in demo | cels-sdl3 public API exclusively | Phase 10 | Demo validates the library's consumer-facing API |

**Deprecated/outdated:**
- Direct SDL_RenderFillRect calls in demo code: use `sdl3_renderable()->filled_rect()` vtable instead
- TTF_RenderText_Blended in demo: use SDL3_Text component with automatic caching instead
- IMG_LoadTexture directly: use SDL3_Sprite component with declarative loading instead

## Open Questions

1. **Sprite-to-window association mechanism**
   - What we know: SDL3_Sprite needs to be associated with a window's renderer. Phase 7 research suggests co-locating sprite on the window entity or having a window reference field.
   - What's unclear: The exact Phase 7 implementation may vary. The demo needs to work with whatever mechanism Phase 7 provides.
   - Recommendation: The planner should reference Phase 7's PLAN files for the exact sprite creation pattern. If sprites must be on the same entity as the window, the demo creates sprite components on the window entity. If sprites reference a window entity, the demo stores the window entity ID and passes it.

2. **Text entity creation pattern**
   - What we know: SDL3_Text is a component. Text entities need to be created and associated with the window. The FPS counter needs updating each frame.
   - What's unclear: Whether text entities are created in composition (World) or in a system, and how the FPS update system references the text entity.
   - Recommendation: Create text entities in the World composition, store their entity IDs via cel_remember, and update them in the FPS system by directly modifying the component via cel_update. The planner should reference Phase 8's PLAN for exact text creation syntax.

3. **Rendering pipeline ordering: primitives vs sprites vs text**
   - What we know: Draw primitives buffer during OnRender and flush at PostRender. Sprites render at OnRender. Text renders at OnRender. The visual stacking depends on execution order within phases.
   - What's unclear: Whether sprites/text are z-ordered relative to draw primitives, or whether they render independently.
   - Recommendation: The planner should check Phase 6/7/8 PLAN files for the exact rendering pipeline. If sprites and text render independently from the draw buffer (directly to SDL_Renderer during OnRender), they will appear UNDER the z-sorted primitives (which flush at PostRender). This may require the demo to adjust z-values or rendering order. Alternatively, the demo can accept any layering order as long as all features are visible.

4. **How to download/create the bundled sprite**
   - What we know: A pixel-art PNG is needed in examples/assets/.
   - What's unclear: Whether to download from OpenGameArt or create one.
   - Recommendation: Create a simple pixel tree sprite programmatically or by hand. For the planner: include a task to either create a minimal PNG using a script/tool or download a CC0 sprite and add attribution. The simplest approach is to use any small CC0 PNG.

## Sources

### Primary (HIGH confidence)
- Existing cels-sdl3 examples (local: examples/minimal, examples/input, examples/renderer, examples/frame-loop) -- established consumer patterns for all existing APIs
- cels_sdl3.h (local) -- complete public API surface with all component types and function declarations
- Phase 6 RESEARCH.md (local) -- Renderable vtable API: sdl3_renderable(), filled_rect, outlined_rect, line with z-index
- Phase 7 RESEARCH.md (local) -- SDL3_Sprite component, texture_path declarative loading, SDL3_TextureState
- Phase 8 RESEARCH.md (local) -- SDL3_Text component, sdl3_font_load, TTF_Text caching, TTF_DrawRendererText
- Phase 9 RESEARCH.md (local) -- SDL3_use(NULL) / cels_register(SDL3_Engine), sdl3_report_error, error callback
- Phase 10 CONTEXT.md (local) -- all user decisions for scene design, interactivity, visual composition

### Secondary (MEDIUM confidence)
- [Google Fonts: Press Start 2P](https://fonts.google.com/specimen/Press+Start+2P) -- OFL 1.1 licensed retro pixel font, verified availability
- [OpenGameArt.org CC0 pixel art](https://opengameart.org/content/pixel-art-simple-trees) -- CC0 tree sprites available
- [OpenGameArt.org minimalist pixel fonts](https://opengameart.org/content/minimalist-pixel-fonts) -- CC0 pixel fonts (alternative to Press Start 2P)

### Tertiary (LOW confidence)
- (none -- this phase is pure composition of verified APIs)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new libraries, purely composing existing cels-sdl3 APIs
- Architecture: HIGH -- follows established example patterns verbatim (CEL_Compose, CEL_System phases, cels_main)
- Pitfalls: HIGH -- asset paths, string lifetime, font loading order are concrete and well-understood
- Scene design: HIGH -- all visual elements mapped directly to specific API calls from phases 6-9
- Asset licensing: HIGH -- OFL (Press Start 2P) and CC0 (OpenGameArt) are well-understood licenses

**Research date:** 2026-03-21
**Valid until:** 2026-04-21 (stable composition of established APIs, 30-day validity)
