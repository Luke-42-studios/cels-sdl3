# Phase 7: Textures - Research

**Researched:** 2026-03-21
**Domain:** SDL3_image texture loading, per-renderer texture caching, ECS sprite component, renderer affinity
**Confidence:** HIGH

## Summary

Phase 7 introduces image loading via SDL3_image and texture rendering via SDL3's `SDL_RenderTexture` / `SDL_RenderTextureRotated`. The core API is `IMG_LoadTexture(renderer, file)` which loads an image from disk directly into GPU memory as an `SDL_Texture*`. Textures are renderer-affine: each `SDL_Texture` is bound to the `SDL_Renderer` that created it and cannot be used with a different renderer. SDL3 provides `SDL_GetRendererFromTexture(texture)` to query which renderer owns a texture (thread-safe).

The ECS architecture places a unified `SDL3_Sprite` component on each drawing entity. This component holds the texture reference (opaque handle into a per-renderer cache), source/destination rects, rotation angle, flip flags, and alpha. A per-renderer texture cache (keyed by file path string) ensures the same image loaded multiple times results in a single `SDL_Texture*` with a reference count. A declarative loading system detects sprites with a path set but no texture loaded, triggers loading, and transitions the sprite through a state machine: NONE -> LOADING -> READY (or FAILED). Rendering only occurs for READY sprites.

For async loading, SDL3 provides `SDL_LoadFileAsync` which uses io_uring (Linux) / IoRing (Windows 11) under the hood. However, the GPU upload step (`IMG_LoadTexture_IO` or `SDL_CreateTextureFromSurface`) must happen on the main thread. The recommended approach for Phase 7 is synchronous loading on the main thread (file I/O + decode + upload all in one call via `IMG_LoadTexture`), with the state machine providing the infrastructure for future async support. This avoids premature complexity while establishing the correct state machine pattern.

**Primary recommendation:** Implement synchronous loading via `IMG_LoadTexture(renderer, resolved_path)` in a loading system at OnLoad phase. Store texture references as opaque cache handles (uint32 index) in the SDL3_Sprite component. Build a per-renderer texture cache with string-keyed lookup and reference counting. The render system queries entities with SDL3_Sprite + SDL3_Renderer and calls `SDL_RenderTextureRotated` for sprites in READY state.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| SDL3_image | 3.4.0 | `IMG_LoadTexture` - load PNG/JPG/etc directly to GPU texture | Already a project dependency; single-call file-to-texture |
| SDL3 | 3.4.2 | `SDL_RenderTexture`, `SDL_RenderTextureRotated`, `SDL_DestroyTexture`, `SDL_GetTextureSize`, `SDL_SetTextureAlphaMod`, `SDL_SetTextureBlendMode`, `SDL_GetRendererFromTexture` | Core rendering and texture lifecycle APIs |

No additional libraries needed. SDL3_image and SDL3 cover all texture loading and rendering requirements.

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| SDL3 AsyncIO | 3.4.2 | `SDL_LoadFileAsync`, `SDL_GetAsyncIOResult` | Future async loading (not Phase 7 -- deferred to keep scope manageable) |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `IMG_LoadTexture(renderer, file)` | `IMG_Load(file)` + `SDL_CreateTextureFromSurface(renderer, surface)` | Two-step gives CPU-side pixel access for manipulation; one-step is simpler for our use case. Use two-step only if image processing is needed before upload. |
| Synchronous loading (Phase 7) | `SDL_LoadFileAsync` + `IMG_LoadTexture_IO` | Async avoids frame hitches for large images but adds complexity. State machine is designed to support async later without API changes. |
| Opaque cache handle (uint32 index) | Raw `SDL_Texture*` in component | Handle survives cache invalidation (renderer destruction); raw pointer would dangle. Handle also portable across potential web/Android targets. |
| Per-renderer cache | Global cache | Per-renderer is required: same image on two windows = two SDL_Textures. `SDL_DestroyRenderer` frees ALL its textures, so cache must be per-renderer. |

## Architecture Patterns

### Recommended Project Structure
```
src/
  texture/
    sdl3_texture.c         # NEW: texture cache, loading, destruction
  renderer/
    sdl3_renderer.c        # EXISTING: unchanged
  sdl3_module.c            # MODIFIED: add SDL3_Sprite component, TextureLoadSystem, SpriteRenderSystem
  sdl3_internal.h          # MODIFIED: add texture cache types and function declarations
include/
  cels_sdl3.h             # MODIFIED: add SDL3_Sprite component, SDL3_TextureState enum, texture API
examples/
  textures/
    main.c                 # NEW: example loading and displaying images
    assets/
      test.png             # NEW: test image asset
```

### Pattern 1: SDL3_Sprite as Unified Draw Component
**What:** A single ECS component holds everything needed to render a textured quad: texture reference, source rect, destination rect, rotation, flip, and alpha. The texture state field tracks loading progress.
**When to use:** Always -- every entity that renders a texture has an SDL3_Sprite.
**Example:**
```c
// Source: SDL3 SDL_FlipMode (SDL_surface.h lines 100-106), SDL_FRect, verified

typedef enum SDL3_TextureState {
    SDL3_TEXTURE_NONE = 0,      // No texture path set
    SDL3_TEXTURE_LOADING,       // Path set, loading in progress
    SDL3_TEXTURE_READY,         // Texture loaded, renderable
    SDL3_TEXTURE_FAILED,        // Load failed (path invalid, format unsupported)
    SDL3_TEXTURE_UNLOADED       // Renderer destroyed, texture invalidated
} SDL3_TextureState;

CEL_Component(SDL3_Sprite) {
    // Texture reference
    const char*        texture_path;   // File path (relative to asset base dir)
    uint32_t           texture_handle; // Opaque handle into per-renderer cache
    SDL3_TextureState  state;          // Loading state machine

    // Rendering parameters
    SDL_FRect          src_rect;       // Source rect (0,0,0,0 = entire texture)
    SDL_FRect          dst_rect;       // Destination rect (position + size)
    double             angle;          // Rotation in degrees (clockwise)
    SDL_FPoint         center;         // Rotation center (0,0 = use dst_rect center)
    SDL_FlipMode       flip;           // SDL_FLIP_NONE / HORIZONTAL / VERTICAL
    Uint8              alpha;          // Per-sprite alpha (255 = opaque)
};
```

### Pattern 2: Per-Renderer Texture Cache
**What:** Each renderer maintains a hash table mapping file paths to cached texture entries. Each entry holds the `SDL_Texture*`, a reference count, and the texture dimensions. Multiple sprites referencing the same path share one GPU texture. When the last reference is released, the texture is freed.
**When to use:** Always -- textures are expensive GPU resources that must be shared and reference-counted.
**Example:**
```c
// Source: designed from CONTEXT.md decisions + SDL3 texture lifecycle

#define SDL3_TEXTURE_CACHE_CAPACITY 128

typedef struct SDL3_TextureCacheEntry {
    char               path[256];      // Resolved file path (key)
    SDL_Texture*       texture;        // GPU texture (NULL if slot empty)
    float              width;          // Texture width in pixels
    float              height;         // Texture height in pixels
    int                ref_count;      // Number of sprites using this texture
} SDL3_TextureCacheEntry;

typedef struct SDL3_TextureCache {
    SDL3_TextureCacheEntry entries[SDL3_TEXTURE_CACHE_CAPACITY];
    int                    count;      // Number of occupied slots
    char                   base_dir[256]; // Asset base directory prefix
} SDL3_TextureCache;

// Cache operations (in sdl3_texture.c)
uint32_t sdl3_texture_cache_load(SDL3_TextureCache* cache,
                                  SDL_Renderer* renderer,
                                  const char* path);
void     sdl3_texture_cache_release(SDL3_TextureCache* cache, uint32_t handle);
SDL_Texture* sdl3_texture_cache_get(const SDL3_TextureCache* cache, uint32_t handle);
void     sdl3_texture_cache_destroy(SDL3_TextureCache* cache);
```

### Pattern 3: Declarative Loading System
**What:** A system at OnLoad phase scans all SDL3_Sprite components. Sprites with `state == SDL3_TEXTURE_NONE` and a non-NULL `texture_path` are auto-loaded. The system resolves the renderer for the sprite's window, loads the texture into that renderer's cache, and transitions the sprite to READY (or FAILED).
**When to use:** Every frame -- new sprites auto-detected and loaded.
**Example:**
```c
// In sdl3_module.c
CEL_System(SDL3_TextureLoadSystem, .phase = OnLoad) {
    cel_query(SDL3_Sprite, SDL3_Renderer, SDL3_WindowComponent);
    cel_each(SDL3_Sprite, SDL3_Renderer, SDL3_WindowComponent) {
        if (SDL3_Sprite->state != SDL3_TEXTURE_NONE) continue;
        if (!SDL3_Sprite->texture_path) continue;
        if (!SDL3_Renderer->renderer) continue;

        // Look up or create cache for this renderer
        SDL3_TextureCache* cache = sdl3_texture_cache_for_renderer(
            SDL3_Renderer->renderer);

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
```

### Pattern 4: Sprite Render System
**What:** A system at OnRender phase queries entities with SDL3_Sprite + SDL3_Renderer. For each READY sprite, it resolves the SDL_Texture* from the cache handle, applies alpha modulation, and calls `SDL_RenderTextureRotated`.
**When to use:** Every frame, for all sprites.
**Example:**
```c
// In sdl3_module.c
CEL_System(SDL3_SpriteRenderSystem, .phase = OnRender) {
    cel_query(SDL3_Sprite, SDL3_Renderer, SDL3_WindowComponent);
    cel_each(SDL3_Sprite, SDL3_Renderer, SDL3_WindowComponent) {
        if (SDL3_WindowComponent->state == SDL3_WINDOW_MINIMIZED ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSED) continue;
        if (SDL3_Sprite->state != SDL3_TEXTURE_READY) continue;

        SDL3_TextureCache* cache = sdl3_texture_cache_for_renderer(
            SDL3_Renderer->renderer);
        SDL_Texture* tex = sdl3_texture_cache_get(
            cache, SDL3_Sprite->texture_handle);
        if (!tex) continue;

        // Apply per-sprite alpha
        if (SDL3_Sprite->alpha < 255) {
            SDL_SetTextureAlphaMod(tex, SDL3_Sprite->alpha);
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        }

        // Determine source rect (NULL = entire texture)
        const SDL_FRect* src = NULL;
        if (SDL3_Sprite->src_rect.w > 0 && SDL3_Sprite->src_rect.h > 0) {
            src = &SDL3_Sprite->src_rect;
        }

        // Determine rotation center (NULL = center of dst_rect)
        const SDL_FPoint* center = NULL;
        if (SDL3_Sprite->center.x != 0 || SDL3_Sprite->center.y != 0) {
            center = &SDL3_Sprite->center;
        }

        SDL_RenderTextureRotated(
            SDL3_Renderer->renderer, tex,
            src, &SDL3_Sprite->dst_rect,
            SDL3_Sprite->angle, center,
            SDL3_Sprite->flip);

        // Reset alpha mod if changed (avoid state leakage to next draw)
        if (SDL3_Sprite->alpha < 255) {
            SDL_SetTextureAlphaMod(tex, 255);
        }
    }
}
```

### Pattern 5: Per-Renderer Cache Lookup Table (Cross-TU Access)
**What:** Since texture cache operations happen in sdl3_texture.c (cross-TU), and the cache needs to be associated with a specific renderer, use a static lookup table mapping `SDL_Renderer*` to `SDL3_TextureCache*`. This follows the same pattern as `SDL3_DrawBufferTable` from Phase 6.
**When to use:** Whenever cross-TU code needs to find the cache for a renderer.
**Example:**
```c
// In sdl3_texture.c
#define SDL3_MAX_RENDERERS 8

static struct {
    SDL_Renderer*      renderer;
    SDL3_TextureCache  cache;
} s_cache_table[SDL3_MAX_RENDERERS];
static int s_cache_count = 0;

SDL3_TextureCache* sdl3_texture_cache_for_renderer(SDL_Renderer* renderer) {
    for (int i = 0; i < s_cache_count; i++) {
        if (s_cache_table[i].renderer == renderer) {
            return &s_cache_table[i].cache;
        }
    }
    // Create new cache for this renderer
    if (s_cache_count < SDL3_MAX_RENDERERS) {
        int idx = s_cache_count++;
        s_cache_table[idx].renderer = renderer;
        memset(&s_cache_table[idx].cache, 0, sizeof(SDL3_TextureCache));
        return &s_cache_table[idx].cache;
    }
    return NULL;
}
```

### Pattern 6: Renderer Destruction Cascade
**What:** When a renderer is destroyed (window CLOSING -> CLOSED), all textures in its cache become invalid (SDL_DestroyRenderer frees all associated textures). The cache must be cleared and all sprites referencing that renderer must transition to UNLOADED state.
**When to use:** During the WindowStateSystem CLOSING -> CLOSED transition.
**Example:**
```c
// In sdl3_module.c WindowStateSystem, before renderer destruction:
// 1. Invalidate all sprites using this renderer's textures
// 2. Destroy the texture cache for this renderer
// 3. Then destroy the renderer (SDL_DestroyRenderer frees GPU textures)
// 4. Then destroy the window

// The cache_destroy function zeroes all entries and removes the
// renderer from the lookup table. No SDL_DestroyTexture calls needed
// because SDL_DestroyRenderer handles that.
sdl3_texture_cache_remove_renderer(SDL3_Renderer->renderer);
```

### Anti-Patterns to Avoid
- **Storing raw SDL_Texture* in the sprite component:** The pointer becomes dangling when the renderer is destroyed. Use an opaque cache handle instead.
- **Global texture cache shared across renderers:** Textures are renderer-specific. Using a texture with the wrong renderer is undefined behavior. Each renderer MUST have its own cache.
- **Loading textures in the render phase:** Texture loading involves disk I/O and GPU upload. Do this at OnLoad phase, not OnRender. The render system should only draw READY textures.
- **Forgetting to reset SDL_SetTextureAlphaMod after drawing:** Textures are shared across sprites. If sprite A sets alpha=128 on a cached texture and sprite B uses the same texture expecting alpha=255, sprite B will render translucent. Always reset alpha mod after drawing.
- **Calling SDL_DestroyTexture on cached textures when sprite is removed:** The cache is reference-counted. Decrement the reference count; only destroy when ref_count reaches 0. Direct SDL_DestroyTexture calls bypass the cache.
- **Making texture state an actual CEL_State global singleton:** CEL_State is for global singletons, not per-entity state. The texture state (NONE/LOADING/READY/FAILED/UNLOADED) is a field within the SDL3_Sprite component. Watching it works via `cel_watch(entity, SDL3_Sprite)` which triggers on any component field change.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Image decoding (PNG, JPG) | Custom PNG/JPG decoder or stb_image | `IMG_LoadTexture(renderer, file)` | SDL3_image handles 18+ formats, does direct-to-GPU upload, already a dependency |
| Texture-to-renderer binding check | Manual tracking of which renderer created a texture | `SDL_GetRendererFromTexture(texture)` | SDL3 built-in function, thread-safe, returns the owning renderer |
| Getting texture dimensions | Tracking width/height separately during loading | `SDL_GetTextureSize(texture, &w, &h)` | SDL3 built-in, returns float dimensions |
| File path resolution | Custom path joining | `SDL_snprintf` for simple path concatenation | Path joining is just string concatenation with `/` separator |
| Async file I/O | Custom thread pool for file loading | `SDL_LoadFileAsync` + `SDL_GetAsyncIOResult` (future) | SDL3 uses io_uring/IoRing; managed thread pool fallback on older systems |
| Hash table for cache | Custom hash table implementation | Linear scan of fixed array (128 entries max) | For <128 textures, linear scan is fast enough and dramatically simpler than a hash table. Profile before optimizing. |

**Key insight:** `IMG_LoadTexture` is the single most important function in this phase. It handles file I/O, format detection, image decoding, pixel format conversion, AND GPU upload in one call. The only thing we build on top is caching (to avoid redundant loads) and the ECS integration (sprite component + loading system).

## Common Pitfalls

### Pitfall 1: Texture Renderer Affinity Violation
**What goes wrong:** Using an `SDL_Texture*` with a different `SDL_Renderer*` than the one that created it causes undefined behavior (garbled rendering, crash, or GPU driver error).
**Why it happens:** In multi-window scenarios, each window has its own renderer. A sprite entity associated with window A might accidentally use window B's renderer.
**How to avoid:** The loading system resolves the renderer from the sprite's associated window and creates the texture on that specific renderer. The render system queries both SDL3_Sprite and SDL3_Renderer on the same entity, ensuring the correct renderer is always used. For validation, use `SDL_GetRendererFromTexture(texture) == renderer` check and log a warning if mismatched (silent skip, no crash).
**Warning signs:** Garbled or missing textures, GPU errors in debug output.

### Pitfall 2: SDL_DestroyRenderer Frees All Textures
**What goes wrong:** After `SDL_DestroyRenderer` is called, ALL textures created on that renderer are freed automatically. Any cached `SDL_Texture*` pointers become dangling.
**Why it happens:** SDL3 docs explicitly state: "Destroy the rendering context for a window and free all associated textures."
**How to avoid:** When a renderer is about to be destroyed: (1) transition all sprites using its textures to UNLOADED state, (2) clear the texture cache for that renderer (zero out entries, do NOT call SDL_DestroyTexture -- SDL does it), (3) remove the renderer from the cache lookup table. The opaque handle design (uint32 index vs raw pointer) makes dangling pointers less dangerous -- a stale handle maps to a zeroed cache entry which returns NULL.
**Warning signs:** Crash or segfault when closing a window while textures are still referenced.

### Pitfall 3: Texture Alpha Mod State Leakage
**What goes wrong:** `SDL_SetTextureAlphaMod` modifies the texture object itself, not the renderer state. Since textures are shared via the cache, setting alpha on a texture for one sprite affects ALL sprites using that texture.
**Why it happens:** Unlike `SDL_SetRenderDrawColor` which is renderer-global state, `SDL_SetTextureAlphaMod` is per-texture state. Shared textures share this state.
**How to avoid:** Always set the texture's alpha mod immediately before drawing each sprite, and reset it to 255 immediately after. This ensures each sprite gets its own alpha regardless of draw order.
**Warning signs:** Sprites appearing with wrong transparency, last-drawn sprite's alpha affecting other sprites using the same texture.

### Pitfall 4: Per-TU Static ID Constraint for SDL3_Sprite
**What goes wrong:** The SDL3_Sprite component ID is 0 (uninitialized) when cross-TU code tries to use it.
**Why it happens:** Same issue as SDL3_EventQueue, SDL3_Renderer from earlier phases -- `CEL_Component _register()` is a no-op; actual ID assignment requires `cels_ensure_component`.
**How to avoid:** Call `cels_ensure_component(&SDL3_Sprite_id, "SDL3_Sprite", sizeof(SDL3_Sprite), CELS_ALIGNOF(SDL3_Sprite))` in the `CEL_Module(SDL3_Engine, init)` body.
**Warning signs:** Sprite component silently not attached, or crash in component lookup.

### Pitfall 5: Source Rect Zero Meaning
**What goes wrong:** SDL3 interprets `SDL_FRect{0, 0, 0, 0}` differently than "no source rect." Passing a pointer to a zero-rect clips to nothing (renders nothing). Passing NULL means "entire texture."
**Why it happens:** `SDL_RenderTexture(renderer, texture, srcrect, dstrect)` treats srcrect=NULL as "use entire texture," but a pointer to {0,0,0,0} is a valid (empty) source rect.
**How to avoid:** Convention: `src_rect.w == 0 && src_rect.h == 0` means "entire texture" (pass NULL to SDL). Any non-zero width/height means an explicit sub-rect. Document this convention clearly.
**Warning signs:** Sprites appearing blank despite texture being READY.

### Pitfall 6: Texture Loading During Render Phase
**What goes wrong:** Loading textures in the OnRender phase causes frame hitches because disk I/O and GPU upload happen mid-frame, delaying the present call.
**Why it happens:** New sprites might be created during update, and a developer might try to load immediately when needed.
**How to avoid:** The TextureLoadSystem runs at OnLoad phase (before rendering). Sprites created during a frame will have state=NONE for that frame and load next frame. One frame of invisible sprite is acceptable.
**Warning signs:** Frame time spikes when new sprites appear.

### Pitfall 7: Cache Entry Path Buffer Overflow
**What goes wrong:** File paths longer than the cache entry's path buffer cause truncation or buffer overflow.
**Why it happens:** Fixed-size path buffers in the cache entry struct.
**How to avoid:** Use `SDL_strlcpy` (or `snprintf`) for path copying to ensure null termination within bounds. Log a warning if path is truncated. 256 bytes is sufficient for typical game asset paths but should be documented.
**Warning signs:** Textures not found despite correct file path, or cache misses for paths that should hit.

### Pitfall 8: Blend Mode Not Set for Alpha Textures
**What goes wrong:** Textures with alpha channels render with black backgrounds instead of transparency.
**Why it happens:** `SDL_SetTextureBlendMode` defaults to `SDL_BLENDMODE_NONE` for some texture types. Without `SDL_BLENDMODE_BLEND`, the alpha channel is ignored during rendering.
**How to avoid:** When loading a texture, set `SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND)` by default. This ensures alpha-transparent images render correctly. Images without alpha channels are unaffected (they have alpha=255 everywhere).
**Warning signs:** Transparent areas in PNG images rendering as black or the clear color.

## Code Examples

### SDL3_image Loading Functions (Verified from header)
```c
// Source: SDL3_image/SDL_image.h (local, release-3.4.0)

// Direct file-to-GPU-texture (preferred for our use case)
SDL_Texture* IMG_LoadTexture(SDL_Renderer *renderer, const char *file);

// From SDL_IOStream (useful for async: load bytes first, then create texture)
SDL_Texture* IMG_LoadTexture_IO(SDL_Renderer *renderer, SDL_IOStream *src, bool closeio);

// File-to-CPU-surface (if pixel manipulation needed before GPU upload)
SDL_Surface* IMG_Load(const char *file);
```

### SDL3 Texture Rendering Functions (Verified from header)
```c
// Source: SDL3/SDL_render.h (local, release-3.4.2)

// Basic texture render (no rotation/flip)
bool SDL_RenderTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                       const SDL_FRect *srcrect, const SDL_FRect *dstrect);

// Texture render with rotation and flip
bool SDL_RenderTextureRotated(SDL_Renderer *renderer, SDL_Texture *texture,
                               const SDL_FRect *srcrect, const SDL_FRect *dstrect,
                               double angle, const SDL_FPoint *center,
                               SDL_FlipMode flip);

// Get texture dimensions
bool SDL_GetTextureSize(SDL_Texture *texture, float *w, float *h);

// Per-texture alpha modulation
bool SDL_SetTextureAlphaMod(SDL_Texture *texture, Uint8 alpha);

// Per-texture blend mode
bool SDL_SetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode blendMode);

// Query which renderer owns a texture (thread-safe)
SDL_Renderer* SDL_GetRendererFromTexture(SDL_Texture *texture);

// Destroy texture (free GPU memory)
void SDL_DestroyTexture(SDL_Texture *texture);
```

### SDL_FlipMode Enum (Verified from header)
```c
// Source: SDL3/SDL_surface.h lines 100-106

typedef enum SDL_FlipMode {
    SDL_FLIP_NONE,
    SDL_FLIP_HORIZONTAL,
    SDL_FLIP_VERTICAL,
    SDL_FLIP_HORIZONTAL_AND_VERTICAL = (SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL)
} SDL_FlipMode;
```

### SDL3 Async IO Functions (For future reference)
```c
// Source: SDL3/SDL_asyncio.h (local, release-3.4.2)
// NOT used in Phase 7 (synchronous loading), but documents the path for future async

// Load entire file asynchronously (uses io_uring/IoRing when available)
bool SDL_LoadFileAsync(const char *file, SDL_AsyncIOQueue *queue, void *userdata);

// Non-blocking check for completed async tasks
bool SDL_GetAsyncIOResult(SDL_AsyncIOQueue *queue, SDL_AsyncIOOutcome *outcome);

// Create a queue for async IO task tracking
SDL_AsyncIOQueue* SDL_CreateAsyncIOQueue(void);

// Wrap memory buffer as IOStream (for passing async-loaded bytes to IMG_LoadTexture_IO)
SDL_IOStream* SDL_IOFromConstMem(const void *mem, size_t size);
```

### Complete Texture Cache Implementation Pattern
```c
// In sdl3_texture.c

#define SDL3_TEXTURE_CACHE_CAPACITY 128
#define SDL3_TEXTURE_PATH_MAX 256

typedef struct SDL3_TextureCacheEntry {
    char          path[SDL3_TEXTURE_PATH_MAX];
    SDL_Texture*  texture;
    float         width;
    float         height;
    int           ref_count;
} SDL3_TextureCacheEntry;

typedef struct SDL3_TextureCache {
    SDL3_TextureCacheEntry entries[SDL3_TEXTURE_CACHE_CAPACITY];
    int                    count;
    char                   base_dir[SDL3_TEXTURE_PATH_MAX];
} SDL3_TextureCache;

// Returns handle (1-based index), or 0 on failure
uint32_t sdl3_texture_cache_load(SDL3_TextureCache* cache,
                                  SDL_Renderer* renderer,
                                  const char* path)
{
    // Resolve full path
    char resolved[SDL3_TEXTURE_PATH_MAX * 2];
    if (cache->base_dir[0]) {
        SDL_snprintf(resolved, sizeof(resolved), "%s/%s", cache->base_dir, path);
    } else {
        SDL_strlcpy(resolved, path, sizeof(resolved));
    }

    // Check if already cached
    for (int i = 0; i < cache->count; i++) {
        if (cache->entries[i].texture &&
            SDL_strcmp(cache->entries[i].path, resolved) == 0) {
            cache->entries[i].ref_count++;
            return (uint32_t)(i + 1);  // 1-based handle
        }
    }

    // Load new texture
    SDL_Texture* tex = IMG_LoadTexture(renderer, resolved);
    if (!tex) return 0;

    // Set blend mode for alpha support by default
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    // Find empty slot or append
    int slot = -1;
    for (int i = 0; i < cache->count; i++) {
        if (!cache->entries[i].texture) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (cache->count >= SDL3_TEXTURE_CACHE_CAPACITY) {
            SDL_DestroyTexture(tex);
            SDL_Log("SDL3: Texture cache full (%d entries)", SDL3_TEXTURE_CACHE_CAPACITY);
            return 0;
        }
        slot = cache->count++;
    }

    SDL3_TextureCacheEntry* entry = &cache->entries[slot];
    SDL_strlcpy(entry->path, resolved, SDL3_TEXTURE_PATH_MAX);
    entry->texture = tex;
    SDL_GetTextureSize(tex, &entry->width, &entry->height);
    entry->ref_count = 1;

    return (uint32_t)(slot + 1);  // 1-based handle
}

SDL_Texture* sdl3_texture_cache_get(const SDL3_TextureCache* cache, uint32_t handle) {
    if (handle == 0 || handle > (uint32_t)cache->count) return NULL;
    return cache->entries[handle - 1].texture;
}

void sdl3_texture_cache_release(SDL3_TextureCache* cache, uint32_t handle) {
    if (handle == 0 || handle > (uint32_t)cache->count) return;
    SDL3_TextureCacheEntry* entry = &cache->entries[handle - 1];
    if (entry->ref_count > 0) {
        entry->ref_count--;
        if (entry->ref_count == 0 && entry->texture) {
            SDL_DestroyTexture(entry->texture);
            entry->texture = NULL;
            entry->path[0] = '\0';
        }
    }
}

// Called when renderer is about to be destroyed
// Does NOT call SDL_DestroyTexture (SDL_DestroyRenderer handles that)
void sdl3_texture_cache_invalidate(SDL3_TextureCache* cache) {
    for (int i = 0; i < cache->count; i++) {
        cache->entries[i].texture = NULL;
        cache->entries[i].ref_count = 0;
        cache->entries[i].path[0] = '\0';
    }
    cache->count = 0;
}
```

### Developer Usage Pattern
```c
// Developer creates a sprite entity (in a composition or system)
CEL_Composition(MySprite) {
    cel_has(SDL3_Sprite,
        .texture_path = "sprites/hero.png",
        .dst_rect = { .x = 100, .y = 100, .w = 64, .h = 64 },
        .flip = SDL_FLIP_NONE,
        .alpha = 255
    );
    // Must also have SDL3_Renderer + SDL3_WindowComponent on same entity
    // OR be composed on a window entity
}

// Spritesheet sub-rect usage
CEL_Composition(AnimFrame) {
    cel_has(SDL3_Sprite,
        .texture_path = "sprites/sheet.png",
        .src_rect = { .x = 0, .y = 0, .w = 32, .h = 32 },  // First frame
        .dst_rect = { .x = 200, .y = 100, .w = 64, .h = 64 },
        .alpha = 255
    );
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `IMG_Init(IMG_INIT_PNG \| IMG_INIT_JPG)` required (SDL2_image) | No initialization required (SDL3_image) | SDL3_image 3.0.0 | SDL3_image auto-initializes codecs on first use; `IMG_Init` is removed |
| `SDL_RenderCopy` / `SDL_RenderCopyEx` (SDL2) | `SDL_RenderTexture` / `SDL_RenderTextureRotated` (SDL3) | SDL3 release | Functions renamed, use `SDL_FRect` (float) instead of `SDL_Rect` (int), return `bool` |
| `SDL_QueryTexture(tex, NULL, NULL, &w, &h)` (SDL2) | `SDL_GetTextureSize(tex, &w, &h)` (SDL3) | SDL3 release | Dedicated function, returns float dimensions |
| `SDL_SetTextureAlphaMod` returns `int` (SDL2) | Returns `bool` (SDL3) | SDL3 release | Standard SDL3 bool return pattern |
| `SDL_RendererFlip` enum (SDL2) | `SDL_FlipMode` enum (SDL3) | SDL3 release | Renamed; `SDL_FLIP_HORIZONTAL_AND_VERTICAL` added as convenience combo |
| No async file I/O in SDL2 | `SDL_LoadFileAsync` with io_uring/IoRing (SDL3) | SDL 3.2.0 | Native async I/O for future use |
| `SDL_RenderCopyExF` (SDL2 float variant) | `SDL_RenderTextureRotated` is always float (SDL3) | SDL3 release | All render functions use float coordinates natively |

**Deprecated/outdated:**
- `IMG_Init()` / `IMG_Quit()`: Removed in SDL3_image. No initialization/cleanup needed.
- `SDL_RenderCopy` / `SDL_RenderCopyEx`: Renamed to `SDL_RenderTexture` / `SDL_RenderTextureRotated` in SDL3.
- `SDL_QueryTexture`: Replaced by `SDL_GetTextureSize` (float dimensions) and `SDL_GetTextureProperties` (format, access).
- `SDL_RendererFlip`: Renamed to `SDL_FlipMode` in SDL3.
- Integer coordinates for texture rendering: SDL3 uses `SDL_FRect` (float) exclusively.

## Open Questions

1. **How should sprites find their associated window's renderer?**
   - What we know: The CONTEXT says "Sprite automatically resolves its renderer from the window entity it's associated with." In the current ECS, the renderer and window components are on the same entity. Sprites on the same entity automatically have access. But if sprites are on child entities or separate entities, they need a way to find their parent window's renderer.
   - What's unclear: Whether sprites will always live on window entities (simple but limiting), or on separate entities with a parent/reference to a window entity (flexible but needs a resolution mechanism).
   - Recommendation: Start with sprites on window entities (querying SDL3_Sprite + SDL3_Renderer + SDL3_WindowComponent in the same cel_each). This covers the common case. If separate sprite entities are needed later, add a `window_entity` field to SDL3_Sprite for explicit resolution. Mark as Claude's discretion per CONTEXT.md.

2. **Asset base directory configuration mechanism**
   - What we know: CONTEXT says "Configurable base asset directory -- set once, then use relative paths everywhere." This could be a global setting on the texture cache, a field on the SDL3_ContextConfig, or a per-cache configuration.
   - What's unclear: Whether this is a global (all caches share the same base dir) or per-renderer setting.
   - Recommendation: Global setting via a function call: `sdl3_set_asset_base_dir(const char* path)`. Stored as a static variable in sdl3_texture.c. All caches read this when resolving paths. Simple, sufficient, and easy to set in main() before the frame loop. Mark as Claude's discretion.

3. **Sprite entity lifecycle -- who manages reference count decrements?**
   - What we know: When a sprite entity is destroyed, its reference in the texture cache must be decremented. The cache entry's texture should be freed when ref_count hits 0. CELS lifecycle observers (`on_destroy`) could handle this.
   - What's unclear: Whether there's a clean way to hook into entity destruction for cleanup, or if a system should scan for removed sprites.
   - Recommendation: Use a CELS lifecycle observer on the sprite component or a cleanup system that runs at PostRender. If CELS supports `on_destroy` for individual components (not just lifecycles), use that. Otherwise, the texture cache can use weak references and periodically scan for orphaned entries. This needs investigation during implementation.

4. **Z-ordering of sprites relative to draw primitives**
   - What we know: Phase 6 introduced z-indexed draw commands buffered and sorted. Sprites need to interleave with primitives in the z-order. The sprite render system currently calls `SDL_RenderTextureRotated` directly during OnRender, but draw primitives are buffered and flushed at PostRender.
   - What's unclear: Whether sprites should also be buffered into the draw command system, or rendered separately in order.
   - Recommendation: For Phase 7, render sprites directly during OnRender (no buffering). Sprites always render on top of draw primitives flushed at PostRender. If interleaved z-ordering is needed, this can be added later by extending the draw command buffer with a texture command type. This is a simplification to keep Phase 7 focused on texture loading mechanics.

## Sources

### Primary (HIGH confidence)
- SDL3_image/SDL_image.h (local, release-3.4.0) -- `IMG_LoadTexture`, `IMG_LoadTexture_IO`, `IMG_Load` function signatures and documentation verified
- SDL3/SDL_render.h (local, release-3.4.2) -- `SDL_RenderTexture`, `SDL_RenderTextureRotated`, `SDL_GetTextureSize`, `SDL_SetTextureAlphaMod`, `SDL_SetTextureBlendMode`, `SDL_GetRendererFromTexture`, `SDL_DestroyTexture`, `SDL_DestroyRenderer` verified
- SDL3/SDL_surface.h (local, release-3.4.2) -- `SDL_FlipMode` enum definition verified
- SDL3/SDL_asyncio.h (local, release-3.4.2) -- `SDL_LoadFileAsync`, `SDL_GetAsyncIOResult`, `SDL_CreateAsyncIOQueue` verified
- SDL3/SDL_iostream.h (local, release-3.4.2) -- `SDL_IOFromConstMem`, `SDL_IOFromMem` verified
- SDL3_image examples/showimage.c (local) -- verified usage pattern: `IMG_Load` + `SDL_CreateTextureFromSurface` and `SDL_RenderTextureRotated` with flip/rotation
- Existing cels-sdl3 codebase (local) -- component patterns, per-TU constraint, window table pattern, renderer lifecycle

### Secondary (MEDIUM confidence)
- SDL3 wiki: SDL_DestroyRenderer documentation states "free all associated textures" -- verified from local header comment line 2637-2638
- SDL3_image no longer requires IMG_Init/IMG_Quit -- verified from absence in 3.4.0 header (no IMG_Init function declared)

### Tertiary (LOW confidence)
- (none -- all findings verified against primary sources)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- SDL3_image 3.4.0 and SDL3 3.4.2 APIs verified from local headers
- Architecture: HIGH -- Component pattern follows established codebase conventions; cache design uses proven per-renderer lookup table pattern from Phase 6
- Pitfalls: HIGH -- Renderer affinity, DestroyRenderer cascade, alpha mod leakage all verified from SDL3 header documentation

**Research date:** 2026-03-21
**Valid until:** 2026-04-21 (stable APIs, 30-day validity)
