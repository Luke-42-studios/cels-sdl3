# Phase 3: Frame Loop - Research

**Researched:** 2026-03-15
**Domain:** SDL3 high-resolution timing, event pumping, frame rate control, CELS system scheduling integration
**Confidence:** HIGH

## Summary

Phase 3 adds a frame loop that integrates SDL3 event pumping and high-resolution delta time calculation into CELS system scheduling. The research investigated five areas: (1) SDL3 3.4.2 timing APIs (`SDL_GetPerformanceCounter/Frequency`) verified from the locally fetched headers, (2) SDL3 event loop APIs (`SDL_PollEvent`, `SDL_WaitEvent`) verified from headers and official wiki, (3) SDL3 frame rate control mechanisms (`SDL_DelayPrecise`, `SDL_DelayNS`, `SDL_NS_PER_SECOND` macros) verified from headers, (4) CELS framework integration points (`cels_step(dt)` -> `cels_engine_progress(dt)` -> `ecs_progress(world, dt)`, `cel_delta()`, `cels_quit()`, `cels_running()`, custom phases, CEL_State for global state singletons) verified from cels.h and cels_api.c source, and (5) the interaction between exit conditions and the existing window state machine.

Key findings: `cels_step(float dt)` passes its argument directly to `ecs_progress()`, making it available inside systems via `cel_delta()`. Delta time must be computed externally and passed in -- the framework does not compute it. SDL3 provides `SDL_GetPerformanceCounter()` (Uint64, thread-safe) and `SDL_GetPerformanceFrequency()` (Uint64, counts-per-second, thread-safe) for sub-microsecond timing. For frame rate capping, SDL3 3.4.2 provides `SDL_DelayPrecise(Uint64 ns)` which busy-waits for precision, and `SDL_DelayNS(Uint64 ns)` which sleeps via OS scheduler for lower CPU. The `SDL_NS_PER_SECOND` macro (1000000000LL) and conversion macros (`SDL_SECONDS_TO_NS`, `SDL_NS_TO_SECONDS`) are available. For pause behavior, `SDL_WaitEvent()` blocks until an event arrives with zero CPU usage. `SDL_EVENT_QUIT` (0x100) is sent automatically when the last window closes (controlled by `SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE`, default "1").

**Primary recommendation:** Create two CELS systems in sdl3_module.c: (1) an event pump system at OnLoad phase that drains SDL events via `SDL_PollEvent` in a while loop (or blocks via `SDL_WaitEvent` when all windows minimized), and (2) a frame loop state system that checks window states and manages the running flag. Delta time calculation lives in a helper function called before `cels_step()` in the consumer's while loop, or inside a dedicated CEL_State singleton updated by a pre-tick system. Per the CONTEXT.md decision, delta time is exposed as a session parameter (not an ECS component), so it should be computed externally and passed to `cels_step(computed_dt)`.

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| SDL3 | 3.4.2 | `SDL_GetPerformanceCounter/Frequency`, `SDL_PollEvent`, `SDL_WaitEvent`, `SDL_DelayPrecise`, `SDL_DelayNS` | Already fetched in Phase 1 |
| CELS | 0.5.3 | `cels_step(dt)`, `cel_delta()`, `cels_quit()`, `cels_running()`, `CEL_System`, `CEL_State`, `cel_mutate` | Framework -- all ECS patterns |

### Supporting

No additional libraries needed. Frame loop is pure SDL3 timing + CELS scheduling.

### Key SDL3 APIs for This Phase

| API | Signature | Purpose | Confidence |
|-----|-----------|---------|------------|
| `SDL_GetPerformanceCounter` | `Uint64 SDL_GetPerformanceCounter(void)` | High-resolution counter value (thread-safe) | HIGH -- verified from SDL3 3.4.2 header and wiki |
| `SDL_GetPerformanceFrequency` | `Uint64 SDL_GetPerformanceFrequency(void)` | Counts per second for converting counter to time (thread-safe) | HIGH -- verified from header and wiki |
| `SDL_PollEvent` | `bool SDL_PollEvent(SDL_Event *event)` | Non-blocking event dequeue; returns false when empty (main thread only) | HIGH -- verified from header and wiki |
| `SDL_WaitEvent` | `bool SDL_WaitEvent(SDL_Event *event)` | Blocking wait until event available (main thread only, zero CPU) | HIGH -- verified from header and wiki |
| `SDL_DelayPrecise` | `void SDL_DelayPrecise(Uint64 ns)` | High-precision delay with busy-wait, nanosecond resolution (any thread) | HIGH -- verified from header |
| `SDL_DelayNS` | `void SDL_DelayNS(Uint64 ns)` | OS-scheduler delay in nanoseconds, lower CPU than DelayPrecise (any thread) | HIGH -- verified from header |
| `SDL_Delay` | `void SDL_Delay(Uint32 ms)` | Millisecond-precision delay via OS scheduler (any thread) | HIGH -- verified from header |
| `SDL_EVENT_QUIT` | `0x100` | Sent on user-requested quit (close last window, Ctrl+C, etc.) | HIGH -- verified from header |

### Key SDL3 Timing Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| `SDL_NS_PER_SECOND` | `1000000000LL` | Nanoseconds in a second |
| `SDL_NS_PER_MS` | `1000000` | Nanoseconds in a millisecond |
| `SDL_SECONDS_TO_NS(S)` | `((Uint64)(S)) * SDL_NS_PER_SECOND` | Convert whole seconds to nanoseconds |
| `SDL_NS_TO_SECONDS(NS)` | `(NS) / SDL_NS_PER_SECOND` | Convert nanoseconds to seconds (integer division) |
| `SDL_MS_TO_NS(MS)` | `((Uint64)(MS)) * SDL_NS_PER_MS` | Convert milliseconds to nanoseconds |

### Key CELS APIs for This Phase

| API | Purpose | Notes |
|-----|---------|-------|
| `cels_step(float dt)` | Expands to `cels_engine_progress(dt)` -- drives all pipeline phases with given delta | dt passed directly to `ecs_progress()` |
| `cel_delta()` | Returns `float` delta time inside a system body | Reads from iterator's `delta_time` field |
| `cels_running()` | Returns `!cels_should_quit()` -- loop condition | Used in consumer while loop |
| `cels_quit()` / `cel_quit()` | Expands to `cels_request_quit()` -- sets quit flag | `cel_quit()` usable inside system bodies |
| `CEL_State(Name)` | Declare a global state singleton (TU-local data) | For frame loop state |
| `CEL_Define_State(Name) { fields }` | Define state type in header (cross-TU) | For public state |
| `cels_state_bind(Name)` | Register static data pointer for cross-TU reads | Called once in owner TU |
| `cel_read(Name)` | Read-only access to state from any TU | Returns `const struct Name*` |
| `cel_mutate(Name) { ... }` | Mutable write scope for state, auto-notifies reactivity | For updating frame state |
| `CEL_System(Name, .phase = Phase)` | Define a system running at specified phase | OnLoad, OnUpdate, etc. |
| `cel_run { ... }` | System body with no query (no entity iteration) | For event pump, frame state |

## Architecture Patterns

### Recommended Project Structure

```
src/
    sdl3_module.c        # Add new systems: event pump, frame state checker
    sdl3_init.c          # Unchanged (context init/shutdown)
    sdl3_internal.h      # Add new internal function declarations
    window/
        sdl3_window.c    # Unchanged (window create/destroy/events)
    loop/
        sdl3_loop.c      # NEW: delta time calculation, FPS tracking, frame rate capping
include/
    cels_sdl3.h          # Add new state/component types: SDL3_FrameState, SDL3_FrameConfig
```

### Pattern 1: Delta Time Calculation

**What:** Compute elapsed seconds between frames using SDL3 performance counters.
**When to use:** Every frame, before calling `cels_step()`.
**Rationale:** The CONTEXT.md decision specifies "delta time exposed via session parameter (not an ECS component) -- available through session context during tick." Since `cels_step(dt)` accepts the delta as a float and propagates it to `cel_delta()` inside systems, the simplest implementation is computing dt externally and passing it directly.

```c
// Source: SDL3 3.4.2 SDL_timer.h + CELS cels.h
static Uint64 s_last_counter = 0;
static Uint64 s_frequency = 0;

static float sdl3_compute_delta(void) {
    if (s_frequency == 0) {
        s_frequency = SDL_GetPerformanceFrequency();
        s_last_counter = SDL_GetPerformanceCounter();
        return 0.0f;  /* first frame: zero delta */
    }
    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)(now - s_last_counter) / (float)s_frequency;
    s_last_counter = now;
    return dt;
}
```

**Key design point:** The float division `(now - last) / frequency` gives seconds as a float. This is the standard SDL3 pattern verified from multiple sources. The cast to float is intentional -- `cel_delta()` returns float, and `cels_step()` accepts float.

### Pattern 2: Event Pump System

**What:** A CELS system that drains all pending SDL events each frame.
**When to use:** Runs at OnLoad phase (earliest) every frame, before any game logic.
**Why OnLoad:** The existing `SDL3_WindowStateSystem` already runs at OnLoad. Event pumping must happen before window state processing. Use explicit ordering or a custom phase to ensure pump runs first.

```c
// Source: CELS cels_system_impl.h + SDL3 SDL_events.h
CEL_System(SDL3_EventPumpSystem, .phase = OnLoad) {
    cel_run {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                /* SDL_QUIT triggers immediate exit */
                cel_quit();
                return;
            }
            /* Route window events to window state machine (Phase 4 will expand this) */
            /* For Phase 3, minimal routing: handle SDL_EVENT_WINDOW_CLOSE_REQUESTED */
        }
    }
}
```

### Pattern 3: Minimized Pause with SDL_WaitEvent

**What:** When all windows are minimized, block on `SDL_WaitEvent` instead of spinning.
**When to use:** CONTEXT.md decision: "Pause when all windows are minimized -- loop blocks on SDL_WaitEvent, zero CPU usage."
**Implementation:** The event pump system checks if all windows are minimized. If so, it calls `SDL_WaitEvent()` to block until a waking event arrives, then processes that event normally.

```c
// Source: SDL3 wiki SDL_WaitEvent + project CONTEXT.md
static bool all_windows_minimized(void) {
    /* Query all SDL3_WindowComponent entities, return true if all MINIMIZED */
    /* Implementation uses cel_query/cel_each */
}

/* Inside event pump system: */
if (all_windows_minimized()) {
    SDL_Event event;
    if (SDL_WaitEvent(&event)) {
        /* Process the waking event normally */
        handle_event(&event);
    }
    return;  /* Skip normal poll loop this frame */
}
```

**Critical note:** `SDL_WaitEvent` blocks the thread. Since CELS runs on a single thread and this system runs inside `cels_step()`, the entire frame is paused. This is the desired behavior -- zero CPU when minimized.

### Pattern 4: Frame Rate Capping

**What:** Limit frame rate to target FPS when VSync is not active.
**When to use:** CONTEXT.md decision: "Configurable frame rate -- support both VSync and target FPS cap."
**Implementation:** After `cels_step()` completes, compute remaining time in frame budget and delay.

```c
// Source: SDL3 3.4.2 SDL_timer.h
static void sdl3_cap_frame_rate(Uint64 frame_start, int target_fps) {
    if (target_fps <= 0) return;  /* No cap / VSync mode */
    Uint64 frame_ns = SDL_NS_PER_SECOND / (Uint64)target_fps;
    Uint64 elapsed_ns = SDL_GetTicksNS() - frame_start;  /* or use perf counter */
    if (elapsed_ns < frame_ns) {
        SDL_DelayNS(frame_ns - elapsed_ns);  /* OS scheduler delay, low CPU */
    }
}
```

**Design choice:** Use `SDL_DelayNS` (not `SDL_DelayPrecise`) for frame rate capping. `SDL_DelayPrecise` busy-waits for precision but wastes CPU. For a game loop, `SDL_DelayNS` provides adequate precision with much lower CPU usage. The user's monitor VSync handles the precise timing when enabled.

### Pattern 5: Running State as CEL_State

**What:** A global state singleton tracking whether the frame loop should continue.
**When to use:** CONTEXT.md decision: "Loop running state is ECS-driven -- a component/state in the ECS that systems can read/write."

```c
// In cels_sdl3.h (public header)
CEL_Define_State(SDL3_FrameState) {
    bool running;
    float delta_time;
    float fps;
    float smoothed_fps;
};

// In consumer code
while (sdl3_should_run()) {
    cels_step(sdl3_delta());
}
// Or equivalently, reading the state:
const struct SDL3_FrameState* frame = cel_read(SDL3_FrameState);
while (frame->running) { ... }
```

### Pattern 6: FPS Smoothing (Exponential Moving Average)

**What:** Smooth FPS counter for debug overlays, avoiding jittery per-frame values.
**When to use:** CONTEXT.md decision: "Track and expose both delta time and smoothed FPS counter for debug overlays."
**Algorithm:** Exponential moving average with configurable smoothing factor.

```c
// Source: Standard EMA approach
static float smooth_fps(float current_fps, float prev_smoothed, float alpha) {
    /* alpha = 0.1 means 90% old value, 10% new value -- good smoothing */
    return alpha * current_fps + (1.0f - alpha) * prev_smoothed;
}

/* Usage each frame: */
float raw_fps = (dt > 0.0f) ? (1.0f / dt) : 0.0f;
state->smoothed_fps = smooth_fps(raw_fps, state->smoothed_fps, 0.1f);
state->fps = raw_fps;
```

### Pattern 7: Consumer Loop Shape

**What:** The consumer's main loop pattern per CONTEXT.md.
**Decided:** "Consumer owns the while loop (`while (running) { cels_step(session); }`)"
**Specific idea from CONTEXT.md:** `while (sdl3_should_run(session)) { cels_step(session); }` or equivalent.

The consumer loop integrates with delta time calculation:

```c
// Consumer pattern
cels_main() {
    cels_register(SDL3_Engine);
    /* consumer systems registered here */
    cels_session(World) {
        while (sdl3_should_run()) {
            float dt = sdl3_delta();
            cels_step(dt);
        }
    }
}
```

Where `sdl3_should_run()` checks `SDL3_FrameState.running` and `sdl3_delta()` computes delta time from performance counters.

### Anti-Patterns to Avoid

- **Passing 0 to cels_step:** The existing example uses `cels_step(0.016f)` which is a hardcoded approximation. Phase 3 replaces this with actual measured delta time.
- **SDL_Delay(16) for frame pacing:** The existing example uses `SDL_Delay(16)` for pacing. This is imprecise (millisecond resolution) and should be replaced with proper timing.
- **Computing delta inside a system:** Delta must be computed BEFORE `cels_step()` is called, not inside a system that runs during the step. Systems read delta via `cel_delta()`.
- **Polling events inside consumer code:** Events should be pumped by a registered CELS system, not by the consumer in their while loop. This keeps event handling inside the ECS pipeline.
- **Using SDL_GetTicks for delta time:** `SDL_GetTicks()` returns milliseconds (Uint64 in SDL3) which is much lower precision than `SDL_GetPerformanceCounter()`. Always use performance counters.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| High-resolution timing | Custom clock_gettime/QueryPerformanceCounter calls | `SDL_GetPerformanceCounter/Frequency` | Cross-platform, already linked, exact same thing |
| Nanosecond constants | `#define NS_PER_SEC 1000000000` | `SDL_NS_PER_SECOND`, `SDL_SECONDS_TO_NS`, etc. | SDL3 provides all conversion macros |
| Precise frame delay | Busy-loop with counter checking | `SDL_DelayPrecise(ns)` or `SDL_DelayNS(ns)` | SDL3 handles platform differences |
| Event queue drain | Manual `SDL_PumpEvents` + `SDL_PeekEvents` | `SDL_PollEvent` in a while loop | `SDL_PollEvent` implicitly pumps |
| Quit flag management | Custom bool + mutex | `cels_quit()` / `cels_running()` | CELS provides this mechanism |
| FPS smoothing | Complex weighted average with ring buffer | Simple exponential moving average (EMA) | EMA with alpha=0.1 is sufficient, no allocation needed |

**Key insight:** SDL3 provides a complete timing toolkit including nanosecond macros, performance counters, and precise delay functions. CELS provides the quit flag mechanism. The only custom code needed is the glue: computing dt from perf counters and passing it to `cels_step()`.

## Common Pitfalls

### Pitfall 1: First-Frame Delta Spike

**What goes wrong:** On the first frame, `s_last_counter` is 0, producing an enormous delta time (potentially seconds) that causes physics/animation to explode.
**Why it happens:** `SDL_GetPerformanceCounter()` returns an absolute counter since boot, not since app start.
**How to avoid:** Initialize `s_last_counter = SDL_GetPerformanceCounter()` during init (before the first frame), and return `0.0f` delta for the very first frame.
**Warning signs:** Objects teleporting or extreme values on frame 1.

### Pitfall 2: Integer Division in Delta Calculation

**What goes wrong:** `delta = (now - last) / frequency` produces 0 if all values are Uint64 (integer division truncates sub-second intervals to 0).
**Why it happens:** Both counter difference and frequency are Uint64. If the frame takes less than `1/frequency` seconds (always true for reasonable frame rates), integer division gives 0.
**How to avoid:** Cast to float/double BEFORE dividing: `(float)(now - last) / (float)frequency`.
**Warning signs:** `cel_delta()` always returns 0.0f.

### Pitfall 3: SDL_PollEvent on Wrong Thread

**What goes wrong:** `SDL_PollEvent` silently fails or causes undefined behavior when called from a non-main thread.
**Why it happens:** SDL3 requires event polling on the main thread (it implicitly calls `SDL_PumpEvents`).
**How to avoid:** All CELS systems run on the main thread in our single-threaded architecture, so this is already handled. Just don't spawn threads for event processing.
**Warning signs:** Events not arriving, mysterious hangs.

### Pitfall 4: SDL_WaitEvent Blocks Entire Frame

**What goes wrong:** Calling `SDL_WaitEvent` unconditionally freezes the application when no events arrive.
**Why it happens:** `SDL_WaitEvent` is a blocking call -- it does not return until an event is available.
**How to avoid:** Only call `SDL_WaitEvent` when explicitly paused (all windows minimized). During normal operation, use `SDL_PollEvent`.
**Warning signs:** Application freezes, appears to hang.

### Pitfall 5: Window Drag/Resize Blocks SDL_PollEvent on Windows

**What goes wrong:** On Windows, during window drag or resize operations, `SDL_PollEvent` blocks for significant time periods.
**Why it happens:** Windows OS quirk -- the modal drag/resize loop captures the event pump.
**How to avoid:** Awareness only -- SDL3 wiki documents this behavior. Delta time calculation handles this naturally (large dt on resume). No workaround needed for Phase 3.
**Warning signs:** Frame stalls during window resize on Windows platform.

### Pitfall 6: Double Event Processing

**What goes wrong:** Events get processed twice -- once in the event pump system and once by a consumer system reading the event queue.
**Why it happens:** Phase 3 pumps events minimally (just SDL_QUIT and window close). Phase 4 will add the full event buffer. If Phase 3 hard-codes event handling, Phase 4 must refactor.
**How to avoid:** Design the event pump to be extensible. In Phase 3, handle only SDL_EVENT_QUIT. Window events are already handled by `sdl3_window_handle_event()`. Phase 4 will add the raw event buffer.
**Warning signs:** Window close fires twice, quit happens unexpectedly.

### Pitfall 7: Forgetting to Cap Delta Time

**What goes wrong:** After a debugger pause or system sleep, delta time is enormous (30+ seconds), causing game state to explode.
**Why it happens:** Performance counter keeps ticking during debugger breakpoints.
**How to avoid:** Clamp delta time to a maximum (e.g., 0.25 seconds = 4 FPS minimum). This is a standard game loop pattern.
**Warning signs:** Explosive movement or NaN values after resuming from debugger.

### Pitfall 8: SDL_QUIT vs Window Close Confusion

**What goes wrong:** Closing a window does not quit the application, or quitting does not close windows.
**Why it happens:** `SDL_EVENT_WINDOW_CLOSE_REQUESTED` and `SDL_EVENT_QUIT` are separate events. By default (`SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE` = "1"), SDL sends `SDL_EVENT_QUIT` when the LAST window closes. But closing a non-last window only sends `SDL_EVENT_WINDOW_CLOSE_REQUESTED`.
**How to avoid:** Handle both: `SDL_EVENT_WINDOW_CLOSE_REQUESTED` transitions that window to CLOSING (existing code). `SDL_EVENT_QUIT` sets the running flag to false (new code). The frame state system also checks if all windows are CLOSED and triggers quit.
**Warning signs:** Application keeps running with no windows, or application quits when closing one of multiple windows.

## Code Examples

### Complete Delta Time Helper (Recommended Implementation)

```c
// Source: SDL3 3.4.2 SDL_timer.h, verified from header
// File: src/loop/sdl3_loop.c

#include <cels_sdl3.h>
#include <SDL3/SDL.h>

/* Delta time computation state (file-local) */
static Uint64 s_perf_frequency = 0;
static Uint64 s_last_counter = 0;
static bool   s_initialized = false;

/* Maximum delta clamp: 250ms (4 FPS minimum) */
#define SDL3_MAX_DELTA 0.25f

void sdl3_loop_init(void) {
    s_perf_frequency = SDL_GetPerformanceFrequency();
    s_last_counter = SDL_GetPerformanceCounter();
    s_initialized = true;
}

float sdl3_compute_delta(void) {
    if (!s_initialized) {
        sdl3_loop_init();
        return 0.0f;
    }
    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)(now - s_last_counter) / (float)s_perf_frequency;
    s_last_counter = now;

    /* Clamp to prevent explosion after debugger pause */
    if (dt > SDL3_MAX_DELTA) dt = SDL3_MAX_DELTA;
    if (dt < 0.0f) dt = 0.0f;  /* Guard against counter wraparound */

    return dt;
}
```

### FPS Tracking with Smoothing

```c
// Source: Standard EMA algorithm
// File: src/loop/sdl3_loop.c (continued)

#define SDL3_FPS_SMOOTHING 0.1f  /* 10% new, 90% old */

static float s_smoothed_fps = 0.0f;

float sdl3_compute_fps(float dt) {
    float raw_fps = (dt > 0.0001f) ? (1.0f / dt) : 0.0f;
    s_smoothed_fps = SDL3_FPS_SMOOTHING * raw_fps
                   + (1.0f - SDL3_FPS_SMOOTHING) * s_smoothed_fps;
    return s_smoothed_fps;
}
```

### Frame Rate Capping

```c
// Source: SDL3 3.4.2 SDL_timer.h
// File: src/loop/sdl3_loop.c (continued)

void sdl3_cap_frame_rate(Uint64 frame_start_ns, int target_fps) {
    if (target_fps <= 0) return;
    Uint64 target_ns = SDL_NS_PER_SECOND / (Uint64)target_fps;
    Uint64 elapsed_ns = SDL_GetTicksNS() - frame_start_ns;
    if (elapsed_ns < target_ns) {
        SDL_DelayNS(target_ns - elapsed_ns);
    }
}
```

### Event Pump System (Minimal for Phase 3)

```c
// Source: CELS cels_system_impl.h + SDL3 SDL_events.h
// File: src/sdl3_module.c (addition)

CEL_System(SDL3_EventPumpSystem, .phase = OnLoad) {
    cel_run {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                cel_quit();
                return;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            case SDL_EVENT_WINDOW_MINIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_RESIZED:
                /* Route to window state machine via existing handler */
                /* Need to find the window component by windowID */
                break;

            default:
                break;
            }
        }
    }
}
```

### Consumer Loop Pattern

```c
// Source: CELS cels.h + project CONTEXT.md decisions
// File: examples/frame-loop/main.c

cels_main() {
    cels_register(SDL3_Engine);
    cels_register(/* consumer systems */);
    cels_session(World) {
        while (sdl3_should_run()) {
            float dt = sdl3_delta();
            cels_step(dt);
        }
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `SDL_GetTicks()` (ms precision) | `SDL_GetPerformanceCounter/Frequency` (sub-us) | SDL2 era, still preferred in SDL3 | Use perf counters for delta time |
| `SDL_Delay(ms)` (ms granularity) | `SDL_DelayPrecise(ns)` / `SDL_DelayNS(ns)` | SDL 3.2.0 | Nanosecond-precision delay available |
| Manual ns constants | `SDL_NS_PER_SECOND`, `SDL_SECONDS_TO_NS()`, etc. | SDL 3.2.0 | Use provided macros |
| `SDL_GetTicks64()` | `SDL_GetTicks()` (already Uint64 in SDL3) | SDL 3.0 | Old name is a compat shim in `SDL_oldnames.h` |
| `event.window.event` subtype enum | Individual `SDL_EVENT_WINDOW_*` event types | SDL 3.0 | Each window event is its own `SDL_EventType` value |
| SDL2 `SDL_RENDERER_VSYNC` flag on creation | `SDL_SetRenderVSync(renderer, int)` post-creation | SDL 3.0 | VSync configured after renderer creation |

**Deprecated/outdated:**
- `SDL_GetTicks64()`: Renamed to `SDL_GetTicks()` in SDL3 (returns Uint64 natively)
- Window event subtypes: SDL2 had `event.window.event == SDL_WINDOWEVENT_CLOSE`. SDL3 uses `event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED` directly
- `SDL_RENDERER_VSYNC` creation flag: SDL3 uses `SDL_SetRenderVSync()` after creation (relevant for Phase 5, noted here for awareness)

## Open Questions

### 1. Event Pump vs Window State System Ordering

**What we know:** Both `SDL3_EventPumpSystem` and `SDL3_WindowStateSystem` run at `OnLoad` phase. Event pumping must happen before window state transitions (the pump generates the events that the state system consumes).
**What's unclear:** CELS system ordering within the same phase. Systems registered earlier MAY run first (Flecs default behavior), but this is not guaranteed by the CELS API.
**Recommendation:** Register `SDL3_EventPumpSystem` before `SDL3_WindowStateSystem` in the module init. If ordering is unreliable, create a custom phase `CEL_Phase(PreLoad, .after = OnLoad)` or use a separate phase. Alternatively, handle window events directly in the event pump system (route to `sdl3_window_handle_event` inline) and leave the window state system for the CLOSING->CLOSED transition only. This latter approach avoids the ordering problem entirely.

### 2. Window Event Routing Without Phase 4

**What we know:** Phase 4 (Input System) will add full event buffering and routing. Phase 3 needs minimal event handling to support the frame loop (SDL_QUIT, window close, minimize/restore for pause behavior).
**What's unclear:** How much event routing to implement in Phase 3 vs defer to Phase 4.
**Recommendation:** Phase 3 should implement just enough: SDL_EVENT_QUIT -> `cel_quit()`, and window events routed to `sdl3_window_handle_event()` inline in the pump system. Phase 4 will refactor to add the event buffer. This keeps Phase 3 focused on the frame loop.

### 3. CEL_State vs Helper Functions for Delta Time

**What we know:** CONTEXT.md says "delta time exposed via session parameter (not an ECS component)." The `cels_step(dt)` mechanism already makes delta available via `cel_delta()`.
**What's unclear:** Whether a separate `SDL3_FrameState` CEL_State is needed for the running flag + FPS tracking, or if simple helper functions (`sdl3_should_run()`, `sdl3_delta()`) suffice.
**Recommendation:** Use BOTH: `SDL3_FrameState` CEL_State for the running flag (so systems can read/write it via `cel_read`/`cel_mutate`), AND helper functions for the consumer loop that read from the state. Delta time flows through `cels_step(dt)` / `cel_delta()` so it does not need to be in the state, but FPS values do (for debug overlay access).

### 4. Per-TU Static ID Constraint for New Systems

**What we know:** Phase 1 decision: "All CELS declarations in single TU (sdl3_module.c) per per-TU static ID constraint." All `CEL_System`, `CEL_Lifecycle`, `CEL_Component`, etc. must be in sdl3_module.c.
**What's unclear:** Whether the new event pump system and any frame state system can practically live in sdl3_module.c without it becoming unwieldy.
**Recommendation:** Keep all CELS macro declarations in sdl3_module.c. Delegate implementation logic to helper functions in `src/loop/sdl3_loop.c` (same pattern as sdl3_init.c and sdl3_window.c). The systems in sdl3_module.c call these helpers.

## Sources

### Primary (HIGH confidence)
- SDL3 3.4.2 headers (locally fetched): `build/_deps/sdl3-src/include/SDL3/SDL_timer.h` -- timing APIs, constants, delay functions
- SDL3 3.4.2 headers (locally fetched): `build/_deps/sdl3-src/include/SDL3/SDL_events.h` -- event types, SDL_EVENT_QUIT, event struct timestamps
- SDL3 3.4.2 headers (locally fetched): `build/_deps/sdl3-src/include/SDL3/SDL_hints.h` -- `SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE`
- CELS 0.5.3 source: `../cels/include/cels/cels.h` -- cels_step, cels_running, cels_quit, cel_delta, CEL_State, cel_mutate
- CELS 0.5.3 source: `../cels/include/cels/private/cels_system_impl.h` -- CEL_System, cel_run, phase aliases, cel_delta implementation
- CELS 0.5.3 source: `../cels/src/cels_api.c` (line 1335) -- `cels_engine_progress` passes dt directly to `ecs_progress`

### Secondary (MEDIUM confidence)
- [SDL Wiki: SDL_GetPerformanceCounter](https://wiki.libsdl.org/SDL3/SDL_GetPerformanceCounter) -- API docs, thread safety confirmed
- [SDL Wiki: SDL_GetPerformanceFrequency](https://wiki.libsdl.org/SDL3/SDL_GetPerformanceFrequency) -- API docs, thread safety confirmed
- [SDL Wiki: SDL_PollEvent](https://wiki.libsdl.org/SDL3/SDL_PollEvent) -- API docs, main-thread-only requirement, Windows drag/resize quirk
- [SDL Wiki: SDL_WaitEvent](https://wiki.libsdl.org/SDL3/SDL_WaitEvent) -- API docs, blocking behavior confirmed
- [SDL Wiki: SDL_SetRenderVSync](https://wiki.libsdl.org/SDL3/SDL_SetRenderVSync) -- VSync constants, defaults to disabled
- [SDL Wiki: SDL_DelayPrecise](https://wiki.libsdl.org/SDL3/SDL_DelayPrecise) -- Busy-wait precision, CPU trade-off

### Tertiary (LOW confidence)
- [Lazy Foo: Frame Rate and VSync (SDL3)](https://lazyfoo.net/tutorials/SDL3/12-frame-rate-and-vsync/index.php) -- Community tutorial, frame rate capping patterns
- [SDL GitHub Issue #13062](https://github.com/libsdl-org/SDL/issues/13062) -- VSync + delayed events issue (platform-specific, noted for awareness)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- All APIs verified from locally fetched SDL3 3.4.2 headers and CELS 0.5.3 source
- Architecture: HIGH -- Patterns follow established cels-sdl3 conventions (sdl3_module.c + helper TUs) and CELS API verified from source
- Timing APIs: HIGH -- SDL_GetPerformanceCounter/Frequency, SDL_DelayPrecise/NS all verified from SDL3 headers
- Event loop: HIGH -- SDL_PollEvent, SDL_WaitEvent, SDL_EVENT_QUIT all verified from headers
- Pitfalls: MEDIUM -- Most are well-known game loop pitfalls, Windows drag/resize quirk from SDL wiki
- FPS smoothing: HIGH -- EMA is a textbook algorithm, no library dependency

**Research date:** 2026-03-15
**Valid until:** 2026-04-15 (30 days -- SDL3 and CELS APIs are stable)
