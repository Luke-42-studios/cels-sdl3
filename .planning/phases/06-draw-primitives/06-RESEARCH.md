# Phase 6: Draw Primitives - Research

**Researched:** 2026-03-21
**Domain:** SDL3 draw primitives, Feature/Provider vtable pattern, draw command buffering with z-index sorting
**Confidence:** HIGH

## Summary

Phase 6 introduces the Feature/Provider rendering model and three draw primitive types (filled rects, outlined rects, lines). The SDL3 draw API is straightforward: `SDL_RenderFillRect`, `SDL_RenderRect`, and `SDL_RenderLine` all take the renderer and use the current draw color set via `SDL_SetRenderDrawColor`. Batch variants exist: `SDL_RenderFillRects`, `SDL_RenderRects`, and `SDL_RenderLines`. All functions return `bool` (SDL3 convention) and must be called from the main thread.

The main architectural challenge is the Feature/Provider vtable pattern. CELS has `cel_module_provides(Feature)` for declaring module capabilities as strings, but does NOT have a built-in vtable dispatch mechanism. The "Renderable->filled_rect(r, rect, color, z)" pattern described in the context decisions is a custom C vtable struct that we design and implement ourselves. The vtable struct holds function pointers for each draw operation, and the SDL3 provider fills in those pointers with its implementation. Developer code calls through the vtable, enabling future backend swapping (SDL_GPU) without changing user systems.

The second major concern is the draw buffer with z-index sorting. Draw calls are NOT immediate -- they are buffered during the frame, sorted by z-index (with creation order as tiebreaker), then flushed during the present cycle. This enables correct z-ordering across all systems regardless of system execution order. The buffer also enables batch optimization: consecutive same-type, same-color draws can be coalesced into single SDL batch calls after sorting.

**Primary recommendation:** Define a `Renderable` vtable struct with function pointers for filled_rect, outlined_rect, line, and their batch variants. Implement a per-window draw command buffer (dynamic array of tagged union commands). Buffer draw calls during OnRender phase, sort by z-index, then flush to SDL3 before present. Register the vtable via `SDL3_Renderable_use(world)` which developer calls explicitly.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| SDL3 | 3.4.2 | SDL_RenderFillRect, SDL_RenderRect, SDL_RenderLine, SDL_RenderFillRects, SDL_RenderRects, SDL_RenderLines, SDL_SetRenderDrawColor, SDL_SetRenderDrawBlendMode | Already in project; provides all 2D draw primitives |

No additional libraries needed. All draw primitive functionality is built into SDL3 core.

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| (none) | - | - | Draw primitives are pure SDL3 + custom vtable + buffer |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Custom vtable struct | CELS cel_module_provides (string-based) | cel_module_provides is purely declarative metadata; it does not provide function pointer dispatch. Custom vtable is required for Renderable->filled_rect() syntax. |
| Draw buffer + sort + flush | Immediate SDL draw calls | Immediate calls cannot support z-ordering across systems. Buffering is required by the context decision. |
| Dynamic draw buffer | Fixed-size draw buffer | Dynamic avoids arbitrary capacity limits; initial capacity of 256 with doubling is reasonable for typical frame workloads |
| Per-window draw buffer | Global draw buffer | Per-window is correct for multi-window support -- each window has its own renderer and its own z-sorted draw list |

## Architecture Patterns

### Recommended Project Structure
```
src/
  renderer/
    sdl3_renderer.c       # EXISTING: renderer create/destroy
    sdl3_draw.c            # NEW: draw primitives implementation, vtable provider, draw buffer
  sdl3_module.c           # MODIFIED: add DrawFlushSystem, register Renderable vtable
  sdl3_internal.h         # MODIFIED: add draw buffer types and function declarations
include/
  cels_sdl3.h            # MODIFIED: add Renderable vtable type, draw API declarations
examples/
  draw-primitives/
    main.c                # NEW: example drawing shapes with z-ordering
```

### Pattern 1: Renderable Feature as C Vtable Struct
**What:** A struct of function pointers that abstracts draw operations. The developer receives a `const Renderable*` pointer and calls draw functions through it. The SDL3 provider fills in the function pointers with its implementation.
**When to use:** Always -- this is the core abstraction enabling backend swapping.
**Example:**
```c
// Source: designed from CONTEXT.md decisions + cels-ncurses TUI_DrawContext pattern

// Public header (cels_sdl3.h)
typedef struct Renderable {
    void (*filled_rect)(SDL_Renderer* r, SDL_FRect rect, SDL_Color color, int z);
    void (*filled_rects)(SDL_Renderer* r, const SDL_FRect* rects,
                         const SDL_Color* colors, int count, int z);
    void (*outlined_rect)(SDL_Renderer* r, SDL_FRect rect, SDL_Color color, int z);
    void (*outlined_rects)(SDL_Renderer* r, const SDL_FRect* rects,
                           const SDL_Color* colors, int count, int z);
    void (*line)(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_Color color, int z);
} Renderable;

// Developer usage in a system:
CEL_System(GameRenderer, .phase = OnRender) {
    cel_query(SDL3_Renderer);
    cel_each(SDL3_Renderer) {
        const Renderable* draw = sdl3_renderable();  // get vtable pointer
        SDL_Renderer* r = SDL3_Renderer->renderer;

        draw->filled_rect(r, (SDL_FRect){10, 10, 100, 50},
                          (SDL_Color){255, 0, 0, 255}, 0);
        draw->outlined_rect(r, (SDL_FRect){120, 10, 100, 50},
                            (SDL_Color){0, 255, 0, 255}, 0);
        draw->line(r, (SDL_FPoint){0, 0}, (SDL_FPoint){200, 200},
                   (SDL_Color){255, 255, 0, 255}, 1);
    }
}
```

### Pattern 2: Draw Command Buffer (Tagged Union)
**What:** All draw calls buffer commands into a per-window dynamic array instead of issuing SDL calls immediately. Each command stores the primitive type, parameters, and z-index. After all OnRender systems have run, a flush system sorts by z-index and issues the actual SDL calls.
**When to use:** Always -- required by the z-ordering decision.
**Example:**
```c
// Source: designed from CONTEXT.md "Draw calls buffered during frame, sorted by z-index"

typedef enum SDL3_DrawCommandType {
    SDL3_DRAW_FILLED_RECT,
    SDL3_DRAW_OUTLINED_RECT,
    SDL3_DRAW_LINE,
} SDL3_DrawCommandType;

typedef struct SDL3_DrawCommand {
    SDL3_DrawCommandType type;
    int z;             // z-index for sorting
    int order;         // creation order for tiebreaking
    SDL_Color color;
    union {
        SDL_FRect rect;                     // for filled/outlined rect
        struct { SDL_FPoint a, b; } line;   // for line
    };
} SDL3_DrawCommand;

typedef struct SDL3_DrawBuffer {
    SDL3_DrawCommand* commands;
    int count;
    int capacity;
    int next_order;    // monotonically increasing counter for tiebreaking
} SDL3_DrawBuffer;
```

### Pattern 3: Per-Window Draw Buffer via Component
**What:** The draw buffer is stored as a component on the window entity alongside SDL3_Renderer. This keeps buffers per-window for multi-window support. The buffer is cleared at the start of each frame (in PreRender after clear), filled during OnRender by draw calls, and flushed in PostRender before present.
**When to use:** Always -- multi-window requires per-window buffers.
**Example:**
```c
// Component on window entity
CEL_Component(SDL3_DrawBuffer) {
    SDL3_DrawCommand* commands;
    int count;
    int capacity;
    int next_order;
};

// PreRender: clear buffer (reset count, keep allocation)
// OnRender: draw calls append to buffer
// PostRender: sort by (z, order), flush to SDL3, then present
```

### Pattern 4: Flush System Sort-and-Batch
**What:** The flush system sorts the draw buffer by (z-index ascending, creation order ascending), then iterates the sorted commands. Consecutive commands of the same type with the same color can be batched into single SDL batch calls (SDL_RenderFillRects, SDL_RenderRects) for efficiency.
**When to use:** Always during PostRender, before SDL_RenderPresent.
**Example:**
```c
// PostRender phase, BEFORE RenderPresentSystem
CEL_System(SDL3_DrawFlushSystem, .phase = PostRender) {
    cel_query(SDL3_WindowComponent, SDL3_Renderer, SDL3_DrawBuffer);
    cel_each(SDL3_WindowComponent, SDL3_Renderer, SDL3_DrawBuffer) {
        if (/* skip non-renderable states */) continue;

        // Sort buffer by (z, order)
        qsort(SDL3_DrawBuffer->commands, SDL3_DrawBuffer->count,
              sizeof(SDL3_DrawCommand), draw_command_compare);

        // Flush each command to SDL3
        SDL_Renderer* r = SDL3_Renderer->renderer;
        for (int i = 0; i < SDL3_DrawBuffer->count; i++) {
            SDL3_DrawCommand* cmd = &SDL3_DrawBuffer->commands[i];
            SDL_SetRenderDrawColor(r, cmd->color.r, cmd->color.g,
                                   cmd->color.b, cmd->color.a);
            switch (cmd->type) {
            case SDL3_DRAW_FILLED_RECT:
                SDL_RenderFillRect(r, &cmd->rect);
                break;
            case SDL3_DRAW_OUTLINED_RECT:
                SDL_RenderRect(r, &cmd->rect);
                break;
            case SDL3_DRAW_LINE:
                SDL_RenderLine(r, cmd->line.a.x, cmd->line.a.y,
                               cmd->line.b.x, cmd->line.b.y);
                break;
            }
        }
    }
}
```

### Pattern 5: Renderer Reference Access Pattern
**What:** Developer systems need the SDL_Renderer* to pass to draw calls. The vtable function pointers take SDL_Renderer* as first parameter. Developer queries SDL3_Renderer component via cel_each and extracts the renderer pointer.
**When to use:** In every developer draw system.
**Example:**
```c
// Developer accesses renderer via standard ECS query
CEL_System(MyDrawSystem, .phase = OnRender) {
    cel_query(SDL3_Renderer);
    cel_each(SDL3_Renderer) {
        SDL_Renderer* r = SDL3_Renderer->renderer;
        const Renderable* draw = sdl3_renderable();
        draw->filled_rect(r, rect, color, z);
    }
}
```

### Anti-Patterns to Avoid
- **Immediate SDL draw calls in user systems:** Bypasses z-ordering. All draws must go through the Renderable vtable which buffers commands.
- **Storing draw color as renderer state:** SDL3 uses global-state SDL_SetRenderDrawColor, but our vtable takes color per-call. Never make the developer call SDL_SetRenderDrawColor manually.
- **Making the draw buffer a global singleton:** Breaks multi-window. Buffer must be per-window entity.
- **Sorting during draw call insertion (insertion sort):** More complex than sort-at-flush for minimal benefit. Most frames have z=0 for everything, so qsort at flush is sufficient.
- **Using cel_update for draw buffer writes:** The draw buffer is mutated by appending commands, but this is internal SDL3 state mutation (like SDL backbuffer writes), not ECS-tracked component mutation. Access via the window table pattern (mutable pointer passed to cross-TU function) instead.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Rectangle drawing | Custom pixel-by-pixel fill | SDL_RenderFillRect / SDL_RenderRect | SDL3 uses GPU-accelerated rendering; custom fill would be CPU-bound |
| Line drawing | Bresenham's algorithm | SDL_RenderLine | SDL3 handles anti-aliasing and subpixel precision internally |
| Batch rendering | Manual loop calling single-draw functions | SDL_RenderFillRects / SDL_RenderRects | SDL3 batches these into single GPU draw calls internally (batching enabled by default since SDL 3.x) |
| Draw command sorting | Custom sort algorithm | qsort from stdlib | Standard library qsort is well-optimized and correct; draw buffers are small enough that O(n log n) is fine |
| Dynamic array for draw buffer | Custom linked list or ring buffer | realloc-based dynamic array | Cache-friendly, sortable, simple -- linked lists would kill sort performance |

**Key insight:** SDL3 already implements render batching internally for Direct3D, Metal, and Vulkan backends. Our draw buffer exists for z-ordering correctness, not for batching optimization. The batch optimization (coalescing consecutive same-type draws) is a bonus that reduces SDL3 API call overhead, but SDL3's internal batching handles the GPU-side batching regardless.

## Common Pitfalls

### Pitfall 1: SDL_SetRenderDrawColor State Leakage
**What goes wrong:** SDL_SetRenderDrawColor is global state on the renderer. If the flush system sets a color for draw command N but forgets to set it again for command N+1 (because they happen to have the same color), this works. But if the developer or another system calls SDL_SetRenderDrawColor between flush and present, the last draw commands get the wrong color.
**Why it happens:** SDL3's draw color is a stateful setter, not a per-call parameter.
**How to avoid:** Always call SDL_SetRenderDrawColor immediately before each SDL draw call in the flush loop. The overhead is negligible (it's just setting 4 bytes on the renderer struct). This guarantees correctness regardless of what other code does.
**Warning signs:** Shapes appearing in the wrong color, or all shapes appearing the same color as the clear color.

### Pitfall 2: Draw Buffer Not Cleared Between Frames
**What goes wrong:** Shapes from the previous frame persist and are drawn again, causing duplicates or ghost images.
**Why it happens:** The draw buffer's count is not reset to 0 at the start of each frame.
**How to avoid:** Clear the draw buffer (set count = 0, keep allocation) at the start of PreRender, alongside the renderer clear. This can be done in the RenderClearSystem or a dedicated DrawBufferClearSystem.
**Warning signs:** Shapes accumulating over multiple frames, drawing becoming slower over time.

### Pitfall 3: Per-TU Static ID Constraint for Draw Buffer Component
**What goes wrong:** The SDL3_DrawBuffer component is added to window entities but its _id is 0 (uninitialized) when the on_create observer fires.
**Why it happens:** Same issue as SDL3_EventQueue and SDL3_Renderer from earlier phases -- CEL_Component _register() is a no-op; actual ID assignment requires cels_ensure_component.
**How to avoid:** Call `cels_ensure_component(&SDL3_DrawBuffer_id, ...)` in the CEL_Module(SDL3_Engine, init) body, alongside the existing ensure calls for SDL3_EventQueue and SDL3_Renderer.
**Warning signs:** Draw buffer component silently not attached, or crash in component lookup.

### Pitfall 4: Accessing Draw Buffer from Cross-TU Code
**What goes wrong:** The draw functions in sdl3_draw.c need to append to the draw buffer, but they cannot use cel_query/cel_each due to the per-TU static ID constraint.
**Why it happens:** sdl3_draw.c is a different TU from sdl3_module.c where the component IDs are initialized.
**How to avoid:** Two approaches: (1) Pass the draw buffer pointer explicitly to the draw functions (like the window table pattern), or (2) Store the draw buffer pointer in the Renderable vtable closure (the vtable functions capture the buffer pointer). Approach (1) is simpler and matches the existing codebase pattern.
**Warning signs:** Draw calls appear to succeed but nothing renders.

### Pitfall 5: Batch Variants with Per-Shape Color
**What goes wrong:** CONTEXT.md specifies "per-shape color in batch calls (parallel color array)." But SDL3's batch functions (SDL_RenderFillRects, SDL_RenderRects) use the single current draw color for ALL rects in the batch. There is no per-rect color parameter.
**Why it happens:** SDL3's batch API is designed for uniform-color batches, not per-element color batches.
**How to avoid:** The user-facing batch API (Renderable->filled_rects with parallel rects[] and colors[] arrays) must decompose into individual buffered commands, one per rect, each with its own color. During flush, consecutive same-color rects CAN be coalesced into SDL batch calls, but only when colors match. This is handled by the flush system, not the user.
**Warning signs:** All rects in a batch drawn with same color (the last color set).

### Pitfall 6: Draw System Registration Order
**What goes wrong:** The DrawFlushSystem must run AFTER all developer OnRender systems but BEFORE RenderPresentSystem. If registration order is wrong, either draw commands are flushed before all systems have emitted them, or flushed after present (invisible).
**Why it happens:** Within the same phase, systems run in registration order. DrawFlushSystem must be at PostRender phase (same as RenderPresentSystem) but registered BEFORE it.
**How to avoid:** Register DrawFlushSystem in CEL_Module init BEFORE RenderPresentSystem. Both run at PostRender phase, so DrawFlushSystem executes first.
**Warning signs:** Some draw calls missing, or all draw calls invisible (flushed after present).

### Pitfall 7: Alpha Blending Default
**What goes wrong:** Developer draws a semi-transparent shape (alpha < 255) but it renders as opaque.
**Why it happens:** CONTEXT.md specifies "Alpha blending OFF by default." SDL3's default blend mode for draw operations is SDL_BLENDMODE_NONE, which means alpha is ignored.
**How to avoid:** Document that alpha blending is off by default. Provide `sdl3_set_blend_mode(renderer, SDL_BLENDMODE_BLEND)` as a convenience wrapper. Developer must explicitly enable it.
**Warning signs:** Semi-transparent colors rendering as fully opaque.

## Code Examples

### SDL3 Draw Primitive Functions (Verified)
```c
// Source: SDL3 wiki - official function signatures
// All available since SDL 3.2.0, main thread only

// Single primitives
bool SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_FRect *rect);
bool SDL_RenderRect(SDL_Renderer *renderer, const SDL_FRect *rect);
bool SDL_RenderLine(SDL_Renderer *renderer, float x1, float y1, float x2, float y2);

// Batch primitives
bool SDL_RenderFillRects(SDL_Renderer *renderer, const SDL_FRect *rects, int count);
bool SDL_RenderRects(SDL_Renderer *renderer, const SDL_FRect *rects, int count);
bool SDL_RenderLines(SDL_Renderer *renderer, const SDL_FPoint *points, int count);

// Color and blend state
bool SDL_SetRenderDrawColor(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
bool SDL_SetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode);
```

### SDL3 Type Definitions (Verified)
```c
// Source: SDL3 wiki - struct definitions

typedef struct SDL_FRect {
    float x, y, w, h;
} SDL_FRect;

typedef struct SDL_FPoint {
    float x, y;
} SDL_FPoint;

typedef struct SDL_Color {
    Uint8 r, g, b, a;
} SDL_Color;

// Blend modes
// SDL_BLENDMODE_NONE  -- no blending (alpha ignored for draw operations)
// SDL_BLENDMODE_BLEND -- alpha blending: dstRGB = (srcRGB * srcA) + (dstRGB * (1-srcA))
```

### Renderable Vtable Definition
```c
// Public API (cels_sdl3.h)
typedef struct SDL3_Renderable {
    void (*filled_rect)(SDL_Renderer* r, SDL_FRect rect, SDL_Color color, int z);
    void (*filled_rects)(SDL_Renderer* r, const SDL_FRect* rects,
                         const SDL_Color* colors, int count, int z);
    void (*outlined_rect)(SDL_Renderer* r, SDL_FRect rect, SDL_Color color, int z);
    void (*outlined_rects)(SDL_Renderer* r, const SDL_FRect* rects,
                           const SDL_Color* colors, int count, int z);
    void (*line)(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_Color color, int z);
} SDL3_Renderable;

// Accessor
extern const SDL3_Renderable* sdl3_renderable(void);

// Explicit registration (developer calls this)
extern void SDL3_Renderable_use(void);
```

### Draw Buffer Implementation
```c
// Internal types (sdl3_internal.h)
typedef enum SDL3_DrawCmdType {
    SDL3_DRAW_FILLED_RECT,
    SDL3_DRAW_OUTLINED_RECT,
    SDL3_DRAW_LINE,
} SDL3_DrawCmdType;

typedef struct SDL3_DrawCmd {
    SDL3_DrawCmdType type;
    int z;
    int order;
    SDL_Color color;
    union {
        SDL_FRect rect;
        struct { SDL_FPoint a, b; } line;
    };
} SDL3_DrawCmd;

#define SDL3_DRAW_BUFFER_INITIAL_CAPACITY 256

// Buffer lifecycle
void sdl3_draw_buffer_init(SDL3_DrawCmd** cmds, int* count, int* capacity);
void sdl3_draw_buffer_clear(int* count, int* next_order);
void sdl3_draw_buffer_push(SDL3_DrawCmd** cmds, int* count, int* capacity,
                            int* next_order, SDL3_DrawCmd cmd);
void sdl3_draw_buffer_flush(SDL3_DrawCmd* cmds, int count, SDL_Renderer* renderer);
void sdl3_draw_buffer_destroy(SDL3_DrawCmd* cmds);
```

### Draw Buffer as Component
```c
// Public API (cels_sdl3.h)
CEL_Component(SDL3_DrawBuffer) {
    SDL3_DrawCmd* commands;
    int count;
    int capacity;
    int next_order;
};
```

### Vtable Implementation (SDL3 Provider)
```c
// In sdl3_draw.c -- the SDL3 provider implementation
// These functions DO NOT call SDL directly -- they buffer commands

static void sdl3_impl_filled_rect(SDL_Renderer* r, SDL_FRect rect,
                                    SDL_Color color, int z) {
    // Look up the draw buffer for this renderer's window
    SDL3_DrawBuffer* buf = sdl3_get_draw_buffer_for_renderer(r);
    if (!buf) return;
    sdl3_draw_buffer_push(&buf->commands, &buf->count, &buf->capacity,
                           &buf->next_order,
                           (SDL3_DrawCmd){
                               .type = SDL3_DRAW_FILLED_RECT,
                               .z = z,
                               .color = color,
                               .rect = rect
                           });
}
```

### Flush System
```c
// In sdl3_module.c -- registered at PostRender BEFORE RenderPresentSystem
CEL_System(SDL3_DrawFlushSystem, .phase = PostRender) {
    cel_query(SDL3_WindowComponent, SDL3_Renderer, SDL3_DrawBuffer);
    cel_each(SDL3_WindowComponent, SDL3_Renderer, SDL3_DrawBuffer) {
        if (SDL3_WindowComponent->state == SDL3_WINDOW_MINIMIZED ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSED) continue;

        if (SDL3_DrawBuffer->count == 0) continue;

        // Sort by (z ascending, order ascending)
        qsort(SDL3_DrawBuffer->commands, SDL3_DrawBuffer->count,
              sizeof(SDL3_DrawCmd), sdl3_draw_cmd_compare);

        // Flush to SDL3
        sdl3_draw_buffer_flush(SDL3_DrawBuffer->commands,
                                SDL3_DrawBuffer->count,
                                SDL3_Renderer->renderer);
    }
}
```

### Draw Buffer Clear (in PreRender or start of PostRender)
```c
// Option: clear in RenderClearSystem alongside SDL_RenderClear
CEL_System(SDL3_RenderClearSystem, .phase = PreRender) {
    cel_query(SDL3_WindowComponent, SDL3_Renderer, SDL3_DrawBuffer);
    cel_each(SDL3_WindowComponent, SDL3_Renderer, SDL3_DrawBuffer) {
        if (/* skip non-renderable states */) continue;

        // Clear draw buffer for this frame
        cel_update(SDL3_DrawBuffer) {
            SDL3_DrawBuffer->count = 0;
            SDL3_DrawBuffer->next_order = 0;
        }

        // Clear renderer
        SDL_SetRenderDrawColor(SDL3_Renderer->renderer, ...);
        SDL_RenderClear(SDL3_Renderer->renderer);
    }
}
```

### Complete Developer Usage Example
```c
// Developer system drawing shapes with z-ordering
CEL_System(UIRenderer, .phase = OnRender) {
    const SDL3_Renderable* draw = sdl3_renderable();

    cel_query(SDL3_Renderer);
    cel_each(SDL3_Renderer) {
        SDL_Renderer* r = SDL3_Renderer->renderer;

        // Background panel (z=0)
        draw->filled_rect(r,
            (SDL_FRect){50, 50, 300, 200},
            (SDL_Color){40, 40, 40, 255}, 0);

        // Border on top of background (z=1)
        draw->outlined_rect(r,
            (SDL_FRect){50, 50, 300, 200},
            (SDL_Color){200, 200, 200, 255}, 1);

        // Divider line (z=1)
        draw->line(r,
            (SDL_FPoint){50, 100}, (SDL_FPoint){350, 100},
            (SDL_Color){100, 100, 100, 255}, 1);
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| SDL_RenderDrawRect (SDL2) | SDL_RenderRect (SDL3) | SDL3 release | Function renamed, returns bool instead of int |
| SDL_RenderDrawLine (SDL2) | SDL_RenderLine (SDL3) | SDL3 release | Function renamed, uses float coords instead of int |
| SDL_RenderDrawRects (SDL2) | SDL_RenderRects (SDL3) | SDL3 release | Function renamed |
| SDL_RenderDrawLines (SDL2) | SDL_RenderLines (SDL3) | SDL3 release | Function renamed, SDL_FPoint instead of SDL_Point |
| SDL_Rect (int) for render functions (SDL2) | SDL_FRect (float) exclusively (SDL3) | SDL3 release | All render functions use float coordinates for subpixel precision |
| SDL_Point (int) for line functions (SDL2) | SDL_FPoint (float) + float params for SDL_RenderLine (SDL3) | SDL3 release | Subpixel precision for lines |
| Render batching off by default | Batching enabled by default for D3D/Metal/Vulkan | SDL 3.x (Nov 2025) | SDL_RenderFillRects and similar batch calls are efficiently batched GPU-side automatically |
| SDL_RENDERER_ACCELERATED flag | No renderer flags; always hardware accelerated when available | SDL3 release | Simplifies renderer creation |

**Deprecated/outdated:**
- `SDL_RenderDrawRect` / `SDL_RenderDrawRects`: Renamed to `SDL_RenderRect` / `SDL_RenderRects` in SDL3
- `SDL_RenderDrawLine` / `SDL_RenderDrawLines`: Renamed to `SDL_RenderLine` / `SDL_RenderLines` in SDL3
- `SDL_Rect` for render operations: SDL3 render functions exclusively use `SDL_FRect` (float) not `SDL_Rect` (int)
- Integer coordinates in draw calls: SDL3 uses float coordinates throughout for subpixel precision

## Open Questions

1. **How does the vtable function access the draw buffer from the renderer pointer?**
   - What we know: The vtable functions receive `SDL_Renderer*` as their first parameter. They need to find the draw buffer for that renderer's window. CELS does not provide a way to look up an entity by component value from cross-TU code.
   - What's unclear: Whether to use a static mapping (SDL_Renderer* -> SDL3_DrawBuffer*), or whether the vtable functions should take SDL3_DrawBuffer* directly instead of (or in addition to) SDL_Renderer*.
   - Recommendation: Use a simple static lookup table (up to SDL3_MAX_WINDOWS=8 entries) mapping SDL_Renderer* to SDL3_DrawBuffer*. Populated during the draw buffer clear pass (PreRender), consumed during OnRender. This avoids changing the vtable signature and keeps the developer-facing API clean. Alternatively, consider making the first parameter a custom struct wrapping both renderer and buffer pointer.

2. **Should SDL3_DrawBuffer be a component or a field in SDL3_Renderer?**
   - What we know: Making it a separate component follows ECS conventions (one component = one concern). Making it a field in SDL3_Renderer is simpler (no extra component registration, no extra ensure_component call).
   - What's unclear: Whether the planner prefers ECS purity or simplicity.
   - Recommendation: Embed the draw buffer fields directly in the SDL3_Renderer component. This avoids a separate component, avoids extra ensure_component, and the draw buffer is tightly coupled to the renderer anyway. The tradeoff is a slightly larger component struct, but the fields are just a pointer + two ints.

3. **Batch optimization during flush: how aggressive?**
   - What we know: After sorting, consecutive same-type same-color commands could be coalesced into SDL batch calls. SDL3 also does internal batching on the GPU side.
   - What's unclear: Whether the CPU-side coalescing overhead (scanning for runs, building temp arrays) outweighs the benefit of fewer SDL API calls.
   - Recommendation: Start simple -- flush each command individually with SDL_SetRenderDrawColor + SDL draw call. This is correct and the per-call overhead is minimal (SDL3 batches internally). Add batch coalescing only if profiling shows it matters. Mark this as Claude's discretion per CONTEXT.md.

## Sources

### Primary (HIGH confidence)
- [SDL3 wiki: SDL_RenderFillRect](https://wiki.libsdl.org/SDL3/SDL_RenderFillRect) - `bool SDL_RenderFillRect(SDL_Renderer*, const SDL_FRect*)`, verified
- [SDL3 wiki: SDL_RenderRect](https://wiki.libsdl.org/SDL3/SDL_RenderRect) - `bool SDL_RenderRect(SDL_Renderer*, const SDL_FRect*)`, verified
- [SDL3 wiki: SDL_RenderLine](https://wiki.libsdl.org/SDL3/SDL_RenderLine) - `bool SDL_RenderLine(SDL_Renderer*, float, float, float, float)`, verified
- [SDL3 wiki: SDL_RenderFillRects](https://wiki.libsdl.org/SDL3/SDL_RenderFillRects) - `bool SDL_RenderFillRects(SDL_Renderer*, const SDL_FRect*, int)`, verified
- [SDL3 wiki: SDL_RenderRects](https://wiki.libsdl.org/SDL3/SDL_RenderRects) - `bool SDL_RenderRects(SDL_Renderer*, const SDL_FRect*, int)`, verified
- [SDL3 wiki: SDL_RenderLines](https://wiki.libsdl.org/SDL3/SDL_RenderLines) - `bool SDL_RenderLines(SDL_Renderer*, const SDL_FPoint*, int)`, verified
- [SDL3 wiki: SDL_SetRenderDrawColor](https://wiki.libsdl.org/SDL3/SDL_SetRenderDrawColor) - verified
- [SDL3 wiki: SDL_SetRenderDrawBlendMode](https://wiki.libsdl.org/SDL3/SDL_SetRenderDrawBlendMode) - verified
- [SDL3 wiki: SDL_FRect](https://wiki.libsdl.org/SDL3/SDL_FRect) - `{ float x, y, w, h }`, verified
- [SDL3 wiki: SDL_FPoint](https://wiki.libsdl.org/SDL3/SDL_FPoint) - `{ float x, y }`, verified
- [SDL3 wiki: SDL_Color](https://wiki.libsdl.org/SDL3/SDL_Color) - `{ Uint8 r, g, b, a }`, verified
- [SDL3 wiki: SDL_BlendMode](https://wiki.libsdl.org/SDL3/SDL_BlendMode) - SDL_BLENDMODE_NONE, SDL_BLENDMODE_BLEND, verified
- [SDL3 examples: primitives.c](https://github.com/libsdl-org/SDL/blob/main/examples/renderer/02-primitives/primitives.c) - official example using all draw primitives
- CELS cels.h (local, line 1008) - `cel_module_provides(Feature)` is string-based capability declaration only; no vtable dispatch
- CELS cels_api.c (local, line 1220) - `cels_module_provides` stores string in module registry; confirmed no vtable mechanism
- Existing cels-sdl3 codebase (local) - per-TU static ID constraint, window table pattern, component patterns
- cels-ncurses cels_ncurses.h + cels_ncurses_draw.h (local) - TUI_Renderable tag + TUI_DrawContext pattern as reference for draw API design

### Secondary (MEDIUM confidence)
- [Phoronix: SDL3 Batch Rendering](https://www.phoronix.com/news/SDL3-Batch-Rendering) - SDL3 render batching enabled for D3D/Metal/Vulkan (Nov 2025)
- [SDL GitHub issue #8584](https://github.com/libsdl-org/SDL/issues/8584) - SDL3 renderers default to batching enabled

### Tertiary (LOW confidence)
- (none -- all findings verified against primary sources)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - SDL3 draw primitive API fully verified via official wiki documentation
- Architecture: HIGH - Vtable pattern is straightforward C; draw buffer pattern is well-understood; per-TU constraint is established in codebase
- Pitfalls: HIGH - SDL state leakage, batch color limitation, and per-TU ID issues are all verified from official docs and existing codebase patterns

**Research date:** 2026-03-21
**Valid until:** 2026-04-21 (stable APIs, 30-day validity)
