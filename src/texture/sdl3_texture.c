/*
 * SDL3 Texture Cache -- per-renderer texture caching with reference counting
 *
 * Each SDL_Renderer gets its own texture cache because textures are
 * renderer-affine (an SDL_Texture cannot be used with a different renderer).
 * A renderer-to-cache lookup table maps SDL_Renderer* -> SDL3_TextureCache.
 *
 * Textures are keyed by resolved file path. Multiple sprites referencing
 * the same image share one GPU texture with a reference count. When the
 * last reference is released, the texture is freed.
 *
 * On renderer destruction, sdl3_texture_cache_invalidate zeroes all
 * entries WITHOUT calling SDL_DestroyTexture -- SDL_DestroyRenderer
 * handles that automatically. Calling DestroyTexture on already-freed
 * textures would cause double-free crashes.
 */

#include <cels_sdl3.h>
#include "../sdl3_internal.h"
#include <string.h>

/* ============================================================================
 * Static globals
 * ============================================================================ */

#define SDL3_MAX_RENDERERS 8

static struct {
    SDL_Renderer*      renderer;
    SDL3_TextureCache  cache;
} s_cache_table[SDL3_MAX_RENDERERS];
static int s_cache_count = 0;

static char s_asset_base_dir[SDL3_TEXTURE_PATH_MAX] = {0};

/* ============================================================================
 * Asset base directory
 * ============================================================================ */

void sdl3_set_asset_base_dir(const char* path) {
    if (!path) {
        s_asset_base_dir[0] = '\0';
        return;
    }
    SDL_strlcpy(s_asset_base_dir, path, SDL3_TEXTURE_PATH_MAX);

    /* Strip trailing slash if present */
    size_t len = SDL_strlen(s_asset_base_dir);
    if (len > 0 && s_asset_base_dir[len - 1] == '/') {
        s_asset_base_dir[len - 1] = '\0';
    }
}

/* ============================================================================
 * Renderer-to-cache lookup table
 * ============================================================================ */

SDL3_TextureCache* sdl3_texture_cache_for_renderer(SDL_Renderer* renderer) {
    /* Search existing entries */
    for (int i = 0; i < s_cache_count; i++) {
        if (s_cache_table[i].renderer == renderer) {
            return &s_cache_table[i].cache;
        }
    }

    /* Create new entry */
    if (s_cache_count >= SDL3_MAX_RENDERERS) {
        SDL_Log("SDL3: Texture cache table full (%d renderers)", SDL3_MAX_RENDERERS);
        return NULL;
    }

    int idx = s_cache_count;
    s_cache_table[idx].renderer = renderer;
    memset(&s_cache_table[idx].cache, 0, sizeof(SDL3_TextureCache));
    s_cache_count++;

    return &s_cache_table[idx].cache;
}

/* ============================================================================
 * Texture loading and lookup
 * ============================================================================ */

uint32_t sdl3_texture_cache_load(SDL3_TextureCache* cache,
                                  SDL_Renderer* renderer,
                                  const char* path) {
    if (!cache || !renderer || !path) return 0;

    /* Resolve full path */
    char resolved[SDL3_TEXTURE_PATH_MAX];
    if (s_asset_base_dir[0] != '\0') {
        SDL_snprintf(resolved, sizeof(resolved), "%s/%s", s_asset_base_dir, path);
    } else {
        SDL_strlcpy(resolved, path, sizeof(resolved));
    }

    /* Check if already cached */
    for (int i = 0; i < cache->count; i++) {
        if (cache->entries[i].texture != NULL &&
            SDL_strcmp(cache->entries[i].path, resolved) == 0) {
            cache->entries[i].ref_count++;
            return (uint32_t)(i + 1);  /* 1-based handle */
        }
    }

    /* Load new texture */
    SDL_Texture* tex = IMG_LoadTexture(renderer, resolved);
    if (!tex) {
        return 0;  /* Caller handles FAILED state */
    }

    /* Enable alpha blending */
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    /* Find an empty slot or append */
    int slot = -1;
    for (int i = 0; i < cache->count; i++) {
        if (cache->entries[i].texture == NULL) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (cache->count >= SDL3_TEXTURE_CACHE_CAPACITY) {
            SDL_Log("SDL3: Texture cache full (%d entries)", SDL3_TEXTURE_CACHE_CAPACITY);
            SDL_DestroyTexture(tex);
            return 0;
        }
        slot = cache->count;
        cache->count++;
    }

    /* Fill entry */
    SDL3_TextureCacheEntry* entry = &cache->entries[slot];
    SDL_strlcpy(entry->path, resolved, SDL3_TEXTURE_PATH_MAX);
    entry->texture = tex;
    SDL_GetTextureSize(tex, &entry->width, &entry->height);
    entry->ref_count = 1;

    return (uint32_t)(slot + 1);  /* 1-based handle */
}

SDL_Texture* sdl3_texture_cache_get(const SDL3_TextureCache* cache,
                                     uint32_t handle) {
    if (handle == 0 || handle > (uint32_t)cache->count) return NULL;
    return cache->entries[handle - 1].texture;
}

void sdl3_texture_cache_get_size(const SDL3_TextureCache* cache,
                                  uint32_t handle,
                                  float* w, float* h) {
    if (handle == 0 || handle > (uint32_t)cache->count) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    const SDL3_TextureCacheEntry* entry = &cache->entries[handle - 1];
    if (w) *w = entry->width;
    if (h) *h = entry->height;
}

/* ============================================================================
 * Reference counting and cleanup
 * ============================================================================ */

void sdl3_texture_cache_release(SDL3_TextureCache* cache, uint32_t handle) {
    if (handle == 0 || handle > (uint32_t)cache->count) return;

    SDL3_TextureCacheEntry* entry = &cache->entries[handle - 1];
    if (entry->ref_count > 0) {
        entry->ref_count--;
    }
    if (entry->ref_count == 0 && entry->texture != NULL) {
        SDL_DestroyTexture(entry->texture);
        entry->texture = NULL;
        entry->path[0] = '\0';
    }
}

void sdl3_texture_cache_invalidate(SDL3_TextureCache* cache) {
    /* Do NOT call SDL_DestroyTexture here -- SDL_DestroyRenderer
     * handles that automatically. Calling DestroyTexture on
     * already-freed textures would cause double-free crashes. */
    for (int i = 0; i < cache->count; i++) {
        cache->entries[i].texture = NULL;
        cache->entries[i].path[0] = '\0';
        cache->entries[i].ref_count = 0;
    }
    cache->count = 0;
}

void sdl3_texture_cache_remove_renderer(SDL_Renderer* renderer) {
    for (int i = 0; i < s_cache_count; i++) {
        if (s_cache_table[i].renderer == renderer) {
            sdl3_texture_cache_invalidate(&s_cache_table[i].cache);

            /* Remove from table by shifting remaining entries down */
            for (int j = i; j < s_cache_count - 1; j++) {
                s_cache_table[j] = s_cache_table[j + 1];
            }
            s_cache_count--;
            return;
        }
    }
}
