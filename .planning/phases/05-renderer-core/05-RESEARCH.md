# Phase 5: Renderer Core - Research

**Researched:** 2026-03-21
**Domain:** SDL3 renderer lifecycle, clear/present cycle, CELS system phases for render ordering
**Confidence:** HIGH

## Summary

Phase 5 pairs each window entity with an `SDL_Renderer` stored as a separate ECS component, runs a clear/present cycle every frame using CELS built-in pipeline phases, and exposes a per-window clear color. The SDL3 renderer API is straightforward: `SDL_CreateRenderer(window, NULL)` creates a hardware-accelerated renderer, `SDL_SetRenderDrawColor` + `SDL_RenderClear` fills the backbuffer, and `SDL_RenderPresent` flips it to screen. All renderer functions must be called from the main thread (which CELS systems already guarantee).

The CELS framework provides five user-facing pipeline phases: `OnLoad`, `OnUpdate`, `PreRender`, `OnRender`, `PostRender`. These map perfectly to the render cycle: a clear system at `PreRender`, developer draw systems at `OnRender` (future phases), and a present system at `PostRender`. No custom phases needed -- the built-in phases already define the correct ordering.

The critical implementation detail is the `SDL_GetWindowSurface` / `SDL_CreateRenderer` incompatibility. The existing window creation code (`sdl3_window.c`) uses `SDL_GetWindowSurface` to commit an initial buffer for Wayland visibility. SDL3 documents that you "may not combine [window surface] with 3D or the rendering API on this window." The fix (verified via SDL issue #10133, merged June 2024, available since SDL 3.2.0) is to call `SDL_DestroyWindowSurface(window)` before `SDL_CreateRenderer(window, NULL)`. This cleanly transitions from surface-based to renderer-based display. Once a renderer exists, `SDL_RenderPresent` handles Wayland buffer commits automatically.

**Primary recommendation:** Create an `SDL3_Renderer` component holding `SDL_Renderer*` and `SDL_Color clear_color`. Add two new systems: `SDL3_RenderClearSystem` (PreRender phase) clears each window, `SDL3_RenderPresentSystem` (PostRender phase) presents each window. Create the renderer in `SDL3_WindowStateSystem` when the window reaches READY state, and destroy it during CLOSING->CLOSED before `SDL_DestroyWindow`. Call `SDL_DestroyWindowSurface` before `SDL_CreateRenderer` to resolve the surface/renderer mutual exclusion.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| SDL3 | 3.4.2 | SDL_Renderer, SDL_RenderClear, SDL_RenderPresent, SDL_SetRenderDrawColor | Already in project, provides the entire 2D rendering API |

No additional libraries needed. All renderer functionality is built into SDL3 core.

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| (none) | - | - | Renderer core is pure SDL3 + CELS ECS |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `SDL_CreateRenderer(window, NULL)` | `SDL_CreateWindowAndRenderer(...)` | Convenience function hides configuration, harder to manage in ECS where window/renderer have separate lifecycles |
| Separate SDL3_Renderer component | Embed SDL_Renderer* in SDL3_WindowComponent | Separate component follows ECS convention, enables querying renderer independently, cleaner destruction ordering |
| Built-in PreRender/PostRender phases | Custom CEL_Phase for clear/present | Unnecessary -- built-in phases already provide the exact ordering needed |

## Architecture Patterns

### Recommended Project Structure
```
src/
  renderer/
    sdl3_renderer.c       # NEW: renderer create/destroy, called from sdl3_module.c
  window/
    sdl3_window.c         # EXISTING: unchanged
  loop/
    sdl3_loop.c           # EXISTING: unchanged
  input/
    sdl3_input.c          # EXISTING: unchanged
  sdl3_module.c           # MODIFIED: add SDL3_Renderer component, RenderClearSystem, RenderPresentSystem
  sdl3_internal.h         # MODIFIED: add renderer function declarations
include/
  cels_sdl3.h            # MODIFIED: add SDL3_Renderer component type
examples/
  renderer/
    main.c                # NEW: renderer example with interactive clear color
```

### Pattern 1: Renderer as Separate Component Paired with Window Entity
**What:** Each window entity gets an `SDL3_Renderer` component alongside its existing `SDL3_WindowComponent`. The renderer component stores the `SDL_Renderer*` pointer and per-window clear color.
**When to use:** Always -- every window entity that reaches READY state gets a renderer.
**Example:**
```c
// Source: verified against SDL3 wiki + cels_sdl3.h patterns
CEL_Component(SDL3_Renderer) {
    SDL_Renderer* renderer;
    SDL_Color     clear_color;  // {r, g, b, a} -- Uint8 each
};
```

### Pattern 2: Render Cycle via Built-in Pipeline Phases
**What:** Two module systems bracket the render frame. `SDL3_RenderClearSystem` runs at `PreRender` and clears each window's renderer to its clear color. `SDL3_RenderPresentSystem` runs at `PostRender` and presents each window's renderer. Developer draw systems run at `OnRender` in between.
**When to use:** Always -- this is the render cycle architecture.
**Example:**
```c
// Source: CELS system phase model (cels_system_impl.h lines 605-609)
// PreRender -> OnRender -> PostRender
CEL_System(SDL3_RenderClearSystem, .phase = PreRender) {
    cel_query(SDL3_WindowComponent, SDL3_Renderer);
    cel_each(SDL3_WindowComponent, SDL3_Renderer) {
        if (SDL3_WindowComponent->state != SDL3_WINDOW_READY) continue;
        cel_update(SDL3_Renderer) {
            SDL_SetRenderDrawColor(SDL3_Renderer->renderer,
                SDL3_Renderer->clear_color.r,
                SDL3_Renderer->clear_color.g,
                SDL3_Renderer->clear_color.b,
                SDL3_Renderer->clear_color.a);
            SDL_RenderClear(SDL3_Renderer->renderer);
        }
    }
}

CEL_System(SDL3_RenderPresentSystem, .phase = PostRender) {
    cel_query(SDL3_WindowComponent, SDL3_Renderer);
    cel_each(SDL3_WindowComponent, SDL3_Renderer) {
        if (SDL3_WindowComponent->state != SDL3_WINDOW_READY) continue;
        // Present is read-only from the component's perspective
        SDL_RenderPresent(SDL3_Renderer->renderer);
    }
}
```

### Pattern 3: Renderer Lifecycle Tied to Window State Machine
**What:** The renderer is created when a window enters READY state and destroyed during CLOSING->CLOSED transition. This uses the existing `SDL3_WindowStateSystem` (OnLoad phase) for destruction and a new renderer-creation system or integration point for creation.
**When to use:** Always -- renderer lifecycle is fully automatic.
**Example:**
```c
// Renderer creation -- called from sdl3_module.c, implementation in sdl3_renderer.c
// Must call SDL_DestroyWindowSurface first due to surface/renderer mutual exclusion
void sdl3_renderer_create(SDL_Window* window, SDL_Renderer** out_renderer) {
    SDL_DestroyWindowSurface(window);  // clean up Wayland surface hack
    *out_renderer = SDL_CreateRenderer(window, NULL);
}

// Renderer destruction -- must happen BEFORE SDL_DestroyWindow
void sdl3_renderer_destroy(SDL_Renderer* renderer) {
    if (renderer) {
        SDL_DestroyRenderer(renderer);  // also frees all associated textures
    }
}
```

### Pattern 4: Window Table Extension for Renderer (Cross-TU Access)
**What:** The existing `SDL3_WindowTable` pattern (used by InputSystem to pass mutable component pointers to cross-TU functions) extends to include `SDL3_Renderer*` pointers. This enables the `SDL3_WindowStateSystem` or a new system to create/destroy renderers via helper functions in `sdl3_renderer.c`.
**When to use:** When renderer create/destroy functions in `sdl3_renderer.c` need to set component data on entities but cannot use `cel_query`/`cel_update` due to per-TU static ID constraint.
**Example:**
```c
// sdl3_module.c passes entity + component_id to renderer creation,
// same pattern as sdl3_window_create
sdl3_renderer_create(entity, window_comp->window,
                     SDL3_Renderer_id);
```

### Anti-Patterns to Avoid
- **Global renderer pointer:** `static SDL_Renderer* g_renderer` -- breaks multi-window. Renderer MUST be per-entity.
- **Creating renderer in window on_create observer:** The window observer already does surface commit for Wayland. Renderer creation should happen separately, after READY state, to keep concerns separate.
- **Calling SDL_RenderPresent inside clear system:** Clear and present MUST be separate systems at different phases so developer draw code slots in between.
- **Skipping SDL_DestroyWindowSurface:** Will cause "Renderer already associated with window" error on SDL 3.4.2.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Render pipeline ordering | Custom callback registration / function pointer arrays | CELS built-in phases (PreRender, OnRender, PostRender) | Framework already provides this -- using it ensures correct ordering with zero custom infrastructure |
| Window-renderer pairing | Manual bookkeeping / hash maps | ECS component on same entity | Entity is the natural join key -- query (WindowComponent, Renderer) gets paired data automatically |
| Renderer driver selection | Explicit driver string ("opengl", "vulkan") | `SDL_CreateRenderer(window, NULL)` | NULL lets SDL pick the optimal driver for the platform; explicit selection breaks portability |
| Surface-to-renderer transition | Manual state tracking for "has surface" vs "has renderer" | `SDL_DestroyWindowSurface` + `SDL_CreateRenderer` | SDL3 handles cleanup internally since issue #10133 fix |

**Key insight:** The CELS framework's built-in pipeline phases (PreRender/OnRender/PostRender) eliminate the need for any custom render pipeline infrastructure. The clear and present systems simply declare their phase, and CELS handles ordering.

## Common Pitfalls

### Pitfall 1: SDL_GetWindowSurface and SDL_CreateRenderer Mutual Exclusion
**What goes wrong:** Calling `SDL_CreateRenderer` on a window that already has a surface from `SDL_GetWindowSurface` fails with "Renderer already associated with window" error.
**Why it happens:** The existing `sdl3_window_create` calls `SDL_GetWindowSurface` to commit an initial buffer for Wayland visibility. SDL3 docs state: "You may not combine this with 3D or the rendering API on this window."
**How to avoid:** Call `SDL_DestroyWindowSurface(window)` before `SDL_CreateRenderer(window, NULL)`. This is safe on SDL 3.2.0+ (fix merged in issue #10133, June 2024). Our pinned SDL 3.4.2 includes this fix.
**Warning signs:** `SDL_CreateRenderer` returns NULL; `SDL_GetError()` mentions "renderer already associated."

### Pitfall 2: Renderer Destruction Order
**What goes wrong:** Destroying `SDL_Window` before `SDL_Renderer` causes use-after-free. Renderer references the window's internal surface/context.
**Why it happens:** SDL docs for `SDL_DestroyRenderer` state: "This should be called before destroying the associated window."
**How to avoid:** In the CLOSING->CLOSED transition, destroy renderer first, then window. The existing `SDL3_WindowStateSystem` must be modified to destroy the renderer component before calling `sdl3_window_destroy`.
**Warning signs:** Segfault or GPU driver error on window close.

### Pitfall 3: Rendering to Non-READY Windows
**What goes wrong:** Calling `SDL_RenderClear`/`SDL_RenderPresent` on a renderer whose window is MINIMIZED, CLOSING, or CLOSED causes undefined behavior or wasted GPU work.
**Why it happens:** Minimized windows have no visible surface. Closed windows have destroyed resources.
**How to avoid:** Guard render systems with `if (window->state != SDL3_WINDOW_READY) continue;`. This matches the existing frame loop pause behavior for minimized windows. Also skip RESIZING state windows -- they are mid-resize and rendering is fine but the guard on READY covers all active states. Actually, RESIZING windows should still render (they are visible), so the guard should check for states that definitely should NOT render: MINIMIZED, CLOSING, CLOSED.
**Warning signs:** Blank frames, GPU errors on minimize, or crash on close.

### Pitfall 4: Forgetting to Register Renderer Component Before Observer Fires
**What goes wrong:** If renderer creation happens in an observer or early system and the `SDL3_Renderer` component ID hasn't been initialized yet, `cels_entity_set_component` uses an uninitialized ID (0).
**Why it happens:** Same as the `SDL3_EventQueue` issue from Phase 4 -- `CEL_Component _register()` is a no-op; actual ID assignment requires `cels_ensure_component`.
**How to avoid:** Call `cels_ensure_component` for `SDL3_Renderer` in `CEL_Module(SDL3_Engine, init)`, same pattern as `SDL3_EventQueue`.
**Warning signs:** Renderer component silently not attached, or crash in component lookup.

### Pitfall 5: cel_update Scope for RenderPresent
**What goes wrong:** Wrapping `SDL_RenderPresent` in `cel_update(SDL3_Renderer)` when it doesn't modify the component data. This triggers unnecessary ECS change detection / dirty flags.
**Why it happens:** Reflexive use of `cel_update` because you're "doing something" with the renderer.
**How to avoid:** `SDL_RenderPresent` reads the `SDL_Renderer*` but doesn't modify the `SDL3_Renderer` component struct. Access the renderer pointer through the read-only iteration variable. Only use `cel_update` when actually modifying component fields (like in the clear system where you call SDL functions through the renderer pointer -- but even there, you're only reading `clear_color` and passing `renderer` to SDL).
**Warning signs:** Excessive recomposition or change notifications for renderer component.

## Code Examples

### SDL3_Renderer Component Definition
```c
// Source: SDL3 wiki SDL_Color definition + cels_sdl3.h component patterns
CEL_Component(SDL3_Renderer) {
    SDL_Renderer* renderer;
    SDL_Color     clear_color;  // RGBA, Uint8 each, default: cornflower blue
};
```

### Renderer Creation (in sdl3_renderer.c)
```c
// Source: SDL3 wiki SDL_CreateRenderer, SDL_DestroyWindowSurface
// Called from sdl3_module.c, receives explicit entity + component_id
void sdl3_renderer_create(cels_entity_t entity,
                           SDL_Window* window,
                           cels_entity_t component_id)
{
    // Must destroy window surface first -- surface and renderer are mutually exclusive
    // (SDL_GetWindowSurface was used for Wayland initial buffer commit in sdl3_window.c)
    SDL_DestroyWindowSurface(window);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("SDL3: SDL_CreateRenderer failed: %s", SDL_GetError());
        return;
    }

    SDL3_Renderer comp = {
        .renderer    = renderer,
        .clear_color = { .r = 100, .g = 149, .b = 237, .a = 255 }  // cornflower blue
    };
    cels_entity_set_component(entity, component_id, &comp, sizeof(comp));
}
```

### Renderer Destruction (in sdl3_renderer.c)
```c
// Source: SDL3 wiki SDL_DestroyRenderer
void sdl3_renderer_destroy(SDL_Renderer* renderer)
{
    if (!renderer) return;
    SDL_DestroyRenderer(renderer);  // also frees all associated textures
}
```

### Complete Render Cycle (in sdl3_module.c)
```c
// Source: CELS phase model, SDL3 wiki render functions
CEL_System(SDL3_RenderClearSystem, .phase = PreRender) {
    cel_query(SDL3_WindowComponent, SDL3_Renderer);
    cel_each(SDL3_WindowComponent, SDL3_Renderer) {
        // Skip windows not actively rendering
        if (SDL3_WindowComponent->state == SDL3_WINDOW_MINIMIZED ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSED) continue;

        SDL_SetRenderDrawColor(SDL3_Renderer->renderer,
            SDL3_Renderer->clear_color.r,
            SDL3_Renderer->clear_color.g,
            SDL3_Renderer->clear_color.b,
            SDL3_Renderer->clear_color.a);
        SDL_RenderClear(SDL3_Renderer->renderer);
    }
}

CEL_System(SDL3_RenderPresentSystem, .phase = PostRender) {
    cel_query(SDL3_WindowComponent, SDL3_Renderer);
    cel_each(SDL3_WindowComponent, SDL3_Renderer) {
        if (SDL3_WindowComponent->state == SDL3_WINDOW_MINIMIZED ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING ||
            SDL3_WindowComponent->state == SDL3_WINDOW_CLOSED) continue;

        SDL_RenderPresent(SDL3_Renderer->renderer);
    }
}
```

### Module Registration Update
```c
// Source: existing sdl3_module.c patterns
CEL_Module(SDL3_Engine, init) {
    // ... existing registrations ...
    cels_register(SDL3_Renderer, SDL3_RenderClearSystem, SDL3_RenderPresentSystem);

    // Eagerly initialize SDL3_Renderer_id for systems that set component data
    cels_ensure_component(&SDL3_Renderer_id, "SDL3_Renderer",
                          sizeof(SDL3_Renderer), CELS_ALIGNOF(SDL3_Renderer));
}
```

### Example App: Interactive Clear Color
```c
// Source: derived from existing input example pattern
CEL_System(ColorChanger, .phase = OnUpdate) {
    cel_query(SDL3_EventQueue, SDL3_Renderer);
    cel_each(SDL3_EventQueue, SDL3_Renderer) {
        for (int i = 0; i < SDL3_EventQueue->count; i++) {
            const SDL_Event* ev = &SDL3_EventQueue->events[i];
            if (ev->type == SDL_EVENT_KEY_DOWN) {
                cel_update(SDL3_Renderer) {
                    switch (ev->key.key) {
                    case SDLK_R:
                        SDL3_Renderer->clear_color = (SDL_Color){255, 0, 0, 255};
                        break;
                    case SDLK_G:
                        SDL3_Renderer->clear_color = (SDL_Color){0, 255, 0, 255};
                        break;
                    case SDLK_B:
                        SDL3_Renderer->clear_color = (SDL_Color){0, 0, 255, 255};
                        break;
                    case SDLK_C:
                        SDL3_Renderer->clear_color = (SDL_Color){100, 149, 237, 255};
                        break;
                    }
                }
            }
        }
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `SDL_CreateRenderer(window, -1, flags)` (SDL2) | `SDL_CreateRenderer(window, NULL)` (SDL3) | SDL3 release | Driver selection by name string or NULL, not integer index |
| `SDL_SetRenderDrawColor` returns `int` (0=ok) | Returns `bool` (true=ok) | SDL3 release | Check with `if (!SDL_SetRenderDrawColor(...))` |
| `SDL_RenderClear` returns `int` | Returns `bool` | SDL3 release | Same bool return pattern |
| `SDL_RenderPresent` returns `void` (SDL2) | Returns `bool` (SDL3) | SDL3 release | Can now detect present failure |
| Surface + renderer mixed on same window (undefined in SDL2) | Explicitly prohibited, `SDL_DestroyWindowSurface` available | SDL 3.2.0 | Must call `SDL_DestroyWindowSurface` before creating renderer if surface was used |

**Deprecated/outdated:**
- `SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED)`: The second parameter is now a driver name string (or NULL), and renderer flags have been removed. SDL3 always creates an accelerated renderer when possible.
- `SDL_RENDERER_PRESENTVSYNC` flag: VSync is now configured through renderer properties, not creation flags.

## Open Questions

1. **Should SDL_RenderClear call be inside cel_update?**
   - What we know: `SDL_SetRenderDrawColor` + `SDL_RenderClear` read the component's `clear_color` and `renderer` fields but don't modify the `SDL3_Renderer` struct itself. The SDL calls mutate SDL-internal state (backbuffer), not the ECS component.
   - What's unclear: Whether calling SDL functions through a pointer obtained via const iteration counts as "read-only" in CELS. The `renderer` pointer itself is not being modified, but the thing it points to (SDL internal backbuffer) is being modified.
   - Recommendation: Do NOT use `cel_update` for the render clear/present systems. The ECS component struct fields are not being modified. SDL-internal state is outside ECS tracking. Using `cel_update` would trigger unnecessary change notifications.

2. **Renderer creation timing: system vs observer**
   - What we know: Context says "renderer created automatically when window reaches READY state." The window reaches READY state synchronously during `sdl3_window_create` (called from `on_create` observer). A new system could check for windows in READY state that lack a renderer and create one.
   - What's unclear: Whether to create the renderer in the window's `on_create` observer (immediate, same frame) or in a separate system that runs next frame.
   - Recommendation: Create renderer in the `on_create` observer, immediately after `sdl3_window_create` returns and the window is in READY state. This ensures the renderer exists on the very first frame and avoids a one-frame delay where the window is visible but not rendering. The observer already has access to the entity and can call `sdl3_renderer_create(entity, window, SDL3_Renderer_id)`.

3. **Existing examples (minimal, frame-loop) that don't use rendering**
   - What we know: These examples currently work because `SDL_GetWindowSurface` provides the Wayland buffer commit. Once renderer creation is automatic for all windows, these examples will also get renderers. This is fine -- they just render cornflower blue and continue working.
   - What's unclear: Whether the surface commit in `sdl3_window_create` should be removed entirely or kept for the brief moment between window creation and renderer creation (same observer call, very short gap).
   - Recommendation: Keep the surface commit in `sdl3_window_create` for now. The renderer creation (in the same observer, immediately after) calls `SDL_DestroyWindowSurface` first, then `SDL_CreateRenderer`. This ensures no gap where Wayland has no committed buffer.

## Sources

### Primary (HIGH confidence)
- [SDL3 wiki: SDL_CreateRenderer](https://wiki.libsdl.org/SDL3/SDL_CreateRenderer) - function signature, parameters, thread safety
- [SDL3 wiki: SDL_RenderClear](https://wiki.libsdl.org/SDL3/SDL_RenderClear) - clear function, draw color relationship
- [SDL3 wiki: SDL_SetRenderDrawColor](https://wiki.libsdl.org/SDL3/SDL_SetRenderDrawColor) - color API (Uint8 r/g/b/a)
- [SDL3 wiki: SDL_RenderPresent](https://wiki.libsdl.org/SDL3/SDL_RenderPresent) - backbuffer present, threading
- [SDL3 wiki: SDL_DestroyRenderer](https://wiki.libsdl.org/SDL3/SDL_DestroyRenderer) - "call before destroying associated window", frees textures
- [SDL3 wiki: SDL_DestroyWindowSurface](https://wiki.libsdl.org/SDL3/SDL_DestroyWindowSurface) - cleanup surface before renderer
- [SDL3 wiki: SDL_GetWindowSurface](https://wiki.libsdl.org/SDL3/SDL_GetWindowSurface) - "may not combine with 3D or rendering API"
- [SDL3 wiki: SDL_Color](https://wiki.libsdl.org/SDL3/SDL_Color) - struct definition { Uint8 r, g, b, a }
- CELS cels_system_impl.h (local) - pipeline phases: OnLoad, OnUpdate, PreRender, OnRender, PostRender
- CELS cels_runtime.h (local) - cels_phase_props_t, cels_entity_set_component
- Existing cels-sdl3 codebase (local) - component patterns, system patterns, per-TU constraint handling

### Secondary (MEDIUM confidence)
- [SDL GitHub issue #10133](https://github.com/libsdl-org/SDL/issues/10133) - SDL_DestroyWindowSurface fix for renderer creation, merged June 2024
- [SDL GitHub issue #7699](https://github.com/libsdl-org/SDL/issues/7699) - Wayland window visibility, SDL_RenderPresent handles buffer commit

### Tertiary (LOW confidence)
- (none -- all findings verified against primary sources)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - SDL3 renderer API is well-documented, functions verified via official wiki
- Architecture: HIGH - CELS pipeline phases verified from framework source; component pattern follows established conventions from phases 2-4
- Pitfalls: HIGH - Surface/renderer mutual exclusion verified via SDL wiki + GitHub issue; destruction ordering verified via SDL_DestroyRenderer docs

**Research date:** 2026-03-21
**Valid until:** 2026-04-21 (stable APIs, 30-day validity)
