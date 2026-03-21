# Phase 9: Module Integration - Research

**Researched:** 2026-03-21
**Domain:** CELS module registration, a la carte provider bundling, SDL3 resource cleanup ordering, error callback design
**Confidence:** HIGH

## Summary

Phase 9 restructures the existing monolithic `CEL_Module(SDL3_Engine, init)` into a composable registration system. Currently, all CELS declarations (components, states, lifecycles, systems) live in one `CEL_Module(SDL3_Engine, init)` body in `sdl3_module.c`. Phase 9 introduces ~7 individual `_use()` functions (SDL3_Init_use, SDL3_Window_use, SDL3_FrameLoop_use, SDL3_Input_use, SDL3_Renderer_use, SDL3_Textures_use, SDL3_Text_use) plus an all-in-one `SDL3_use(&config)`. Each `_use()` function registers only its own CELS declarations. No auto-dependency pulling -- the developer explicitly calls what they need.

The critical constraint is the **per-TU static ID requirement**: all CELS declarations (CEL_Component, CEL_State, CEL_System, CEL_Lifecycle) generate static per-TU ID variables, and `cels_register` only initializes THIS TU's copies. This means ALL `_use()` functions must live in the same translation unit (sdl3_module.c), not in separate .c files. The `_use()` functions are simply C functions that call subsets of `cels_register(...)` and `cels_ensure_component(...)`. They are NOT separate CELS modules -- they partition the registration within the single SDL3_Engine module.

Error reporting is a separate concern from CELS's `CEL_OnError` (which handles framework-level errors like component conflicts). The SDL3 error callback is a simple function pointer: `void (*SDL3_ErrorCallback)(const char* context, const char* message)`. A global static holds the callback. When no callback is registered, errors go to `SDL_Log` (which writes to stderr). The callback is fire-and-forget -- the module decides recovery, the callback is informational only.

**Primary recommendation:** Keep all `_use()` functions as simple C functions within `sdl3_module.c` that partition the existing `cels_register(...)` calls. The `CEL_Module(SDL3_Engine, init)` body becomes empty (or minimal) and the developer calls `SDL3_use(&config)` which internally calls all individual `_use()` functions. Individual `_use()` functions take zero arguments.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| CELS | v0.5.3 | ECS framework -- CEL_Module, cels_register, CEL_System, CEL_Component | Already in project, provides the module/registration infrastructure |
| SDL3 | 3.4.2 | SDL_GetError for error surfacing | Already in project |
| SDL3_image | 3.4.0 | Texture loading (auto-init, no explicit init/quit) | Already in project |
| SDL3_ttf | 3.2.2 | TTF_Init/TTF_Quit lifecycle | Already in project |

No new libraries needed. Phase 9 is purely an organizational refactor of existing code.

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| (none) | - | - | No new dependencies |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Partition registration via plain C functions | Separate `CEL_Module` per provider | Cannot work -- per-TU static ID constraint means all declarations must be in sdl3_module.c |
| Global error callback | CELS `CEL_OnError` | `CEL_OnError` is for CELS framework errors (abort after handler), not for SDL errors (informational, fire-and-forget) |
| Config struct on `SDL3_use()` | Config on each `_use()` function | Context says zero-arg individual _use() for simplicity -- config only on the all-in-one |
| Hardcoded cleanup order | Dependency graph resolution | Over-engineered for ~5 resource types; hardcoded order is clear, verifiable, and matches SDL3 requirements |

## Architecture Patterns

### Recommended Project Structure
```
src/
  sdl3_module.c           # ALL _use() functions + CEL_Module(SDL3_Engine) + all systems
  sdl3_internal.h         # Error callback declaration, cleanup helpers
  sdl3_init.c             # Init/shutdown + error callback storage
  loop/sdl3_loop.c        # Frame loop state
  window/sdl3_window.c    # Window create/destroy
  input/sdl3_input.c      # Event drain
  renderer/sdl3_renderer.c    # Renderer create/destroy
  renderer/sdl3_draw.c        # Draw buffer (Phase 6)
  texture/sdl3_texture.c      # Texture cache (Phase 7)
  text/sdl3_text.c            # Font + text helpers (Phase 8)
include/
  cels_sdl3.h             # PUBLIC: SDL3_use(), individual _use() declarations, config struct, error callback type
```

### Pattern 1: Registration Partitioning Within a Single Module
**What:** The `CEL_Module(SDL3_Engine, init)` body is minimal -- it only calls `cels_module_provides("SDL3")` or similar metadata. Each `_use()` function is a plain C function in the same TU that calls `cels_register(...)` and `cels_ensure_component(...)` for its subset of declarations.
**When to use:** Always -- this is the only architecture that satisfies both a la carte registration AND the per-TU static ID constraint.
**Example:**
```c
// In sdl3_module.c (same TU as all CEL_System/CEL_Component declarations)

// Guard flags prevent double-registration
static bool s_init_registered = false;
static bool s_window_registered = false;
// ... etc.

void SDL3_Init_use(void) {
    if (s_init_registered) return;
    s_init_registered = true;
    cels_register(SDL3_ContextState, SDL3_ContextLC, SDL3_ContextConfig);
}

void SDL3_Window_use(void) {
    if (s_window_registered) return;
    s_window_registered = true;
    cels_register(SDL3_WindowConfig, SDL3_WindowComponent, SDL3_EventQueue,
                  SDL3_Renderer, SDL3_WindowLC, SDL3_WindowStateSystem);
    cels_ensure_component(&SDL3_EventQueue_id, "SDL3_EventQueue",
                          sizeof(SDL3_EventQueue), CELS_ALIGNOF(SDL3_EventQueue));
    cels_ensure_component(&SDL3_Renderer_id, "SDL3_Renderer",
                          sizeof(SDL3_Renderer), CELS_ALIGNOF(SDL3_Renderer));
}

void SDL3_FrameLoop_use(void) {
    if (s_frameloop_registered) return;
    s_frameloop_registered = true;
    cels_register(SDL3_FrameState, SDL3_FrameStateSystem);
}

void SDL3_Input_use(void) {
    if (s_input_registered) return;
    s_input_registered = true;
    cels_register(SDL3_InputSystem);
}

void SDL3_Renderer_use(void) {
    if (s_renderer_registered) return;
    s_renderer_registered = true;
    cels_register(SDL3_RenderClearSystem, SDL3_RenderPresentSystem);
}
```

**Key detail:** The guard flag pattern prevents double-registration when both individual `_use()` and `SDL3_use()` are called, or when `SDL3_use()` is called multiple times.

### Pattern 2: All-in-One Registration with Config Struct
**What:** `SDL3_use(const SDL3_Config* config)` calls all individual `_use()` functions and stores config. Passing NULL uses defaults.
**When to use:** When the developer wants everything registered in one call.
**Example:**
```c
// In include/cels_sdl3.h
typedef void (*SDL3_ErrorCallback)(const char* context, const char* message);

typedef struct SDL3_Config {
    SDL3_ErrorCallback  on_error;       // NULL = default stderr handler
    Uint32              sdl_init_flags; // 0 = SDL_INIT_VIDEO (default)
} SDL3_Config;

// In sdl3_module.c
void SDL3_use(const SDL3_Config* config) {
    // Store config (error callback, etc.)
    if (config) {
        sdl3_set_error_callback(config->on_error);
        // Store init flags for deferred use
        if (config->sdl_init_flags) {
            sdl3_set_init_flags(config->sdl_init_flags);
        }
    }

    // Register all providers
    SDL3_Init_use();
    SDL3_Window_use();
    SDL3_FrameLoop_use();
    SDL3_Input_use();
    SDL3_Renderer_use();
    SDL3_Textures_use();
    SDL3_Text_use();
}
```

### Pattern 3: Error Callback -- Simple Function Pointer
**What:** A static function pointer in sdl3_init.c. When set, SDL errors are reported through it. When NULL, a default handler prints via `SDL_Log`.
**When to use:** Always -- all SDL error sites call through this pattern.
**Example:**
```c
// In sdl3_init.c
static SDL3_ErrorCallback s_error_callback = NULL;

static void sdl3_default_error_handler(const char* context, const char* message) {
    SDL_Log("SDL3 error [%s]: %s", context, message);
}

void sdl3_set_error_callback(SDL3_ErrorCallback callback) {
    s_error_callback = callback;
}

void sdl3_report_error(const char* context) {
    const char* msg = SDL_GetError();
    if (s_error_callback) {
        s_error_callback(context, msg);
    } else {
        sdl3_default_error_handler(context, msg);
    }
}
```

**Error call sites (replace existing `SDL_Log("SDL3: ... failed: %s", SDL_GetError())`):**
- `sdl3_init()` -- SDL_Init, TTF_Init failures
- `sdl3_window_create()` -- SDL_CreateWindow failure
- `sdl3_renderer_create()` -- SDL_CreateRenderer failure
- `sdl3_texture_cache_load()` -- IMG_LoadTexture failure (Phase 7)
- `sdl3_font_load()` -- TTF_OpenFont failure (Phase 8)
- `sdl3_text_create()` -- TTF_CreateText failure (Phase 8)

### Pattern 4: Hardcoded Reverse-Dependency Cleanup Order
**What:** Per-window cleanup follows a fixed order: text handles -> text engine -> texture cache -> draw buffer -> renderer -> window. Global cleanup: fonts -> TTF_Quit -> SDL_Quit.
**When to use:** During CLOSING->CLOSED transition (per-window) and during sdl3_shutdown() (global).
**Example:**
```c
// In SDL3_WindowStateSystem CLOSING block (sdl3_module.c):
// Per-window cleanup order:
if (SDL3_WindowComponent->state == SDL3_WINDOW_CLOSING) {
    // 1. Destroy TTF_Text handles (Phase 8) -- before text engine
    // 2. Destroy TTF_TextEngine (Phase 8) -- before renderer
    // 3. Invalidate texture cache (Phase 7) -- before renderer
    // 4. Destroy draw buffer (Phase 6) -- before renderer
    // 5. Destroy SDL_Renderer -- before window
    // 6. Destroy SDL_Window

    // Each step calls sdl3_report_error on failure and continues (best-effort)
}

// In sdl3_shutdown() (sdl3_init.c):
// Global cleanup order:
// 1. sdl3_fonts_close_all() -- before TTF_Quit
// 2. TTF_Quit() -- before SDL_Quit
// 3. SDL_Quit()
```

### Pattern 5: Best-Effort Cleanup with Error Reporting
**What:** During cleanup, if one resource fails to destroy, report the error via callback and continue destroying remaining resources. Never abort cleanup.
**When to use:** All cleanup paths.
**Example:**
```c
// Each cleanup step wraps in error check
if (SDL3_Renderer->renderer) {
    sdl3_texture_cache_remove_renderer(SDL3_Renderer->renderer);
    // ^ if this fails internally, it logs and continues

    sdl3_draw_buffer_destroy(SDL3_Renderer);
    // ^ if this fails internally, it logs and continues

    sdl3_renderer_destroy(SDL3_Renderer->renderer);
    // ^ SDL_DestroyRenderer -- check if it returned false
    //   if false, call sdl3_report_error("destroy_renderer")
    //   continue regardless

    cel_update(SDL3_Renderer) {
        SDL3_Renderer->renderer = NULL;
    }
}
```

### Anti-Patterns to Avoid
- **Separate CEL_Module per provider:** Cannot work due to per-TU static ID constraint. All CELS declarations must be in sdl3_module.c.
- **Auto-dependency pulling:** `SDL3_Window_use()` must NOT call `SDL3_Init_use()`. Per CONTEXT.md: "Explicit registration only."
- **Late/dynamic registration:** All `_use()` calls must happen before the first `cels_step()`. Per CONTEXT.md: "All registration must happen before the first frame."
- **Throwing on cleanup failure:** Best-effort cleanup means log and continue, never abort.
- **Using CEL_OnError for SDL errors:** CEL_OnError is for framework errors and calls abort() after handler. SDL error callback is informational only.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Dependency resolution | Auto-dependency graph for provider ordering | Hardcoded order in SDL3_use() | Only ~7 providers with known fixed dependencies -- graph resolution is overkill |
| Cleanup ordering | Generic topological sort for destruction order | Hardcoded reverse-dependency order | SDL3 destruction requirements are well-defined and fixed |
| Error string formatting | Custom error message builder | SDL_GetError() + context string | SDL already provides detailed error messages |
| Module system | Custom module registry with init/shutdown hooks | cels_register() partition + static guard flags | CELS already provides module registration; we just partition calls |
| Leak detection | Custom resource tracking | Valgrind, ASan (external tools) | Per CONTEXT.md: "developers use external tools" |
| Pre-shutdown hooks | Custom hook registration system | Developer writes own systems | Per CONTEXT.md: "No pre-shutdown hooks" |

**Key insight:** Phase 9 is an organizational refactor, not new functionality. The `_use()` functions are thin wrappers around existing `cels_register(...)` calls. The error callback is a simple function pointer. The cleanup order is already mostly correct in the existing code -- Phase 9 just makes it systematic and adds error reporting to each step.

## Common Pitfalls

### Pitfall 1: Per-TU Static ID Constraint Violation
**What goes wrong:** Putting `_use()` functions in separate .c files causes `cels_register(SDL3_InputSystem)` to initialize a different TU's copy of `SDL3_InputSystem_id`, leaving sdl3_module.c's copy at 0.
**Why it happens:** Each `CEL_System(Name, ...)` creates a `static cels_entity_t Name_id` in the TU where it appears. `cels_register(Name)` calls `Name_register()` which initializes THAT TU's `Name_id`.
**How to avoid:** ALL `_use()` functions MUST live in `sdl3_module.c` (same TU as all CELS declarations). They are plain C functions, not CELS modules.
**Warning signs:** Systems not running, components not found, zero entity IDs in runtime lookups.

### Pitfall 2: Double Registration
**What goes wrong:** Developer calls both `SDL3_use(NULL)` and `SDL3_Window_use()` -- without guards, components get registered twice, potentially causing CELS errors.
**Why it happens:** `SDL3_use()` calls all individual `_use()` functions. If developer also calls individual ones, components register twice.
**How to avoid:** Static boolean guard flag in each `_use()` function: `if (s_window_registered) return;`. The `cels_register` call itself may be idempotent (CEL_Module checks `if (Name != 0) return`), but the `cels_ensure_component` calls and any init logic should not run twice.
**Warning signs:** Duplicate registration warnings from CELS, or silent no-op if guards work.

### Pitfall 3: Registration Order Affecting System Execution
**What goes wrong:** If `SDL3_Input_use()` is called AFTER `SDL3_Window_use()`, the InputSystem might register after WindowStateSystem, changing execution order within the OnLoad phase (since CELS runs systems in registration order within the same phase).
**Why it happens:** CELS system execution order within a phase is determined by registration order.
**How to avoid:** Document the required call order for individual `_use()` functions. In `SDL3_use()`, call them in the correct order: Init -> FrameLoop -> Input -> Window -> Renderer -> Textures -> Text. The order must match the current registration order in `CEL_Module(SDL3_Engine, init)`.
**Warning signs:** Events processed after window state checks, textures loaded before input is drained, render clear happening before draw flush.

### Pitfall 4: CEL_Module(SDL3_Engine, init) vs _use() Functions Conflict
**What goes wrong:** If `CEL_Module(SDL3_Engine, init)` still contains `cels_register(...)` calls AND `_use()` functions also call them, registration happens twice.
**Why it happens:** The existing module init body has all the registrations. Refactoring to `_use()` functions means the module init body must be emptied.
**How to avoid:** `CEL_Module(SDL3_Engine, init)` body should ONLY call `SDL3_use(NULL)` (or be empty if the developer calls `SDL3_use()` explicitly). The module's `_init` function is triggered by `cels_register(SDL3_Engine)`. So: `cels_register(SDL3_Engine)` -> `SDL3_Engine_init()` -> `SDL3_Engine_init_body()` which calls `SDL3_use(NULL)`. This means `cels_register(SDL3_Engine)` is the all-in-one shorthand.
**Warning signs:** Systems running twice, double initialization.

### Pitfall 5: Existing cels_register(SDL3_Engine) in Examples
**What goes wrong:** All existing examples use `cels_register(SDL3_Engine)`. This pattern must continue to work after refactoring.
**Why it happens:** Backward compatibility -- existing examples are the de facto API.
**How to avoid:** `CEL_Module(SDL3_Engine, init)` body calls `SDL3_use(NULL)` which registers everything with default config. This preserves the existing `cels_register(SDL3_Engine)` pattern. For custom config, developer uses `SDL3_use(&config)` instead.
**Warning signs:** Existing examples fail to compile or behave differently after Phase 9.

### Pitfall 6: Cleanup of Partially Registered Providers
**What goes wrong:** Developer registers SDL3_Init_use() and SDL3_Window_use() but NOT SDL3_Textures_use(). During cleanup, trying to clean up texture caches that were never initialized.
**Why it happens:** A la carte registration means not all providers are present.
**How to avoid:** Cleanup code uses the same guard flags to skip cleanup for unregistered providers. Or better: cleanup code checks for NULL pointers and zero IDs (the existing pattern already does this -- e.g., `if (SDL3_Renderer->renderer)` before destroy). The system-based cleanup (WindowStateSystem) already only processes entities that exist.
**Warning signs:** Segfault on cleanup when only partial providers registered.

## Code Examples

### SDL3_Config Struct Definition
```c
// Source: CONTEXT.md decisions + existing SDL_Log pattern
typedef void (*SDL3_ErrorCallback)(const char* context, const char* message);

typedef struct SDL3_Config {
    SDL3_ErrorCallback  on_error;       /* NULL = default stderr handler */
    Uint32              sdl_init_flags; /* 0 = SDL_INIT_VIDEO (default) */
} SDL3_Config;
```

### All-in-One Registration
```c
// Source: derived from existing CEL_Module(SDL3_Engine, init) pattern
void SDL3_use(const SDL3_Config* config) {
    if (config && config->on_error) {
        sdl3_set_error_callback(config->on_error);
    }
    if (config && config->sdl_init_flags) {
        sdl3_set_init_flags(config->sdl_init_flags);
    }

    SDL3_Init_use();
    SDL3_FrameLoop_use();
    SDL3_Input_use();
    SDL3_Window_use();
    SDL3_Renderer_use();
    SDL3_Textures_use();
    SDL3_Text_use();
}
```

### CEL_Module(SDL3_Engine) Becomes a Thin Wrapper
```c
// Source: derived from existing pattern
CEL_Module(SDL3_Engine, init) {
    SDL3_use(NULL);
}
```

This preserves backward compatibility: `cels_register(SDL3_Engine)` triggers `SDL3_Engine_init()` which calls `SDL3_use(NULL)`.

### Individual _use() Function (Window Example)
```c
// Source: derived from existing CEL_Module(SDL3_Engine, init) registration
static bool s_window_registered = false;

void SDL3_Window_use(void) {
    if (s_window_registered) return;
    s_window_registered = true;

    cels_register(SDL3_WindowConfig, SDL3_WindowComponent, SDL3_EventQueue,
                  SDL3_Renderer, SDL3_WindowLC, SDL3_WindowStateSystem);

    cels_ensure_component(&SDL3_EventQueue_id, "SDL3_EventQueue",
                          sizeof(SDL3_EventQueue), CELS_ALIGNOF(SDL3_EventQueue));
    cels_ensure_component(&SDL3_Renderer_id, "SDL3_Renderer",
                          sizeof(SDL3_Renderer), CELS_ALIGNOF(SDL3_Renderer));
}
```

### Error Reporting Implementation
```c
// In sdl3_init.c
static SDL3_ErrorCallback s_error_callback = NULL;

void sdl3_set_error_callback(SDL3_ErrorCallback callback) {
    s_error_callback = callback;
}

void sdl3_report_error(const char* context) {
    const char* sdl_msg = SDL_GetError();
    if (!sdl_msg || sdl_msg[0] == '\0') {
        sdl_msg = "(no SDL error)";
    }
    if (s_error_callback) {
        s_error_callback(context, sdl_msg);
    } else {
        SDL_Log("SDL3 error [%s]: %s", context, sdl_msg);
    }
}
```

### Consumer Usage: Default (All Providers)
```c
// Existing pattern -- unchanged
cels_main() {
    cels_register(SDL3_Engine);
    // ... rest of app
}
```

### Consumer Usage: Custom Error Handler
```c
void my_error_handler(const char* context, const char* message) {
    fprintf(stderr, "SDL3 ERROR in %s: %s\n", context, message);
}

cels_main() {
    SDL3_use(&(SDL3_Config){
        .on_error = my_error_handler
    });
    // ... rest of app
}
```

### Consumer Usage: A La Carte
```c
cels_main() {
    SDL3_Init_use();       // Just SDL init/shutdown
    SDL3_Window_use();     // + window management
    SDL3_FrameLoop_use();  // + frame loop
    SDL3_Input_use();      // + input system
    SDL3_Renderer_use();   // + render clear/present
    // Skip textures and text -- custom rendering pipeline
    // ...
}
```

### Consumer Usage: Minimal (Headless-Like)
```c
cels_main() {
    SDL3_Init_use();  // Just SDL init/shutdown lifecycle
    // No window, no loop, no input
    // Useful for SDL utilities or testing
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `cels_register(SDL3_Engine)` registers everything | `cels_register(SDL3_Engine)` -> `SDL3_use(NULL)` -> individual `_use()` calls | Phase 9 | Backward compatible, but now decomposable |
| `SDL_Log()` for all SDL errors | `sdl3_report_error(context)` with optional callback | Phase 9 | Errors can be routed to application-specific handler |
| Implicit cleanup order in WindowStateSystem | Documented, hardcoded reverse-dependency cleanup with best-effort semantics | Phase 9 | More robust, errors reported instead of swallowed |

**Deprecated/outdated:**
- Direct `SDL_Log("SDL3: ... failed: %s", SDL_GetError())` calls in implementation files -- replace with `sdl3_report_error("context_name")`.

## Registration Partitioning Map

How the existing `CEL_Module(SDL3_Engine, init)` body maps to individual `_use()` functions:

| _use() Function | Registers | Current Lines in CEL_Module |
|-----------------|-----------|----------------------------|
| `SDL3_Init_use()` | SDL3_ContextState, SDL3_ContextLC, SDL3_ContextConfig | `cels_register(SDL3_ContextState, SDL3_ContextLC, SDL3_ContextConfig)` |
| `SDL3_FrameLoop_use()` | SDL3_FrameState, SDL3_FrameStateSystem | `cels_register(SDL3_FrameState, ...)` + `cels_register(SDL3_FrameStateSystem)` |
| `SDL3_Input_use()` | SDL3_InputSystem | `cels_register(SDL3_FrameState, SDL3_InputSystem)` (InputSystem part) |
| `SDL3_Window_use()` | SDL3_WindowConfig, SDL3_WindowComponent, SDL3_EventQueue, SDL3_Renderer, SDL3_WindowLC, SDL3_WindowStateSystem + ensure_component for EventQueue, Renderer | `cels_register(SDL3_WindowConfig, ...)` + both `cels_ensure_component` calls |
| `SDL3_Renderer_use()` | SDL3_RenderClearSystem, SDL3_RenderPresentSystem | `cels_register(SDL3_RenderClearSystem)` + `cels_register(SDL3_RenderPresentSystem)` |
| `SDL3_Textures_use()` | SDL3_Sprite, SDL3_TextureLoadSystem, SDL3_SpriteRenderSystem + ensure_component for Sprite | Phase 7 additions |
| `SDL3_Text_use()` | SDL3_Text, SDL3_TextHandle, SDL3_TextRenderSystem + ensure_component for Text, TextHandle | Phase 8 additions |

**Critical ordering constraint:** Within each `_use()`, registration order determines system execution order within the same phase. The call order of `_use()` functions in `SDL3_use()` must produce the same overall registration order as the current monolithic module init.

**Correct `SDL3_use()` call order:**
1. `SDL3_Init_use()` -- context lifecycle (no systems, just state + lifecycle)
2. `SDL3_FrameLoop_use()` -- FrameState (needed by InputSystem)
3. `SDL3_Input_use()` -- InputSystem (OnLoad, must run before WindowStateSystem)
4. `SDL3_Window_use()` -- window components + WindowStateSystem (OnLoad, after InputSystem)
5. `SDL3_Renderer_use()` -- RenderClear (PreRender) + RenderPresent (PostRender)
6. `SDL3_Textures_use()` -- TextureLoadSystem (OnLoad) + SpriteRenderSystem (OnRender)
7. `SDL3_Text_use()` -- TextRenderSystem (OnRender)

Note: `SDL3_FrameLoop_use()` currently registers both `SDL3_FrameState` and `SDL3_FrameStateSystem`. But `SDL3_InputSystem` also takes `SDL3_FrameState` in its current `cels_register` call: `cels_register(SDL3_FrameState, SDL3_InputSystem)`. This needs careful partitioning -- `SDL3_FrameState` should be registered by `SDL3_FrameLoop_use()`, and `SDL3_Input_use()` should only register `SDL3_InputSystem`. The guard flags prevent double-registration of `SDL3_FrameState`.

Also note: `SDL3_FrameStateSystem` registration currently appears AFTER `SDL3_WindowStateSystem` (line 253 in current code). This is correct because within OnLoad phase: InputSystem -> WindowStateSystem -> FrameStateSystem. The partitioning must preserve this order. So `SDL3_FrameLoop_use()` should register FrameState early but FrameStateSystem AFTER Window's registration. This is a subtle ordering issue.

**Resolution:** Split FrameLoop registration into two parts:
- `SDL3_FrameLoop_use()` registers `SDL3_FrameState` (the state singleton)
- `SDL3_FrameLoop_use()` also registers `SDL3_FrameStateSystem`, but it must be called AFTER `SDL3_Window_use()` in the `SDL3_use()` sequence to preserve ordering.

Wait -- this conflicts with the goal that `SDL3_FrameLoop_use()` is called third but `SDL3_FrameStateSystem` needs to register after WindowStateSystem. The solution: register `SDL3_FrameStateSystem` inside `SDL3_Window_use()` (since it depends on window state) or have a separate ordering mechanism.

**Better resolution:** Move `SDL3_FrameStateSystem` registration into a "late init" step. Since `SDL3_use()` controls the call order, we can structure it as:
```
SDL3_Init_use()      -> ContextState, ContextLC, ContextConfig
SDL3_FrameLoop_use() -> FrameState (state only, no system yet)
SDL3_Input_use()     -> InputSystem
SDL3_Window_use()    -> WindowConfig, WindowComponent, EventQueue, Renderer,
                        WindowLC, WindowStateSystem
                        + ensure_components
SDL3_FrameLoop_use() -> (already called, guard prevents re-entry)
  BUT we need FrameStateSystem to register HERE
```

**Cleanest solution:** Have `SDL3_use()` handle the ordering explicitly. The individual `_use()` functions register their core items, and `SDL3_use()` does a final `cels_register(SDL3_FrameStateSystem)` at the right point. But this means `SDL3_FrameLoop_use()` alone cannot set up a working frame loop without the developer manually registering FrameStateSystem...

**Pragmatic solution:** Since FrameStateSystem is tightly coupled to WindowStateSystem (it checks if all windows are closed), include FrameStateSystem registration in `SDL3_Window_use()`. FrameLoop_use() provides the FrameState singleton and any loop utilities (sdl3_should_run, sdl3_delta). Window_use() provides FrameStateSystem because it's the system that monitors window states. This is semantically clean: the "all windows closed -> stop running" logic belongs to the window provider.

## Cleanup Order Detail

### Per-Window Cleanup (CLOSING -> CLOSED in WindowStateSystem)
```
1. text handles:    SDL3_TextHandle entities -> sdl3_text_destroy()
2. text engine:     SDL3_Renderer->text_engine -> TTF_DestroyRendererTextEngine()
3. texture cache:   sdl3_texture_cache_remove_renderer(renderer)
4. draw buffer:     sdl3_draw_buffer_destroy(SDL3_Renderer)
5. renderer:        sdl3_renderer_destroy(renderer) -> SDL_DestroyRenderer()
6. window:          sdl3_window_destroy(window) -> SDL_DestroyWindow()
```

Each step: if the pointer is non-NULL, destroy it. If destroy fails, call `sdl3_report_error(context)`. Continue regardless.

### Global Cleanup (sdl3_shutdown in sdl3_init.c)
```
1. fonts:           sdl3_fonts_close_all() -- close all open TTF_Font slots
2. TTF:             TTF_Quit()
3. SDL:             SDL_Quit()
```

### Cleanup Guard for Partial Registration
When `_use()` is used a la carte, some cleanup steps may reference uninitialized resources. The existing NULL-check pattern (`if (SDL3_Renderer->renderer)`, `if (SDL3_Renderer->text_engine)`) already handles this. Systems only iterate entities that have the queried components, so if a component type was never registered, the system body is a no-op.

## Open Questions

1. **Should SDL3_use() be callable before or after cels_register(SDL3_Engine)?**
   - What we know: `cels_register(SDL3_Engine)` calls `SDL3_Engine_init()` which calls `SDL3_use(NULL)`. If developer calls `SDL3_use(&config)` first, then `cels_register(SDL3_Engine)` will call `SDL3_use(NULL)` which hits guard flags and does nothing (already registered). Config was already applied. This works.
   - If developer calls `cels_register(SDL3_Engine)` first (registers with defaults), then `SDL3_use(&config)`, the `_use()` functions hit guards (no-ops), but the config (error callback) is still applied (config storage is separate from registration).
   - Recommendation: Document both patterns as valid. `SDL3_use(&config)` replaces `cels_register(SDL3_Engine)` when custom config is needed. Both can be called -- the guard flags make it safe.

2. **Where does SDL3_Renderable_use() fit?**
   - What we know: Phase 6 introduces `SDL3_Renderable_use()` as a separate registration function for the draw vtable. It's not a CELS component/system registration -- it populates a static vtable.
   - Recommendation: Include `SDL3_Renderable_use()` call inside `SDL3_Renderer_use()`. The vtable is part of the renderer subsystem. When developer registers renderer, they get the draw vtable too.

3. **Config struct extensibility**
   - What we know: CONTEXT.md specifies "Config struct contains: error handler callback, SDL subsystem flags, log verbosity."
   - What's unclear: What "log verbosity" means in practice. The error callback handles errors. Normal operation is silent (per CONTEXT.md: "Silent cleanup"). Possible: a verbosity level that enables debug logging.
   - Recommendation: Start with just `on_error` and `sdl_init_flags`. Add verbosity later if needed. Two-field struct is simpler to validate and document.

## Sources

### Primary (HIGH confidence)
- CELS cels.h (local, `/home/cachy/workspaces/libs/cels/include/cels/cels.h`) -- CEL_Module, cels_register, CEL_OnError macros, module requires/provides
- cels-sdl3 sdl3_module.c (local) -- current registration pattern, system definitions, all existing CELS declarations
- cels-sdl3 cels_sdl3.h (local) -- public API surface, component types, module declaration
- cels-sdl3 sdl3_internal.h (local) -- internal function declarations, cross-TU patterns
- cels-sdl3 sdl3_init.c (local) -- init/shutdown, state management pattern
- cels-ncurses ncurses_module.c (local) -- sibling module registration pattern (reference)
- Phase 09-CONTEXT.md (local) -- user decisions constraining implementation
- Phase 06/07/08 PLAN.md files (local) -- _use() patterns for Phase 6-8 systems

### Secondary (MEDIUM confidence)
- Phase 06/07/08 RESEARCH.md files (local) -- established patterns for draw buffer, texture cache, text system
- Existing examples (local) -- consumer API patterns that must remain backward compatible

### Tertiary (LOW confidence)
- (none -- all findings derived from local codebase analysis)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new libraries, purely organizational refactor of existing code
- Architecture: HIGH -- patterns derived directly from existing codebase and CELS framework source; per-TU constraint verified from existing code comments and patterns
- Pitfalls: HIGH -- per-TU constraint, double registration, ordering issues all verified from existing codebase behavior
- Registration partitioning: HIGH -- exact mapping from current code to individual _use() functions documented
- Error reporting: HIGH -- simple function pointer pattern, well-understood; verified CEL_OnError is NOT suitable (calls abort)
- Cleanup ordering: HIGH -- SDL3 destruction requirements verified from wiki and existing code

**Research date:** 2026-03-21
**Valid until:** 2026-04-21 (stable internal refactor, 30-day validity)
