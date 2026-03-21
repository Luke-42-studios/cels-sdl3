# Phase 8: Text Rendering - Research

**Researched:** 2026-03-21
**Domain:** SDL3_ttf 3.2.2 text engine API, font loading, TTF_Text lifecycle, ECS-based caching
**Confidence:** HIGH

## Summary

Phase 8 adds text rendering capabilities using SDL3_ttf 3.2.2's modern text engine API. The critical finding from researching the blocker is: **SDL3_ttf 3.x has a completely new text rendering model compared to SDL2_ttf.** The old approach (TTF_RenderText_Blended -> SDL_Surface -> SDL_CreateTextureFromSurface) is still available but is NOT the recommended path. SDL3_ttf 3.x introduces `TTF_TextEngine` + `TTF_Text` objects, which internally manage glyph atlas textures and handle caching at the library level. This is the API the context decisions specify, and it is the correct modern approach.

The flow is: (1) Create a `TTF_TextEngine` per renderer via `TTF_CreateRendererTextEngine(SDL_Renderer*)`, (2) Load fonts via `TTF_OpenFont(path, ptsize)`, (3) Create persistent `TTF_Text` objects via `TTF_CreateText(engine, font, string, 0)`, (4) Draw each frame with `TTF_DrawRendererText(text, x, y)`, (5) Update text via `TTF_SetTextString()` which triggers internal re-layout. The `TTF_Text` object IS the cache -- it holds the internal text representation and only regenerates when properties change. This aligns perfectly with the ECS reactivity model: `cel_update(SDL3_Text)` signals a dirty component, the text system detects dirty entities and calls `TTF_SetTextString` / `TTF_SetTextColor` / etc., and unchanged entities simply re-draw with `TTF_DrawRendererText`.

A significant design constraint: `TTF_SetFontWrapAlignment` is set on the **font**, not on the text object. If multiple text entities share the same font but need different alignments, we must either create per-alignment font copies or set alignment on the font before creating/updating each text. Since fonts in our model are shared globally by fontId, the cleanest approach is to set alignment per-text by modifying the font temporarily before text operations. However, this is fragile. The recommended approach is to handle horizontal alignment ourselves through position calculation (left = x as-is, center = x + (width - textWidth)/2, right = x + width - textWidth), reserving `TTF_SetFontWrapAlignment` only for multi-line wrapped text alignment within the wrap boundary.

**Primary recommendation:** Create per-window `TTF_TextEngine` alongside the renderer. Store `TTF_Text*` handles on ECS entities as opaque pointers managed by the text system. Use ECS dirty detection (cel_update triggers) to know when TTF_Text properties need updating. Render with `TTF_DrawRendererText` each frame for entities whose window matches. Font storage is a simple global array indexed by fontId, matching Clay's pattern.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| SDL3_ttf | 3.2.2 | TTF_OpenFont, TTF_CreateRendererTextEngine, TTF_CreateText, TTF_DrawRendererText, TTF_SetTextString, TTF_SetTextColor, TTF_SetTextWrapWidth, TTF_GetTextSize, TTF_DestroyText | Already linked in project; provides the entire text rendering pipeline |
| SDL3 | 3.4.2 | SDL_Renderer (required by text engine) | Already in project; text engine draws to SDL_Renderer |

No additional libraries needed. SDL3_ttf 3.2.2 provides all text rendering functionality.

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| (none) | - | - | Text rendering is pure SDL3_ttf + CELS ECS |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| TTF_CreateText + TTF_DrawRendererText (text engine) | TTF_RenderText_Blended -> SDL_CreateTextureFromSurface (legacy surface path) | Legacy path requires manual texture management, no built-in caching, no wrap/alignment support, more code. Text engine is the modern SDL3_ttf 3.x way. |
| Per-window TTF_TextEngine | Single global text engine | Text engine is tied to a specific SDL_Renderer. Multi-window requires per-window engines. |
| Global font array indexed by fontId | Per-entity font storage | Global array matches Clay's fontId pattern, avoids duplicating font loads, enables font sharing |

**Installation:** Already linked via CMakeLists.txt:
```cmake
FetchContent_Declare(SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG        release-3.2.2
    GIT_SHALLOW    TRUE
)
```

## Architecture Patterns

### Recommended Project Structure
```
src/
  text/
    sdl3_text.c            # NEW: font loading, text engine lifecycle, TTF_Text management
  sdl3_module.c            # MODIFIED: add text engine creation in window observer,
                           #   text render system, text cleanup observer, font registration
  sdl3_internal.h          # MODIFIED: add font/text types and function declarations
  sdl3_init.c              # EXISTING: TTF_Init/TTF_Quit already wired
include/
  cels_sdl3.h              # MODIFIED: add SDL3_Text component, font API declarations
examples/
  text/
    main.c                 # NEW: text rendering example
```

### Pattern 1: TTF_TextEngine Per Window (Tied to Renderer)
**What:** Each window entity gets a `TTF_TextEngine*` created from its `SDL_Renderer*`. The text engine manages internal glyph atlas textures for that renderer. Text objects created with one engine can only be drawn by that engine's renderer.
**When to use:** Always -- text engine is renderer-specific by SDL3_ttf design.
**Example:**
```c
// Source: SDL3_ttf 3.2.2 header + showfont.c example
// In window on_create observer, after renderer creation:
TTF_TextEngine* engine = TTF_CreateRendererTextEngine(renderer);
if (!engine) {
    SDL_Log("TTF_CreateRendererTextEngine failed: %s", SDL_GetError());
    return;
}
// Store engine pointer on the window entity (as part of a component)

// Cleanup order: destroy all TTF_Text first, then engine, then renderer
TTF_DestroyRendererTextEngine(engine);
```

### Pattern 2: Global Font Array Indexed by fontId
**What:** A static array of `TTF_Font*` pointers indexed by integer fontId. The developer calls `sdl3_font_load(FONT_BODY, "fonts/Roboto.ttf", 16)` to register a font at a specific slot. This matches Clay's font registration pattern where Clay references fonts by integer ID.
**When to use:** Always -- this is the decided font resource model.
**Example:**
```c
// Source: CONTEXT.md decision + Clay SDL3 renderer pattern
#define SDL3_MAX_FONTS 32

// Internal storage (in sdl3_text.c)
static TTF_Font* g_fonts[SDL3_MAX_FONTS] = {0};

// Public API
bool sdl3_font_load(int font_id, const char* path, float pt_size) {
    if (font_id < 0 || font_id >= SDL3_MAX_FONTS) return false;
    if (g_fonts[font_id]) {
        TTF_CloseFont(g_fonts[font_id]);
    }
    g_fonts[font_id] = TTF_OpenFont(path, pt_size);
    return g_fonts[font_id] != NULL;
}

TTF_Font* sdl3_font_get(int font_id) {
    if (font_id < 0 || font_id >= SDL3_MAX_FONTS) return NULL;
    return g_fonts[font_id];
}

void sdl3_font_close(int font_id) {
    if (font_id >= 0 && font_id < SDL3_MAX_FONTS && g_fonts[font_id]) {
        TTF_CloseFont(g_fonts[font_id]);
        g_fonts[font_id] = NULL;
    }
}

void sdl3_fonts_close_all(void) {
    for (int i = 0; i < SDL3_MAX_FONTS; i++) {
        if (g_fonts[i]) { TTF_CloseFont(g_fonts[i]); g_fonts[i] = NULL; }
    }
}
```

### Pattern 3: SDL3_Text Component with Opaque TTF_Text Handle
**What:** An ECS component stores the developer-facing text properties (string, fontId, color, position, etc.). A separate internal handle stores the `TTF_Text*` pointer. The text system bridges between the two: when the component changes (detected by ECS reactivity), the system updates the TTF_Text object. Unchanged components simply re-draw.
**When to use:** Always -- this is the core caching mechanism.
**Example:**
```c
// Public component (cels_sdl3.h)
typedef enum SDL3_TextAlign {
    SDL3_TEXT_ALIGN_LEFT = 0,
    SDL3_TEXT_ALIGN_CENTER,
    SDL3_TEXT_ALIGN_RIGHT,
} SDL3_TextAlign;

CEL_Component(SDL3_Text) {
    const char*     string;      // text content (NULL-terminated)
    int             font_id;     // index into global font array
    float           font_size;   // point size (can override font's default)
    SDL_Color       color;       // text color (default: white)
    float           x, y;        // render position
    SDL3_TextAlign  align;       // horizontal alignment
    int             wrap_width;  // 0 = no wrap, >0 = wrap at this pixel width
};

// Internal handle -- NOT a public component, managed by text system
// Stored alongside SDL3_Text or in a parallel component
CEL_Component(SDL3_TextHandle) {
    TTF_Text*    ttf_text;     // cached TTF_Text object
    uint32_t     dirty_hash;   // hash of last-applied properties for change detection
};
```

### Pattern 4: ECS Reactivity for Cache Invalidation
**What:** The text system does NOT re-create TTF_Text every frame. Instead, it checks whether the SDL3_Text component has changed since last sync. If unchanged, it simply calls `TTF_DrawRendererText`. If changed, it updates the TTF_Text properties (TTF_SetTextString, TTF_SetTextColor, etc.) then draws.
**When to use:** Always -- this fulfills TEXT-03 (cached rendering).
**Example:**
```c
// In text render system (OnRender phase):
// For each entity with SDL3_Text + SDL3_TextHandle:
//   1. Check if SDL3_Text properties changed
//   2. If dirty: sync properties to TTF_Text object
//   3. Call TTF_DrawRendererText(handle->ttf_text, text->x, text->y)

// Change detection approach: hash the component fields
// Alternative: use CELS cel_watch reactivity if available
// Simplest: store a copy of previous state and memcmp
```

### Pattern 5: Destroy-and-Recreate for Font/Engine Changes
**What:** When fontId or fontSize changes, the TTF_Text must be destroyed and recreated because `TTF_SetTextFont` changes the font but font size is a property of the font object itself. If a text entity changes fontSize, we need either a different TTF_Font (loaded at the new size) or call `TTF_SetFontSize` (which affects all text using that font -- undesirable). The clean solution: per-text-entity font copies when size differs from the loaded font's default.
**When to use:** When text entities need different sizes of the same font family.
**Important nuance:** `TTF_SetFontSize(font, ptsize)` changes the size on the font object itself, affecting ALL TTF_Text objects using that font. Clay's SDL3 renderer works around this by calling `TTF_SetFontSize` before each `TTF_CreateText` and destroying the text immediately after drawing. For cached text, this is unacceptable.
**Recommendation:** Either (a) require each fontId+size combination to be a separate font load, or (b) use `TTF_CopyFont` to create per-entity font copies when size differs. Option (a) is simpler and matches Clay's expected usage (Clay specifies exact fontId per size).

### Anti-Patterns to Avoid
- **Creating and destroying TTF_Text every frame:** This is what Clay's SDL3 renderer does (TTF_CreateText + TTF_DrawRendererText + TTF_DestroyText per frame). It works but defeats caching entirely. Our ECS approach must persist TTF_Text objects.
- **Sharing TTF_Font across different sizes without copies:** `TTF_SetFontSize` mutates the font globally. All TTF_Text objects using that font will re-layout. Each unique size needs its own font instance.
- **Setting TTF_SetFontWrapAlignment on shared fonts:** Alignment is a font-level property. Changing it affects all text using that font. Handle alignment via position math for non-wrapped text. For wrapped text, either accept font-level alignment or create per-alignment font copies.
- **Storing raw `const char*` in component without ownership:** The string pointer in SDL3_Text must either be owned (copied) by the component or guaranteed to outlive the entity. If the string is a user stack variable, it will dangle. Decision needed on ownership model.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Glyph rasterization | Custom FreeType integration | TTF_OpenFont + TTF_CreateText | SDL3_ttf wraps FreeType + HarfBuzz with caching, atlas management |
| Text texture atlas | Manual glyph atlas management | TTF_CreateRendererTextEngine (manages atlas internally) | SDL3_ttf's renderer text engine handles glyph atlas textures automatically |
| Text wrapping | Manual word-break algorithm | TTF_SetTextWrapWidth(text, width) | SDL3_ttf handles word wrapping, newline characters, and whitespace visibility |
| Text measurement | Character-by-character width accumulation | TTF_GetTextSize(text, &w, &h) or TTF_GetStringSize(font, str, 0, &w, &h) | SDL3_ttf accounts for kerning, ligatures, and HarfBuzz shaping |
| Text string mutation | Destroy and recreate TTF_Text on every string change | TTF_SetTextString(text, new_string, 0) | Preserves the text object, only triggers re-layout when content changes |

**Key insight:** SDL3_ttf 3.x's text engine API manages the entire rendering pipeline internally -- glyph rasterization, atlas packing, texture creation, and GPU upload. The developer's job is to create TTF_Text objects, update their properties when they change, and call TTF_DrawRendererText each frame. The heavy lifting is invisible.

## Common Pitfalls

### Pitfall 1: TTF_TextEngine Lifetime vs Renderer Lifetime
**What goes wrong:** The text engine is created from an SDL_Renderer. If the renderer is destroyed before all TTF_Text objects and the engine, SDL3_ttf will crash or leak resources.
**Why it happens:** Destruction order is critical: TTF_Text objects -> TTF_TextEngine -> SDL_Renderer.
**How to avoid:** Destroy text handles in an on_destroy observer (or cleanup system) that runs BEFORE renderer destruction. Destroy the text engine in the same observer, after all text handles. The existing CLOSING->CLOSED pattern already handles renderer destruction in WindowStateSystem -- text cleanup must happen before that.
**Warning signs:** Crash during window close, or "invalid renderer" errors from SDL3_ttf.

### Pitfall 2: TTF_SetFontSize Mutates All Text Using That Font
**What goes wrong:** Developer loads font at size 16, creates text A at size 16. Later creates text B wanting size 24. Calls `TTF_SetFontSize(font, 24)` -- now text A also becomes size 24 because TTF_SetFontSize changes the font object globally.
**Why it happens:** `TTF_SetFontSize` modifies the font, and "this updates any TTF_Text objects using this font" (per SDL3_ttf docs).
**How to avoid:** Each fontId+size pair must be a separate font instance. Either require developers to register `sdl3_font_load(FONT_BODY_16, "Roboto.ttf", 16)` and `sdl3_font_load(FONT_BODY_24, "Roboto.ttf", 24)` separately, or use `TTF_CopyFont` internally to clone fonts when size differs.
**Warning signs:** All text rendered at the same unexpected size.

### Pitfall 3: TTF_SetFontWrapAlignment Is Font-Level, Not Text-Level
**What goes wrong:** Two text entities share the same font. Entity A wants left alignment, entity B wants center alignment. Calling `TTF_SetFontWrapAlignment(font, TTF_HORIZONTAL_ALIGN_CENTER)` before drawing entity B also changes entity A's alignment if the text is wrapped.
**Why it happens:** Alignment in SDL3_ttf 3.2.2 is set on the TTF_Font, not on the TTF_Text.
**How to avoid:** For non-wrapped text: compute alignment via position math (offset x based on text width). For wrapped text with different alignments: use separate font instances per alignment, or accept the limitation and set alignment before each draw (understanding it affects all text on that font).
**Warning signs:** All wrapped text having the same alignment regardless of per-entity settings.

### Pitfall 4: Per-TU Static ID Constraint (Existing Pattern)
**What goes wrong:** Text functions in sdl3_text.c need to access components but cannot use cel_query/cel_each.
**Why it happens:** Same per-TU constraint as all other implementation files in this project.
**How to avoid:** All cel_query/cel_each/cel_update code lives in sdl3_module.c. Helper functions in sdl3_text.c receive data via explicit parameters (pointers, tables). This matches the window table pattern from Phase 4.
**Warning signs:** Component IDs are 0, draw calls appear to succeed but nothing renders.

### Pitfall 5: cels_ensure_component for New Components
**What goes wrong:** SDL3_Text and SDL3_TextHandle components have uninitialized IDs when observers try to access them.
**Why it happens:** CEL_Component _register() is a no-op. Actual ID assignment requires cels_ensure_component.
**How to avoid:** Add `cels_ensure_component(&SDL3_Text_id, ...)` and `cels_ensure_component(&SDL3_TextHandle_id, ...)` in CEL_Module(SDL3_Engine, init).
**Warning signs:** Text components silently not attached, NULL component reads.

### Pitfall 6: Text Engine Destroyed Before Text Objects
**What goes wrong:** SDL3_ttf documentation states "All text created by this engine should be destroyed before calling this function" for TTF_DestroyRendererTextEngine.
**Why it happens:** Destroying the engine while TTF_Text objects still reference it causes undefined behavior.
**How to avoid:** In the window CLOSING->CLOSED transition, iterate all text entities and destroy their TTF_Text handles before destroying the text engine. Alternatively, use an ECS lifecycle observer pattern: text entity on_destroy -> TTF_DestroyText.
**Warning signs:** Crash during shutdown, double-free errors.

### Pitfall 7: TTF_OpenFont Uses Float for ptsize
**What goes wrong:** Developer passes integer point size, gets unexpected results or warnings.
**Why it happens:** SDL3_ttf 3.x changed ptsize from int to float. `TTF_OpenFont(const char *file, float ptsize)`.
**How to avoid:** Use float for all point size parameters. The component's `font_size` field should be float.
**Warning signs:** Compiler warnings about int-to-float conversion.

### Pitfall 8: String Ownership in SDL3_Text Component
**What goes wrong:** Developer sets `SDL3_Text.string` to a stack-allocated string. The text system later reads the pointer -- it's now dangling garbage.
**Why it happens:** C has no ownership semantics. A `const char*` in a component is just a pointer.
**How to avoid:** Document clearly that strings must outlive the entity (static strings, heap-allocated, or string table). Alternatively, the text system could strdup the string internally and free on destroy/change. The simpler approach: require stable string pointers (same as Clay's approach).
**Warning signs:** Garbled text, crashes in TTF_SetTextString.

## Code Examples

Verified patterns from SDL3_ttf 3.2.2 header and official examples:

### Complete Text Engine Lifecycle
```c
// Source: SDL3_ttf 3.2.2 SDL_ttf.h, verified against showfont.c

// 1. Init (already done in sdl3_init.c)
TTF_Init();

// 2. Load font
TTF_Font* font = TTF_OpenFont("fonts/Roboto-Regular.ttf", 16.0f);
if (!font) {
    SDL_Log("TTF_OpenFont failed: %s", SDL_GetError());
}

// 3. Create text engine for a specific renderer
TTF_TextEngine* engine = TTF_CreateRendererTextEngine(renderer);
if (!engine) {
    SDL_Log("TTF_CreateRendererTextEngine failed: %s", SDL_GetError());
}

// 4. Create text object (cached -- persist across frames)
TTF_Text* text = TTF_CreateText(engine, font, "Hello World", 0);
TTF_SetTextColor(text, 255, 255, 255, 255);

// 5. Each frame: just draw (no re-creation needed)
TTF_DrawRendererText(text, 100.0f, 50.0f);

// 6. When text content changes:
TTF_SetTextString(text, "New text", 0);  // triggers internal re-layout

// 7. When text color changes:
TTF_SetTextColor(text, 255, 0, 0, 255);

// 8. When wrapping is needed:
TTF_SetTextWrapWidth(text, 200);  // wrap at 200 pixels

// 9. Get text dimensions (for alignment calculation):
int w, h;
TTF_GetTextSize(text, &w, &h);

// 10. Cleanup (ORDER MATTERS):
TTF_DestroyText(text);                    // first: destroy all text objects
TTF_DestroyRendererTextEngine(engine);     // second: destroy engine
TTF_CloseFont(font);                      // third: close fonts
TTF_Quit();                               // last: shutdown ttf
```

### TTF_Text Modification Functions (All Available)
```c
// Source: SDL3_ttf 3.2.2 SDL_ttf.h

// String operations
bool TTF_SetTextString(TTF_Text *text, const char *string, size_t length);
bool TTF_InsertTextString(TTF_Text *text, int offset, const char *string, size_t length);
bool TTF_AppendTextString(TTF_Text *text, const char *string, size_t length);
bool TTF_DeleteTextString(TTF_Text *text, int offset, int length);

// Visual properties
bool TTF_SetTextColor(TTF_Text *text, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
bool TTF_SetTextColorFloat(TTF_Text *text, float r, float g, float b, float a);
bool TTF_SetTextFont(TTF_Text *text, TTF_Font *font);
bool TTF_SetTextPosition(TTF_Text *text, int x, int y);
bool TTF_SetTextWrapWidth(TTF_Text *text, int wrap_width);
bool TTF_SetTextWrapWhitespaceVisible(TTF_Text *text, bool visible);
bool TTF_SetTextEngine(TTF_Text *text, TTF_TextEngine *engine);
bool TTF_SetTextDirection(TTF_Text *text, TTF_Direction direction);

// Measurement
bool TTF_GetTextSize(TTF_Text *text, int *w, int *h);
bool TTF_GetTextPosition(TTF_Text *text, int *x, int *y);
```

### Font Measurement Functions (Without TTF_Text Object)
```c
// Source: SDL3_ttf 3.2.2 SDL_ttf.h

// Measure a string without creating a TTF_Text object
bool TTF_GetStringSize(TTF_Font *font, const char *text, size_t length, int *w, int *h);
bool TTF_GetStringSizeWrapped(TTF_Font *font, const char *text, size_t length,
                               int wrap_width, int *w, int *h);
bool TTF_MeasureString(TTF_Font *font, const char *text, size_t length,
                        int max_width, int *measured_width, size_t *measured_length);

// Font metrics
int  TTF_GetFontHeight(const TTF_Font *font);
int  TTF_GetFontAscent(const TTF_Font *font);
int  TTF_GetFontDescent(const TTF_Font *font);
int  TTF_GetFontLineSkip(const TTF_Font *font);
void TTF_SetFontLineSkip(TTF_Font *font, int lineskip);
```

### Font Properties Set on TTF_Font (Not TTF_Text)
```c
// Source: SDL3_ttf 3.2.2 SDL_ttf.h
// IMPORTANT: These affect ALL TTF_Text objects using this font!

bool TTF_SetFontSize(TTF_Font *font, float ptsize);
void TTF_SetFontStyle(TTF_Font *font, TTF_FontStyleFlags style);
void TTF_SetFontWrapAlignment(TTF_Font *font, TTF_HorizontalAlignment align);
void TTF_SetFontKerning(TTF_Font *font, bool enabled);
void TTF_SetFontLineSkip(TTF_Font *font, int lineskip);
bool TTF_AddFallbackFont(TTF_Font *font, TTF_Font *fallback);

// Available styles: TTF_STYLE_NORMAL, TTF_STYLE_BOLD, TTF_STYLE_ITALIC,
//                   TTF_STYLE_UNDERLINE, TTF_STYLE_STRIKETHROUGH
// Available alignments: TTF_HORIZONTAL_ALIGN_LEFT, TTF_HORIZONTAL_ALIGN_CENTER,
//                       TTF_HORIZONTAL_ALIGN_RIGHT
```

### Clay SDL3 Renderer Text Pattern (Reference -- DO NOT copy this approach)
```c
// Source: github.com/nicbarker/clay renderers/SDL3/clay_renderer_SDL3.c
// Clay creates+destroys text every frame -- this works but is NOT cached:

TTF_Font *font = rendererData->fonts[config->fontId];
TTF_SetFontSize(font, config->fontSize);  // MUTATES shared font!
TTF_Text *text = TTF_CreateText(rendererData->textEngine, font,
                                 config->stringContents.chars,
                                 config->stringContents.length);
TTF_SetTextColor(text, r, g, b, a);
TTF_DrawRendererText(text, rect.x, rect.y);
TTF_DestroyText(text);  // destroyed every frame -- no caching

// Our approach: persist TTF_Text, update only when properties change
```

### TTF_Text Struct (Public Fields)
```c
// Source: SDL3_ttf 3.2.2 SDL_ttf.h line 1723-1732
typedef struct TTF_Text {
    char *text;             // A copy of the UTF-8 string (read-only, auto-managed)
    int num_lines;          // Number of lines in the text, 0 if empty
    int refcount;           // Application reference count
    TTF_TextData *internal; // Private internal data
} TTF_Text;
```

## State of the Art

| Old Approach (SDL2_ttf) | Current Approach (SDL3_ttf 3.x) | When Changed | Impact |
|--------------------------|----------------------------------|--------------|--------|
| TTF_RenderText_Blended -> SDL_Surface -> SDL_CreateTextureFromSurface | TTF_CreateRendererTextEngine + TTF_CreateText + TTF_DrawRendererText | SDL_ttf 3.0.0 | Eliminates manual surface->texture conversion; library manages glyph atlas internally |
| No persistent text objects | TTF_Text objects persist across frames | SDL_ttf 3.0.0 | Built-in caching -- only re-renders when properties change |
| TTF_RenderUTF8_Blended (separate UTF-8 functions) | TTF_RenderText_Blended (all text is UTF-8 by default) | SDL_ttf 3.0.0 | Simplified API; all functions accept UTF-8 |
| int ptsize parameter | float ptsize parameter | SDL_ttf 3.0.0 | More precise font sizing |
| TTF_SizeText (returns int) | TTF_GetStringSize / TTF_GetTextSize (bool return) | SDL_ttf 3.0.0 | Consistent bool return convention |
| No text engine concept | Three engines: Surface, Renderer, GPU | SDL_ttf 3.0.0 | Unified text object model with backend-specific rendering |
| No TTF_SetTextString | TTF_SetTextString + TTF_InsertTextString + TTF_AppendTextString | SDL_ttf 3.0.0 | Text modification without recreation |
| No alignment API | TTF_SetFontWrapAlignment (LEFT, CENTER, RIGHT) | SDL_ttf 3.0.0 | Built-in alignment for wrapped text |
| No fallback fonts | TTF_AddFallbackFont / TTF_RemoveFallbackFont | SDL_ttf 3.0.0 | Multi-font glyph coverage (emoji, CJK, etc.) |
| No TTF_CopyFont | TTF_CopyFont for independent size/style copies | SDL_ttf 3.0.0 | Safe per-entity font variants |

**Deprecated/outdated:**
- `TTF_RenderUTF8_*`: Renamed to `TTF_RenderText_*` (all text is UTF-8 in SDL3_ttf)
- `TTF_SizeText` / `TTF_SizeUTF8`: Replaced by `TTF_GetStringSize`
- `int` point sizes: Now `float` throughout
- Manual surface->texture pipeline for text: Replaced by text engine API

## Open Questions

1. **String ownership in SDL3_Text component**
   - What we know: SDL3_Text.string is a `const char*`. TTF_SetTextString makes its own internal copy. But the ECS component stores the pointer for change detection.
   - What's unclear: Should the text system strdup the string for safety, or require developers to ensure pointer stability?
   - Recommendation: Require stable pointers (static strings or heap-allocated). Document this clearly. This matches Clay's approach where string contents are referenced by pointer. Avoids hidden allocation/deallocation in the text system.

2. **Per-entity font size handling**
   - What we know: The SDL3_Text component has a `font_size` field. But `TTF_SetFontSize` mutates the shared font. Clay works around this by calling SetFontSize before each CreateText, but we cache text objects.
   - What's unclear: Whether to (a) require each size as a separate fontId registration, (b) use TTF_CopyFont internally, or (c) use the font_size field only as documentation and require fontId to encode size.
   - Recommendation: Option (a) -- each fontId encodes a specific family+size. This is simplest, matches Clay's usage, and avoids internal font cloning complexity. Remove `font_size` from the component and let fontId fully determine the font. If size override is needed later, add it as a v2 feature using TTF_CopyFont.

3. **Letter spacing in SDL3_Text component**
   - What we know: The CONTEXT.md mentions letter/line spacing as component properties. SDL3_ttf 3.2.2 has `TTF_SetFontLineSkip` for line spacing (font-level) but NO letter spacing API at all.
   - What's unclear: Whether to include letter_spacing in the component as a no-op placeholder.
   - Recommendation: Omit letter_spacing from the component. Include line_spacing as a font-level configuration in sdl3_font_load, not per-text. Document this limitation.

4. **Multi-window text entity association**
   - What we know: Text engines are per-window (per-renderer). A TTF_Text object is bound to the engine that created it. Multi-window support means a text entity needs to know which window it belongs to.
   - What's unclear: Whether text entities should explicitly reference a window entity, or whether the system queries text+window pairs differently.
   - Recommendation: Add a window_entity field to SDL3_Text (or co-locate text and window components via ECS query). For single-window apps, this can default to the first window found. For multi-window, explicit association is required.

5. **TTF_TextEngine storage location**
   - What we know: The engine is created per-renderer and must be destroyed before the renderer. It needs to be accessible during text creation and during cleanup.
   - What's unclear: Whether to store it in the SDL3_Renderer component, a new SDL3_TextEngine component, or a static lookup.
   - Recommendation: Add `TTF_TextEngine*` field to the existing SDL3_Renderer component. This keeps engine lifetime coupled to renderer lifetime (correct) and avoids a new component.

## Sources

### Primary (HIGH confidence)
- SDL3_ttf 3.2.2 `include/SDL3_ttf/SDL_ttf.h` (local, `/home/cachy/workspaces/libs/cels-sdl3/cmake-build-debug/_deps/sdl3_ttf-src/include/SDL3_ttf/SDL_ttf.h`) - Complete API reference, all function signatures verified
- SDL3_ttf showfont.c example (local, `/home/cachy/workspaces/libs/cels-sdl3/cmake-build-debug/_deps/sdl3_ttf-src/examples/showfont.c`) - Official renderer text engine usage pattern
- SDL3_ttf editbox.c example (local, `/home/cachy/workspaces/libs/cels-sdl3/cmake-build-debug/_deps/sdl3_ttf-src/examples/editbox.c`) - TTF_Text persistence pattern, string modification
- [SDL Wiki: TTF_CreateRendererTextEngine](https://wiki.libsdl.org/SDL3_ttf/TTF_CreateRendererTextEngine) - Official API documentation
- [SDL Wiki: TTF_CreateText](https://wiki.libsdl.org/SDL3_ttf/TTF_CreateText) - Official API documentation
- [SDL Wiki: TTF_DrawRendererText](https://wiki.libsdl.org/SDL3_ttf/TTF_DrawRendererText) - Official API documentation
- Existing cels-sdl3 codebase (local) - Per-TU constraint, component patterns, lifecycle observers, module registration

### Secondary (MEDIUM confidence)
- [Clay SDL3 renderer](https://github.com/nicbarker/clay/blob/main/renderers/SDL3/clay_renderer_SDL3.c) - Reference implementation showing font array + text engine pattern (create/destroy per frame approach)
- [Clay issue #254](https://github.com/nicbarker/clay/issues/254) - Text rendering issues and fix using text engine

### Tertiary (LOW confidence)
- (none -- all findings verified against primary SDL3_ttf header source)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - SDL3_ttf 3.2.2 API fully verified via local header file
- Architecture: HIGH - Text engine pattern verified against official examples (showfont.c, editbox.c); component design follows established cels-sdl3 patterns
- Pitfalls: HIGH - Font mutation gotchas (SetFontSize, SetFontWrapAlignment affecting all text) verified directly from header documentation; destruction order documented in API comments
- API differences from SDL2: HIGH - Complete function signature comparison done against local header

**Research date:** 2026-03-21
**Valid until:** 2026-04-21 (stable API, 30-day validity)
