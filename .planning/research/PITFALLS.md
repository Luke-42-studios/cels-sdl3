# Pitfalls Research

**Domain:** SDL3 CELS Backend Module
**Researched:** 2026-03-15
**Confidence:** MEDIUM (based on training data knowledge of SDL3 API; WebSearch/Context7 unavailable for live verification -- all SDL3-specific API claims should be spot-checked against installed SDL3 headers before implementation)

---

## Critical Pitfalls

These cause rewrites, broken architecture, or hard-to-diagnose bugs.

---

### Pitfall 1: Using SDL2 API Patterns in SDL3 Code

**What goes wrong:** SDL3 renamed, removed, or restructured hundreds of functions from SDL2. Code written from SDL2 muscle memory or SDL2-era tutorials will fail to compile or exhibit subtle behavioral differences. This is the single most likely source of wasted time on this project.

**Why it happens:** SDL2 dominated for over a decade. Most tutorials, Stack Overflow answers, blog posts, and even Claude's training data are SDL2-oriented. The SDL2-to-SDL3 migration is not a minor version bump -- it is a full API redesign with systematic renaming, new return types, and removed functions.

**Key SDL2-to-SDL3 API changes (HIGH confidence on the pattern, MEDIUM on specifics -- verify against headers):**

| SDL2 Pattern | SDL3 Replacement | Notes |
|---|---|---|
| `SDL_Init(SDL_INIT_VIDEO)` | `SDL_Init(SDL_INIT_VIDEO)` | Same, but subsystem flags may differ |
| `SDL_CreateWindow(title, x, y, w, h, flags)` | `SDL_CreateWindow(title, w, h, flags)` | Position removed from creation; use `SDL_SetWindowPosition()` after |
| `SDL_CreateRenderer(window, -1, flags)` | `SDL_CreateRenderer(window, NULL)` | Renderer index removed; driver name string or NULL instead of index |
| `int` return codes (0=success) | `bool` return type (`true`=success) | Many functions changed from int to SDL_bool/bool |
| `SDL_bool` enum (`SDL_TRUE`/`SDL_FALSE`) | Standard C `bool` (`true`/`false`) | SDL_bool is removed in SDL3 |
| `SDL_WINDOW_SHOWN` flag | Removed -- windows shown by default | Passing old flag will fail to compile |
| `SDL_SetRenderDrawColor(r, g, b, a)` as Uint8 | Colors may use float (0.0-1.0) in some APIs | Mixed integer/float APIs; check each function |
| `SDL_RenderCopy(renderer, texture, src, dst)` | `SDL_RenderTexture(renderer, texture, src, dst)` | Renamed |
| `SDL_RenderCopyEx(...)` | `SDL_RenderTextureRotated(...)` | Renamed with different parameter ordering |
| `SDL_RenderFillRect(renderer, rect)` | `SDL_RenderFillRect(renderer, rect)` but with `SDL_FRect` | SDL3 uses float rects (`SDL_FRect`) throughout rendering |
| `SDL_Rect` for rendering | `SDL_FRect` for rendering | Integer rects still exist but rendering APIs use float versions |
| `SDL_PollEvent(&event)` | `SDL_PollEvent(&event)` | Same pattern but event struct layout changed |
| `event.window.event` for window sub-events | Each window event is its own event type | No more sub-event enum; `SDL_EVENT_WINDOW_RESIZED` is a top-level event |
| `SDL_KEYDOWN` | `SDL_EVENT_KEY_DOWN` | Event constants renamed to `SDL_EVENT_*` prefix |
| `SDL_GetKeyboardState(NULL)` | `SDL_GetKeyboardState(NULL)` | Returns `const bool*` instead of `const Uint8*` |
| `SDL_GameController*` | `SDL_Gamepad*` | Entire controller API renamed to "gamepad" |
| `SDL_GameControllerOpen(index)` | Gamepad events carry `SDL_JoystickID`; no index-based open | Instance-based, not index-based |
| `IMG_Load()` | `IMG_Load()` | SDL3_image keeps similar API but ensure SDL3-compatible version |
| `TTF_RenderText_Blended()` | May have changed to new text engine API | SDL3_ttf has significant API changes; verify carefully |

**How to avoid:**
1. Bookmark the official SDL2-to-SDL3 migration guide (https://wiki.libsdl.org/SDL3/MigrationGuide) and reference it constantly
2. Never write SDL calls from memory -- always check the SDL3 header signatures first
3. Use `#include <SDL3/SDL.h>` (not `SDL.h` or `SDL2/SDL.h`) -- the include path itself enforces the right version
4. Enable `-Werror` in CMake for the example app so type mismatches from old APIs are caught immediately
5. If using FetchContent, pin a specific SDL3 release tag, not `main` branch

**Warning signs:**
- Compilation errors mentioning unknown types like `SDL_bool`, `SDL_WINDOW_SHOWN`
- Functions taking wrong number of arguments
- Implicit conversions between `int` and `bool`
- Rect-related type errors (SDL_Rect vs SDL_FRect)

**Phase to address:** Phase 1 (Window Provider) -- this is day-one discipline. Establish correct SDL3 idioms in the very first code written.

---

### Pitfall 2: SDL_Renderer Bound to Wrong Window in Multi-Window Setup

**What goes wrong:** SDL3 ties each `SDL_Renderer` to exactly one `SDL_Window`. In a multi-window ECS architecture, if you store renderers as global/singleton state or fail to track the window-renderer pairing per entity, you will render to the wrong window silently, get blank windows, or crash when a window is destroyed but its renderer is still "active."

**Why it happens:** Most SDL tutorials (both SDL2 and SDL3) demonstrate single-window apps with one global renderer. The natural instinct is `static SDL_Renderer* g_renderer;`. When you add a second window, you need a second renderer, and every draw call must target the correct one. In an ECS, this means the renderer must be a component associated with each window entity, not a singleton.

**Specific failure modes:**
1. **Rendering to stale renderer:** Window A is destroyed, but a system still holds a pointer to its renderer and draws to it. Undefined behavior or crash.
2. **Implicit current renderer:** SDL_Renderer draw calls operate on whichever renderer was last made current. If systems interleave draw calls for different windows without switching renderers, frames get mixed.
3. **Present on wrong renderer:** `SDL_RenderPresent()` must be called on each window's renderer separately. Missing a present = blank window. Double-presenting = wasted frame.
4. **Texture created on wrong renderer:** `SDL_CreateTextureFromSurface()` creates a texture bound to a specific renderer. Using that texture with a different renderer is undefined behavior. In multi-window, you need per-renderer texture instances or a texture management strategy.

**How to avoid:**
1. **Store SDL_Window + SDL_Renderer as a paired component** on each window entity. Never separate them.
2. **Design the render system to iterate window entities**, binding the correct renderer for each window's draw pass.
3. **Textures must track which renderer they belong to.** Either duplicate textures per renderer, or ensure a texture is only used with its creating renderer.
4. **Use the WindowState state machine** to gate rendering: only render to windows in READY state, skip MINIMIZED/CLOSING/CLOSED.
5. **On window destroy, destroy renderer first, then window.** And ensure no systems hold stale pointers.

**Warning signs:**
- Blank or flickering window in multi-window setup
- Crash when closing one of multiple windows
- Textures appearing as white/black rectangles in second window
- `SDL_GetError()` reporting renderer errors

**Phase to address:** Phase 1 (Window Provider) and Phase 3 (Renderer Provider) -- the data model must be right from window creation, and rendering must enforce correct renderer binding.

---

### Pitfall 3: Event Loop Ownership Conflict Between SDL3 and CELS Frame Loop

**What goes wrong:** SDL3 requires `SDL_PollEvent()` to be called on the main thread, and it drives the window system's event pump. CELS has its own frame loop concept. If these two loops conflict -- for example, CELS drives the frame tick and SDL events are polled at the wrong time, or SDL events are polled in a non-main-thread system -- the result is frozen windows, unresponsive input, or platform-specific hangs (especially on macOS if ever ported).

**Why it happens:** ECS frameworks naturally want to own the main loop: `while (running) { ecs_progress(world, delta); }`. SDL3 also wants to pump events in the main loop. If the CELS frame loop is the outer driver, SDL event polling must happen exactly once per frame, at the right point in the system execution order, on the main thread. Getting this wrong is subtle because it may "work" on Linux but break on other platforms.

**Specific failure modes:**
1. **Polling events in a system that runs on a worker thread:** SDL event polling is not thread-safe. It must happen on the main thread.
2. **Polling events multiple times per frame:** If both the window system and the input system poll independently, events get split between them non-deterministically.
3. **Not polling events when window is minimized:** Even minimized windows need event polling to process restore events. Skipping the poll when "nothing to render" causes the window to become unresponsive.
4. **Frame loop timing conflicts:** If CELS's `ecs_progress()` uses its own delta time but SDL_events carry their own timestamps, time can desync.

**How to avoid:**
1. **Single event pump system:** One CELS system (e.g., `SDL3_PollEvents`) runs at the beginning of each frame, drains all SDL events, and distributes them to ECS components. No other system touches `SDL_PollEvent()`.
2. **Pin event pump to main pipeline phase:** Use flecs pipeline ordering to ensure the event pump runs before any input-processing or window-state systems.
3. **Always poll events, even when minimized.** Only skip rendering, never skip event polling.
4. **CELS frame loop is the outer loop;** SDL3 is the event/rendering backend within it. Do not use `SDL_AppIterate()` callback model -- use the traditional poll-based model where CELS owns the loop.

**Warning signs:**
- Window title bar shows "(Not Responding)" on some platforms
- Input events arrive out of order or are intermittently lost
- Closing the window via X button doesn't work reliably
- Window refuses to un-minimize

**Phase to address:** Phase 1 (Window Provider) -- the event pump architecture must be established before input or rendering are built on top of it.

---

### Pitfall 4: SDL3's Properties API Ignored in Favor of Legacy Configuration Patterns

**What goes wrong:** SDL3 introduced a properties-based configuration system (`SDL_CreateProperties()`, `SDL_SetStringProperty()`, `SDL_CreateWindowWithProperties()`) that replaces many flag-based and parameter-based patterns from SDL2. Ignoring this system means missing SDL3-native configuration options, writing verbose workaround code, and creating an API that cannot expose SDL3's full capabilities to CELS consumers.

**Why it happens:** The properties API is new to SDL3 and has no SDL2 equivalent. Developers reaching for familiar patterns will use `SDL_CreateWindow()` with flags, then call setters afterward. This works for simple cases but becomes unwieldy for advanced configuration (custom GL context attributes, Vulkan surface configuration, platform-specific window hints).

**Specific consequences:**
1. Missing ability to set platform-specific window properties at creation time
2. Verbose post-creation configuration that could be one-liner properties
3. API design that doesn't expose SDL3's property strings to consumers
4. Potential for the "it works but not correctly" failure when properties have different semantics than flags

**How to avoid:**
1. **Use `SDL_CreateWindowWithProperties()` from the start** rather than the convenience `SDL_CreateWindow()`. Even if you only set title/width/height initially, the properties-based path is the extensible one.
2. **Expose a configuration component** that maps to SDL3 properties, allowing CELS consumers to set arbitrary window properties before creation.
3. **Read the SDL3 properties documentation** for each subsystem (window, renderer, audio) before designing the component schema.

**Warning signs:**
- Needing many `SDL_SetWindow*()` calls immediately after `SDL_CreateWindow()`
- Inability to configure platform-specific options
- Feature requests that can't be fulfilled without API redesign

**Phase to address:** Phase 1 (Window Provider) -- window creation API design should consider the properties system from the beginning.

**Confidence:** MEDIUM -- the properties API is well-documented in SDL3 migration guides, but exact property names and capabilities should be verified against headers.

---

### Pitfall 5: Destroying SDL Resources in Wrong Order

**What goes wrong:** SDL3 resources have implicit dependency ordering: textures depend on their renderer, renderers depend on their window, and all depend on SDL being initialized. Destroying in the wrong order causes use-after-free, crashes, or driver errors. In an ECS with deferred destruction and observer-based cleanup, the destruction order is not deterministic by default.

**Why it happens:** ECS entity destruction is typically deferred to end-of-frame. If a window entity has components for SDL_Window, SDL_Renderer, and multiple SDL_Textures, the order in which component destructors fire is implementation-defined (depends on flecs internals). The correct order is: textures first, then renderer, then window, then SDL_Quit last.

**Specific failure modes:**
1. **Renderer destroyed before its textures:** SDL_Texture becomes invalid when its parent renderer is destroyed. Accessing it afterward is UB.
2. **Window destroyed before renderer:** Renderer references a window surface that no longer exists.
3. **SDL_Quit() called while resources still exist:** All SDL state is invalidated, dangling pointers everywhere.
4. **ECS world destroyed without explicit SDL cleanup:** If flecs destroys entities in arbitrary order during `ecs_fini()`, the destruction cascade is unsafe.

**How to avoid:**
1. **Explicit destruction ordering in a shutdown system,** not component destructors. A `SDL3_Shutdown` system that runs during CELS shutdown phase should:
   - Destroy all textures
   - Destroy all renderers
   - Destroy all windows
   - Call `SDL_Quit()`
2. **Use flecs `OnRemove` hooks with dependency-aware ordering.** If using component hooks for cleanup, ensure texture components are removed before renderer components via explicit ordering or by making textures child entities of the window entity (flecs destroys children before parents by default).
3. **Never store raw SDL pointers in components without a cleanup strategy.** Prefer a managed handle component that knows its dependencies.
4. **Guard all SDL destroy calls with null checks.** Double-destroy protection.

**Warning signs:**
- Crashes on window close (especially the last window)
- Crashes on application exit
- Valgrind/ASan reporting use-after-free on shutdown
- `SDL_GetError()` reporting "Invalid renderer" or similar

**Phase to address:** Phase 1 (Window Provider) for window/renderer lifecycle, Phase 3 (Renderer/Texture) for texture cleanup, and explicitly revisited during integration testing.

---

### Pitfall 6: Incorrect SDL3 Event Type Handling (Restructured Event System)

**What goes wrong:** SDL3 fundamentally restructured how events are categorized. SDL2 used a `SDL_WindowEvent` with a sub-type enum (`event.window.event == SDL_WINDOWEVENT_RESIZED`). SDL3 promotes each window event to a top-level event type (`event.type == SDL_EVENT_WINDOW_RESIZED`). Code that checks for `SDL_WINDOWEVENT` and then switches on sub-types will not compile in SDL3. More subtly, the event data layout within the union changed.

**Why it happens:** This is one of the most disruptive SDL2-to-SDL3 changes, and it affects the core event dispatch code that everything else depends on.

**Key event mapping (MEDIUM confidence -- verify against SDL3/SDL_events.h):**

| SDL2 | SDL3 |
|---|---|
| `SDL_QUIT` | `SDL_EVENT_QUIT` |
| `SDL_KEYDOWN` | `SDL_EVENT_KEY_DOWN` |
| `SDL_KEYUP` | `SDL_EVENT_KEY_UP` |
| `SDL_MOUSEMOTION` | `SDL_EVENT_MOUSE_MOTION` |
| `SDL_MOUSEBUTTONDOWN` | `SDL_EVENT_MOUSE_BUTTON_DOWN` |
| `SDL_MOUSEWHEEL` | `SDL_EVENT_MOUSE_WHEEL` |
| `SDL_WINDOWEVENT` + sub-type | `SDL_EVENT_WINDOW_*` (each is top-level) |
| `SDL_CONTROLLERDEVICEADDED` | `SDL_EVENT_GAMEPAD_ADDED` |
| `SDL_CONTROLLERAXISMOTION` | `SDL_EVENT_GAMEPAD_AXIS_MOTION` |
| `SDL_CONTROLLERBUTTONDOWN` | `SDL_EVENT_GAMEPAD_BUTTON_DOWN` |
| `SDL_FINGERDOWN` | `SDL_EVENT_FINGER_DOWN` |

**Additional changes:**
- `event.key.keysym.sym` replaced by `event.key.key` (SDL_Keycode directly)
- `event.key.keysym.scancode` replaced by `event.key.scancode`
- Mouse events carry `float` coordinates, not `int`
- Window ID is accessible via `event.window.windowID` (or similar per-event-type accessor)

**How to avoid:**
1. **Build the event dispatch table from SDL3 headers, not memory or tutorials.** Read `SDL3/SDL_events.h` directly.
2. **Write the event dispatcher as a mapping layer early** so the rest of the codebase works with CELS-native event components, not raw SDL events.
3. **Test every event type you intend to handle** with a simple printf/log before building higher-level systems on top.

**Warning signs:**
- Unhandled event types in switch statements
- Input events that "never fire" because the wrong constant is checked
- Window resize not detected
- Gamepad input completely non-functional

**Phase to address:** Phase 1 (Window Provider) for window events, Phase 2 (Input Provider) for keyboard/mouse/gamepad/touch events.

---

### Pitfall 7: Blocking the Main Thread with Synchronous Asset Loading

**What goes wrong:** Loading textures (via SDL3_image) and fonts (via SDL3_ttf) is I/O-bound and can block the main thread for 10-100ms per asset. In a frame loop targeting 16ms frames, loading even a few assets synchronously during gameplay causes visible stuttering. Loading at startup is acceptable for small apps but does not scale.

**Why it happens:** The simple path is `IMG_Load("sprite.png")` -> `SDL_CreateTextureFromSurface(renderer, surface)` inline wherever you need the texture. This is fine for the example app but creates an architectural assumption (synchronous loading) that is painful to undo later.

**Specific to this project:** Since CELS is an ECS framework, the natural pattern is "add a `TextureRequest` component, and a system loads it." If that system runs in the main pipeline and loads synchronously, it blocks the frame.

**How to avoid:**
1. **For v1, synchronous loading at initialization is acceptable.** Don't over-engineer async loading for the first milestone.
2. **But design the loading API so it CAN become async later.** Use a request/result component pattern: `TextureRequest` component triggers loading, `TextureLoaded` component holds the result. The implementation can be sync now and async later without changing the consumer API.
3. **Never load assets inside render systems.** Load during a dedicated "asset loading" phase that runs before rendering.
4. **SDL3_image's `IMG_Load()` returns an `SDL_Surface*`** which is CPU-side. The conversion to `SDL_Texture` (GPU-side) via `SDL_CreateTextureFromSurface()` must happen on the main thread with the correct renderer. Keep this constraint in mind for any future async design.

**Warning signs:**
- Hitching when new entities with textures are spawned
- Frame time spikes correlating with asset loads
- "Loading screen" need arising unexpectedly

**Phase to address:** Design the component schema in Phase 3 (Renderer) to be async-friendly, but implement synchronously. Flag for async implementation in a future asset pipeline milestone.

---

### Pitfall 8: SDL3_ttf API Changes Breaking Text Rendering Assumptions

**What goes wrong:** SDL3_ttf underwent significant API changes from SDL2_ttf. The rendering model changed -- SDL3_ttf may use a text engine concept where you create text objects and render them, rather than the SDL2 pattern of `TTF_RenderText_Blended()` returning an `SDL_Surface*`. Code written assuming the SDL2_ttf API will not compile or will use deprecated paths.

**Why it happens:** SDL3_ttf is less widely documented than SDL3 core, and most font rendering tutorials target SDL2_ttf. The new text engine API is more powerful but has a different workflow.

**Specific concerns (LOW-MEDIUM confidence -- verify against SDL3_ttf headers):**
1. The old `TTF_RenderText_Blended(font, text, color)` -> `SDL_Surface*` -> `SDL_CreateTextureFromSurface()` pattern may be deprecated or removed
2. SDL3_ttf may introduce `TTF_CreateText()` / `TTF_DrawRendererText()` that integrates more directly with SDL_Renderer
3. Font sizing, hinting, and DPI handling may have new APIs
4. Text wrapping and layout capabilities may have changed

**How to avoid:**
1. **Before writing any text rendering code, read the SDL3_ttf headers directly.** Do not assume the SDL2_ttf API shape.
2. **Check SDL3_ttf release notes and migration guide** if one exists.
3. **Prototype the simplest possible text render** (load font, render "Hello World" to screen) before designing the text component system.
4. **Consider that SDL3_ttf's new API might be more compatible with the ECS pattern** -- if text objects are persistent and renderable, they map naturally to components.

**Warning signs:**
- SDL3_ttf header has completely different function signatures than expected
- Cannot find `TTF_RenderText_Blended` in SDL3_ttf headers
- Text rendering produces no visible output despite no errors

**Phase to address:** Phase 4 (Text Rendering) -- treat this as requiring its own mini-research spike before implementation. Do not assume SDL2_ttf patterns.

---

### Pitfall 9: Single-Window Assumptions Baked into Component Design

**What goes wrong:** Even when explicitly planning for multi-window, it is easy to design components and systems that implicitly assume one window. Common manifestations: input state stored as a singleton (not per-window), "the renderer" as a global, camera/viewport as a singleton, and event routing that doesn't filter by window ID.

**Why it happens:** Single-window is simpler to think about. The first implementation naturally starts with one window. If the data model doesn't enforce multi-window from the beginning, single-window assumptions accumulate in every system.

**Specific patterns that break multi-window:**
1. **Singleton input state:** `InputState` as a global component. Mouse position is window-relative -- which window? Keyboard focus is per-window.
2. **Global renderer reference:** Any system that reaches for "the renderer" instead of "this window's renderer."
3. **SDL event dispatch without window ID filtering:** SDL3 events carry `windowID`. If the input system doesn't route events to the correct window entity, all windows process all events.
4. **Texture caching without renderer affinity:** A global texture cache that serves textures created on renderer A to renderer B.
5. **Viewport/camera as singleton:** If viewport transform is global, it cannot differ per window.

**How to avoid:**
1. **Make window entity the root of a hierarchy:** Window entity owns renderer, input state, and viewport as child entities or paired components.
2. **Filter SDL events by windowID** in the event dispatch system, populating only the correct window entity's input components.
3. **Test with two windows from the earliest possible phase.** Don't wait until "multi-window phase" -- test with two windows as soon as one window works.
4. **Code review checklist item:** "Does this system access any singleton? If yes, should it be per-window?"

**Warning signs:**
- Input working in one window but not others
- Rendering appearing in wrong window
- All windows responding to events intended for one
- "It works with one window" but breaks with two

**Phase to address:** Phase 1 (Window Provider) -- the entity/component model for windows determines whether multi-window is possible. This is an architectural decision, not a feature to add later.

---

### Pitfall 10: CMake FetchContent Version Pinning and SDL3 Companion Library Compatibility

**What goes wrong:** SDL3, SDL3_image, and SDL3_ttf are separate projects with independent release schedules. Using `FetchContent` without pinning specific compatible versions leads to builds that break when upstream updates, or worse, subtle runtime bugs when companion libraries are built against a different SDL3 version than the one in use.

**Why it happens:** The convenient `FetchContent_Declare` with `GIT_TAG main` always fetches latest, which may include breaking changes. Even pinning SDL3 but not SDL3_image/SDL3_ttf can cause ABI mismatches if their SDL3 version requirements advance.

**Specific failure modes:**
1. **SDL3_image built against SDL3 3.2 but project uses SDL3 3.0:** Missing symbols at link time, or crashes at runtime.
2. **FetchContent fetching SDL3 main branch:** Nightly builds may have in-progress API changes.
3. **System-installed SDL3 conflicting with FetchContent SDL3:** If the system has SDL3 installed and FetchContent also builds it, CMake may link against the wrong one.
4. **SDL3_ttf depending on FreeType/HarfBuzz:** Transitive dependencies that FetchContent may or may not pull in correctly.

**How to avoid:**
1. **Pin all three dependencies to specific release tags** (e.g., `release-3.2.0` or whatever the current stable tag is).
2. **Document the known-good version combination** in the project README or CMakeLists.txt.
3. **Use `find_package(SDL3)` first, fall back to FetchContent** -- prefer system packages for stability, use FetchContent for environments without SDL3.
4. **Test both build paths** (system SDL3 and FetchContent SDL3) in CI.
5. **Set `SDL_SHARED OFF` and `SDL_STATIC ON`** (or vice versa) explicitly in FetchContent to avoid building both.

**Warning signs:**
- Build works on one machine but not another
- Linker errors about missing SDL3 symbols despite SDL3 being "found"
- Runtime crashes in SDL3_image/SDL3_ttf functions
- CMake warnings about "SDL3 found in multiple locations"

**Phase to address:** Phase 0 / project setup -- CMakeLists.txt is the first file written.

---

## Moderate Pitfalls

Mistakes that cause delays or accumulate technical debt.

---

### Pitfall 11: Ignoring SDL3's Float-Based Coordinate System for Rendering

**What goes wrong:** SDL3 moved rendering coordinates from integer (`SDL_Rect`, `int` positions) to floating-point (`SDL_FRect`, `float` positions). If the CELS component schema uses integer coordinates for positions and sizes, there will be constant casting, precision loss, and an inability to do sub-pixel positioning for smooth animation.

**How to avoid:** Use `float` for all position and size components from day one. Define rendering components with `SDL_FRect`-compatible layouts. Only convert to integers at the window/display boundary if needed.

**Phase to address:** Phase 3 (Renderer Provider) -- component schema design.

---

### Pitfall 12: Not Handling High-DPI / Display Scale Correctly

**What goes wrong:** SDL3 has improved high-DPI support compared to SDL2. Window size in "screen coordinates" differs from size in "pixels" on high-DPI displays. If rendering code assumes 1:1 screen-to-pixel mapping, content will appear at wrong size or position on high-DPI displays, or mouse coordinates won't match rendered positions.

**How to avoid:**
1. Use `SDL_GetWindowSizeInPixels()` for rendering dimensions, not `SDL_GetWindowSize()`.
2. Account for display scale factor in input coordinate mapping.
3. Store both logical size and pixel size in the window component.
4. Test on a high-DPI display or use `SDL_HINT_WINDOWS_DPI_SCALING` (if applicable on Linux/Wayland).

**Phase to address:** Phase 1 (Window Provider) for size tracking, Phase 2 (Input) for coordinate mapping, Phase 3 (Renderer) for correct rendering dimensions.

---

### Pitfall 13: Polling Gamepad State Without Handling Hot-Plug

**What goes wrong:** Gamepads can be connected and disconnected at any time. SDL3 sends `SDL_EVENT_GAMEPAD_ADDED` and `SDL_EVENT_GAMEPAD_REMOVED` events. Code that opens gamepads at init and never handles hot-plug will crash when a gamepad is disconnected (dangling `SDL_Gamepad*` pointer) or fail to recognize newly connected gamepads.

**How to avoid:**
1. Handle `SDL_EVENT_GAMEPAD_ADDED`: create/update a gamepad entity with the `SDL_Gamepad*` handle.
2. Handle `SDL_EVENT_GAMEPAD_REMOVED`: destroy the gamepad entity and close the handle.
3. Never cache `SDL_Gamepad*` outside of the component that owns it.
4. Use instance IDs (`SDL_JoystickID`), not device indices, for gamepad identification. SDL3 uses instance IDs consistently.

**Phase to address:** Phase 2 (Input Provider) -- gamepad support must include hot-plug from the start.

---

### Pitfall 14: Treating the ECS as a Scene Graph

**What goes wrong:** Developers familiar with scene-graph engines (Unity, Godot) try to build parent-child transform hierarchies in the ECS. While flecs supports entity hierarchies, using them for scene graph semantics (world transform = parent transform * local transform) requires explicit system code and is a significant feature. Attempting this ad-hoc leads to incorrect rendering order, missing transforms, and performance issues from hierarchy traversal.

**How to avoid:**
1. For v1, use flat entity positioning (world coordinates only). No parent-child transform chains.
2. If hierarchy is needed, design it explicitly as a system that computes world transforms from local transforms, not as an implicit ECS relationship side-effect.
3. Rendering order should be explicit (z-order component or layer component), not derived from hierarchy.

**Phase to address:** Phase 3 (Renderer) -- keep it simple. Defer hierarchical transforms to a future milestone.

---

### Pitfall 15: Not Separating SDL3 Initialization from Window Creation

**What goes wrong:** Calling `SDL_Init()` and `SDL_CreateWindow()` in the same system/function creates a coupling that makes testing, multi-window, and error recovery harder. SDL3 initialization is a one-time global operation; window creation is per-window and repeatable.

**How to avoid:**
1. `SDL_Init()` happens once in the Engine module's `_use()` registration, as a lifecycle hook (e.g., `OnStart` system).
2. Window creation happens per-window-entity, triggered by component addition.
3. `SDL_Quit()` happens in `OnStop` or `ecs_fini()` cleanup, after all windows are destroyed.

**Phase to address:** Phase 1 (Window Provider).

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Global/singleton renderer | Simpler first implementation | Blocks multi-window, blocks future GPU backend swap | Never -- even for v1, store per-window |
| Raw `SDL_Texture*` in components (no wrapper) | Less code, direct SDL access | No reference counting, no cleanup tracking, textures leak on entity destroy | Acceptable for v1 if `OnRemove` hooks handle cleanup |
| Synchronous asset loading | Simpler implementation | Frame hitching with more assets, must rewrite for async | Acceptable for v1 with documented plan to add async |
| Hardcoded key bindings in systems | Fast to prototype | Cannot rebind, accessibility issues, inflexible | Acceptable for example app only, not for input provider |
| Integer coordinates in components | Matches mental model | Precision loss, constant casting, doesn't match SDL3 float API | Never -- use float from the start |
| `SDL_CreateWindow()` instead of `WithProperties()` | Simpler initial call | Cannot extend to platform-specific config without API change | Acceptable for v1 if the component schema is properties-friendly |
| Monolithic event dispatch (giant switch) | All events handled in one place | Unmaintainable as event count grows, hard to extend | Acceptable for Phase 1 prototype, refactor in Phase 2 |
| No render batching | Simpler draw calls | Performance ceiling with many sprites; each `SDL_RenderTexture()` is a draw call | Acceptable for v1 with documented limitation |

---

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Creating/destroying textures every frame | Low FPS, GPU memory churn, driver stalls | Cache textures in components, create once, reuse | >10 textured entities |
| Calling `SDL_RenderPresent()` without vsync awareness | Tearing, or 100% CPU usage in busy-wait | Let SDL3 handle vsync via renderer creation flags (`SDL_RENDERER_PRESENTVSYNC`) or properties | Immediately visible |
| Rebuilding font textures every frame for static text | CPU spike on text-heavy screens | Cache rendered text surfaces/textures, only re-render on text change | >5 text elements |
| Querying `SDL_GetWindowSize()` every frame | Unnecessary syscall overhead per frame | Cache window size in component, update only on `SDL_EVENT_WINDOW_RESIZED` | Measurable with many windows |
| Unnecessary `SDL_SetRenderDrawColor()` calls | Minor CPU overhead per draw call | Batch draw calls by color, or track current color to avoid redundant state changes | >100 draw calls per frame |
| Loading full-resolution textures when smaller would suffice | GPU memory exhaustion, slower rendering | Accept size hints in texture loading, scale surfaces before GPU upload | >50 textures or textures >4K |
| Polling all input devices when none are connected | Wasted cycles, unnecessary SDL calls | Only poll devices that have corresponding entities (created on connect events) | Not critical but wasteful |

---

## "Looks Done But Isn't" Checklist

These are things that appear to work in basic testing but fail in real usage.

- [ ] **Window close via X button works** -- but does the ECS entity get cleaned up, and do all resources (renderer, textures) get destroyed in correct order?
- [ ] **Rendering works** -- but does it work after a window resize? SDL3 may invalidate the renderer or require re-creation of render targets on resize.
- [ ] **Input works in one window** -- but do events route correctly to the right window entity when two windows are open?
- [ ] **Gamepad works** -- but does it survive a disconnect/reconnect cycle without leaking handles?
- [ ] **Text renders correctly** -- but does it render correctly at different DPI scales? Different font sizes? With Unicode characters?
- [ ] **Application exits cleanly** -- but does it exit cleanly when closed via Ctrl+C / SIGINT? Does the ECS shutdown sequence run?
- [ ] **Multi-window works** -- but can you close one window and keep the other running without crashing?
- [ ] **Texture loading works** -- but does it fail gracefully when the file doesn't exist? Is SDL_GetError() checked?
- [ ] **The frame loop runs** -- but does it respect vsync? Does it idle correctly when minimized (not burning CPU)?
- [ ] **CMake build works** -- but does it work with both system SDL3 and FetchContent SDL3? On a clean machine?
- [ ] **The example app works** -- but does it demonstrate the CELS patterns (Feature/Provider, components, systems) or does it bypass them with direct SDL calls?

---

## Pitfall-to-Phase Mapping

| Phase | Pitfalls to Address | Risk Level |
|-------|-------------------|------------|
| **Phase 0: Project Setup / CMake** | #10 (FetchContent versioning) | MEDIUM -- get this wrong and every subsequent phase fights build issues |
| **Phase 1: Window Provider** | #1 (SDL2 vs SDL3 APIs), #2 (renderer-window pairing), #3 (event loop ownership), #4 (properties API), #5 (resource destruction order), #6 (event type changes), #9 (multi-window data model), #15 (init vs window creation), #12 (high-DPI) | HIGH -- this phase sets the architectural foundation; most critical pitfalls land here |
| **Phase 2: Input Provider** | #6 (event type mapping), #9 (per-window input state), #13 (gamepad hot-plug), #12 (coordinate mapping) | MEDIUM -- depends on correct event architecture from Phase 1 |
| **Phase 3: Renderer Provider** | #2 (renderer binding), #5 (texture cleanup), #7 (synchronous loading), #11 (float coordinates), #14 (not a scene graph) | MEDIUM-HIGH -- texture lifecycle and renderer binding are the tricky parts |
| **Phase 4: Text Rendering** | #8 (SDL3_ttf API changes) | MEDIUM -- requires its own mini-research spike; SDL3_ttf is the least-documented dependency |
| **Phase 5: Engine Module + Example** | All pitfalls surface here in integration | LOW individually, but integration testing catches accumulated issues |

**Phase 1 carries the most pitfall risk.** It is the foundation phase where SDL3 API patterns, event architecture, multi-window data model, and resource lifecycle are established. Extra time invested in Phase 1 correctness prevents cascading issues in all subsequent phases.

---

## Confidence Notes

| Pitfall | Confidence | Rationale |
|---------|------------|-----------|
| #1 (SDL2 vs SDL3 API) | HIGH on pattern, MEDIUM on specific renames | SDL3 API redesign is well-documented; exact function signatures should be verified against installed headers |
| #2 (Multi-window renderer) | HIGH | This is a fundamental SDL architecture constraint, unchanged from SDL2 |
| #3 (Event loop ownership) | HIGH | Main-thread event pumping is an SDL requirement across all versions |
| #4 (Properties API) | MEDIUM | Properties system is confirmed SDL3 feature; exact capabilities need header verification |
| #5 (Destruction order) | HIGH | Resource dependency ordering is a universal graphics API concern |
| #6 (Event restructuring) | HIGH on the restructuring, MEDIUM on exact constant names | SDL3 event changes are the most-discussed migration topic |
| #7 (Sync loading) | HIGH | I/O-bound loading blocking the main thread is a universal game dev pitfall |
| #8 (SDL3_ttf changes) | LOW-MEDIUM | SDL3_ttf is the least documented; claims about new text engine API need verification |
| #9 (Single-window assumptions) | HIGH | Architectural pattern, not API-specific |
| #10 (CMake versioning) | HIGH | Standard FetchContent best practices |
| #11 (Float coordinates) | MEDIUM | SDL3 float rendering is confirmed; exact API shapes need verification |
| #12 (High-DPI) | MEDIUM | SDL3 improved high-DPI handling; exact APIs need verification |
| #13 (Gamepad hot-plug) | HIGH | Hot-plug has been in SDL for years; SDL3 just renames the APIs |
| #14 (ECS as scene graph) | HIGH | Common ECS architectural mistake |
| #15 (Init vs window creation) | HIGH | Separation of concerns, SDL-version-independent |

---
*Pitfalls research for: SDL3 CELS Backend Module*
*Researched: 2026-03-15*
*Source: Training data knowledge (May 2025 cutoff). WebSearch and Context7 were unavailable for live verification. All SDL3-specific API claims (function names, parameter types, constant names) should be verified against installed SDL3 headers before implementation.*
