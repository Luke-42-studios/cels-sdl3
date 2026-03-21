/*
 * SDL3 Draw Primitives - Buffer, Vtable, Flush
 *
 * Implements the SDL3_Renderable vtable provider. Draw calls buffer
 * commands into per-window draw buffers (embedded in SDL3_Renderer
 * component). The flush function sorts by (z, order) and issues SDL3
 * draw calls.
 *
 * CANNOT use cel_query/cel_each (per-TU static ID constraint).
 * Uses the SDL3_DrawBufferTable for renderer-to-buffer lookup.
 */

#include <cels_sdl3.h>
#include "../sdl3_internal.h"
#include <stdlib.h>
#include <assert.h>

/* ============================================================================
 * Static globals
 * ============================================================================ */

static SDL3_DrawBufferTable s_draw_table = {0};
static SDL3_Renderable s_renderable = {0};
static bool s_renderable_registered = false;

/* ============================================================================
 * Draw buffer lifecycle
 * ============================================================================ */

void sdl3_draw_buffer_init(SDL3_Renderer* comp)
{
    comp->draw_cmds = (SDL3_DrawCmd*)malloc(
        SDL3_DRAW_BUFFER_INITIAL_CAPACITY * sizeof(SDL3_DrawCmd));
    comp->draw_count = 0;
    comp->draw_capacity = SDL3_DRAW_BUFFER_INITIAL_CAPACITY;
    comp->draw_next_order = 0;
}

void sdl3_draw_buffer_clear(SDL3_Renderer* comp)
{
    comp->draw_count = 0;
    comp->draw_next_order = 0;
}

void sdl3_draw_buffer_push(SDL3_Renderer* comp, SDL3_DrawCmd cmd)
{
    if (comp->draw_count == comp->draw_capacity) {
        comp->draw_capacity *= 2;
        comp->draw_cmds = (SDL3_DrawCmd*)realloc(
            comp->draw_cmds,
            (size_t)comp->draw_capacity * sizeof(SDL3_DrawCmd));
    }
    cmd.order = comp->draw_next_order++;
    comp->draw_cmds[comp->draw_count++] = cmd;
}

static int sdl3_draw_cmd_compare(const void* a, const void* b)
{
    const SDL3_DrawCmd* ca = (const SDL3_DrawCmd*)a;
    const SDL3_DrawCmd* cb = (const SDL3_DrawCmd*)b;
    if (ca->z != cb->z) return (ca->z > cb->z) - (ca->z < cb->z);
    return (ca->order > cb->order) - (ca->order < cb->order);
}

void sdl3_draw_buffer_flush(SDL3_Renderer* comp)
{
    if (comp->draw_count == 0) return;

    qsort(comp->draw_cmds, (size_t)comp->draw_count,
          sizeof(SDL3_DrawCmd), sdl3_draw_cmd_compare);

    SDL_Renderer* r = comp->renderer;
    for (int i = 0; i < comp->draw_count; i++) {
        SDL3_DrawCmd* cmd = &comp->draw_cmds[i];
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

void sdl3_draw_buffer_destroy(SDL3_Renderer* comp)
{
    free(comp->draw_cmds);
    comp->draw_cmds = NULL;
    comp->draw_count = 0;
    comp->draw_capacity = 0;
    comp->draw_next_order = 0;
}

/* ============================================================================
 * Renderer-to-buffer lookup table
 * ============================================================================ */

void sdl3_draw_table_clear(void)
{
    s_draw_table.count = 0;
}

void sdl3_draw_table_add(SDL_Renderer* renderer, SDL3_Renderer* comp)
{
    if (s_draw_table.count < SDL3_MAX_WINDOWS) {
        s_draw_table.entries[s_draw_table.count++] = (SDL3_DrawBufferEntry){
            .renderer = renderer,
            .comp = comp
        };
    }
}

SDL3_Renderer* sdl3_draw_table_find(SDL_Renderer* renderer)
{
    for (int i = 0; i < s_draw_table.count; i++) {
        if (s_draw_table.entries[i].renderer == renderer) {
            return s_draw_table.entries[i].comp;
        }
    }
    return NULL;
}

/* ============================================================================
 * Vtable implementation functions (buffer commands, not immediate SDL calls)
 * ============================================================================ */

static void sdl3_impl_filled_rect(SDL_Renderer* r, SDL_FRect rect,
                                   SDL_Color color, int z)
{
    SDL3_Renderer* comp = sdl3_draw_table_find(r);
    if (!comp) return;
    sdl3_draw_buffer_push(comp, (SDL3_DrawCmd){
        .type = SDL3_DRAW_FILLED_RECT,
        .z = z,
        .color = color,
        .rect = rect
    });
}

static void sdl3_impl_filled_rects(SDL_Renderer* r, const SDL_FRect* rects,
                                    const SDL_Color* colors, int count, int z)
{
    SDL3_Renderer* comp = sdl3_draw_table_find(r);
    if (!comp) return;
    for (int i = 0; i < count; i++) {
        sdl3_draw_buffer_push(comp, (SDL3_DrawCmd){
            .type = SDL3_DRAW_FILLED_RECT,
            .z = z,
            .color = colors[i],
            .rect = rects[i]
        });
    }
}

static void sdl3_impl_outlined_rect(SDL_Renderer* r, SDL_FRect rect,
                                     SDL_Color color, int z)
{
    SDL3_Renderer* comp = sdl3_draw_table_find(r);
    if (!comp) return;
    sdl3_draw_buffer_push(comp, (SDL3_DrawCmd){
        .type = SDL3_DRAW_OUTLINED_RECT,
        .z = z,
        .color = color,
        .rect = rect
    });
}

static void sdl3_impl_outlined_rects(SDL_Renderer* r, const SDL_FRect* rects,
                                      const SDL_Color* colors, int count, int z)
{
    SDL3_Renderer* comp = sdl3_draw_table_find(r);
    if (!comp) return;
    for (int i = 0; i < count; i++) {
        sdl3_draw_buffer_push(comp, (SDL3_DrawCmd){
            .type = SDL3_DRAW_OUTLINED_RECT,
            .z = z,
            .color = colors[i],
            .rect = rects[i]
        });
    }
}

static void sdl3_impl_line(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b,
                            SDL_Color color, int z)
{
    SDL3_Renderer* comp = sdl3_draw_table_find(r);
    if (!comp) return;
    sdl3_draw_buffer_push(comp, (SDL3_DrawCmd){
        .type = SDL3_DRAW_LINE,
        .z = z,
        .color = color,
        .line = { .a = a, .b = b }
    });
}

/* ============================================================================
 * Public API
 * ============================================================================ */

const SDL3_Renderable* sdl3_renderable(void)
{
    assert(s_renderable_registered &&
           "SDL3_Renderable_use() must be called before sdl3_renderable()");
    return &s_renderable;
}

void SDL3_Renderable_use(void)
{
    s_renderable = (SDL3_Renderable){
        .filled_rect    = sdl3_impl_filled_rect,
        .filled_rects   = sdl3_impl_filled_rects,
        .outlined_rect  = sdl3_impl_outlined_rect,
        .outlined_rects = sdl3_impl_outlined_rects,
        .line           = sdl3_impl_line
    };
    s_renderable_registered = true;
}

void sdl3_set_blend_mode(SDL_Renderer* renderer, SDL_BlendMode mode)
{
    SDL_SetRenderDrawBlendMode(renderer, mode);
}
