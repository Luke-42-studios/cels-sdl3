/*
 * SDL3 Text - Font loading, TTF_Text lifecycle helpers
 *
 * Global font array indexed by fontId. Each fontId encodes a specific
 * family+size combination. TTF_Text helpers are called from systems
 * in sdl3_module.c (per-TU static ID constraint prevents cel_query here).
 */

#include <cels_sdl3.h>
#include "../sdl3_internal.h"

/* ============================================================================
 * Global Font Array
 * ============================================================================ */

static TTF_Font* g_fonts[SDL3_MAX_FONTS] = {0};

/* ============================================================================
 * Font API
 * ============================================================================ */

bool sdl3_font_load(int font_id, const char* path, float pt_size)
{
    sdl3_ensure_init();

    if (font_id < 0 || font_id >= SDL3_MAX_FONTS) {
        sdl3_report_error("font_load_invalid_id");
        return false;
    }

    /* Close existing font at this slot if any */
    if (g_fonts[font_id]) {
        TTF_CloseFont(g_fonts[font_id]);
        g_fonts[font_id] = NULL;
    }

    TTF_Font* font = TTF_OpenFont(path, pt_size);
    if (!font) {
        sdl3_report_error("font_load");
        return false;
    }

    g_fonts[font_id] = font;
    return true;
}

TTF_Font* sdl3_font_get(int font_id)
{
    if (font_id < 0 || font_id >= SDL3_MAX_FONTS) return NULL;
    return g_fonts[font_id];
}

void sdl3_font_close(int font_id)
{
    if (font_id >= 0 && font_id < SDL3_MAX_FONTS && g_fonts[font_id]) {
        TTF_CloseFont(g_fonts[font_id]);
        g_fonts[font_id] = NULL;
    }
}

void sdl3_fonts_close_all(void)
{
    for (int i = 0; i < SDL3_MAX_FONTS; i++) {
        if (g_fonts[i]) {
            TTF_CloseFont(g_fonts[i]);
            g_fonts[i] = NULL;
        }
    }
}

/* ============================================================================
 * TTF_Text Lifecycle Helpers
 * ============================================================================ */

TTF_Text* sdl3_text_create(TTF_TextEngine* engine, int font_id,
                            const char* string)
{
    TTF_Font* font = sdl3_font_get(font_id);
    if (!font) {
        sdl3_report_error("text_create_no_font");
        return NULL;
    }

    TTF_Text* text = TTF_CreateText(engine, font, string, 0);
    if (!text) {
        sdl3_report_error("text_create");
        return NULL;
    }

    return text;
}

bool sdl3_text_sync(TTF_Text* ttf_text, const SDL3_Text* text,
                     const SDL3_TextHandle* handle)
{
    bool dirty = false;

    /* String pointer change detection */
    if (text->string != handle->last_string) {
        TTF_SetTextString(ttf_text, text->string, 0);
        dirty = true;
    }

    /* Color change detection */
    if (text->color.r != handle->last_color.r ||
        text->color.g != handle->last_color.g ||
        text->color.b != handle->last_color.b ||
        text->color.a != handle->last_color.a) {
        TTF_SetTextColor(ttf_text, text->color.r, text->color.g,
                         text->color.b, text->color.a);
        dirty = true;
    }

    /* Wrap width change detection */
    if (text->wrap_width != handle->last_wrap) {
        TTF_SetTextWrapWidth(ttf_text, text->wrap_width);
        dirty = true;
    }

    /* Font change detection -- requires destroy and recreate (caller handles) */
    if (text->font_id != handle->last_font_id) {
        TTF_Font* new_font = sdl3_font_get(text->font_id);
        if (new_font) {
            TTF_SetTextFont(ttf_text, new_font);
        }
        dirty = true;
    }

    return dirty;
}

void sdl3_text_destroy(TTF_Text* ttf_text)
{
    if (ttf_text) {
        TTF_DestroyText(ttf_text);
    }
}
