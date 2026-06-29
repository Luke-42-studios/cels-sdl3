/*
 * SDL3 Input Actions -- typed-handle action map, polled query API.
 *
 * Translates SDL3 device state into a per-action SDL3_ActionState each frame,
 * keyed on entity components attached via the SDL3Action composition. Polled
 * queries read from a side-table mirror (sdl3_action_state) for O(N) lookup
 * over a small action count.
 *
 * Pipeline (all OnLoad, registered AFTER SDL3_Input_use so SDL events are
 * already drained):
 *
 *   1. SDL3_InputFrameSnapshotSystem
 *      Snapshots SDL keyboard/mouse/gamepad/quit into SDL3_InputFrame singleton.
 *   2. SDL3_InputActionResetSystem
 *      Zeros raw_strength/raw_vector on every (ActionConfig, ActionState) entity.
 *   3. One system per binding kind -- each OR/MAX/SUMs its contribution into
 *      raw_strength / raw_vector for actions whose context matches the active
 *      one (or whose context_name is NULL, meaning "global").
 *   4. SDL3_InputActionPhaseSystem
 *      Applies per-action deadzone (linear remap on strength, circular clip
 *      on vector), derives phase from prev_active vs current active flag,
 *      and mirrors the resolved state into a name-keyed side table.
 *
 * Context model
 *   Each action carries an optional context_name. SDL3InputContext writes its
 *   .name to SDL3_InputFrame.active_context iff .active=true; SDL3Action's
 *   composition body inherits the most-recently-composed context name from
 *   a file-scope static set by SDL3InputContext's body (good for the common
 *   "siblings: gameplay + menu" case -- contexts are NOT meant to nest in v1).
 *   sdl3_input_set_active_context() switches contexts at runtime by
 *   overwriting active_context.
 */

#include <cels_sdl3.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

CEL_State(SDL3_InputFrame);

/* The state data variable. Name must match the CEL_State name so
 * cels_state_bind(SDL3_InputFrame) resolves &SDL3_InputFrame correctly. */
static struct SDL3_InputFrame SDL3_InputFrame = {0};

/* Compose-time context inheritance: set by SDL3InputContext's body, read by
 * the SDL3Action body. Documented constraint: contexts do not nest in v1. */
static const char* s_compose_context = NULL;

/* Side-table mirror of resolved action states for O(N) name lookup from the
 * polled query path. Repopulated by the phase system each frame. */
#define SDL3_ACTIONS_MAX 64
typedef struct {
    char             name[64];
    SDL3_ActionState state;
} sdl3_resolved_t;
static sdl3_resolved_t s_resolved[SDL3_ACTIONS_MAX];
static int             s_resolved_count = 0;

/* Optional gamepad handle -- opened lazily on first connect event. */
static SDL_Gamepad* s_gamepad = NULL;
static bool         s_gamepad_subsys_init = false;

/* ============================================================================
 * Helpers
 * ============================================================================ */

/* The compare bound below must never exceed the backing array's size, or the
 * strncmp could over-read SDL3_InputFrame.active_context. */
_Static_assert(sizeof(((struct SDL3_InputFrame*)0)->active_context)
                   >= SDL3_INPUT_CONTEXT_NAME_MAX,
               "active_context buffer smaller than SDL3_INPUT_CONTEXT_NAME_MAX");

static bool action_context_active(const char* action_ctx)
{
    if (action_ctx == NULL || action_ctx[0] == '\0') return true;
    /* Compare only NAME_MAX-1 bytes: sdl3_input_set_active_context() truncates
     * the active context to NAME_MAX-1, so comparing the full NAME_MAX bytes
     * against an untruncated config name would make a long-named context never
     * match its own truncated active form — silently disabling all its actions.
     * Bounding the compare to the same NAME_MAX-1 prefix keeps the two sides
     * consistent. */
    return strncmp(action_ctx, SDL3_InputFrame.active_context,
                   SDL3_INPUT_CONTEXT_NAME_MAX - 1) == 0;
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float maxf(float a, float b) { return a > b ? a : b; }

static bool key_is_down(SDL_Keycode key)
{
    if (key == SDLK_UNKNOWN) return false;
    SDL_Scancode sc = SDL_GetScancodeFromKey(key, NULL);
    if (sc <= 0 || sc >= SDL_SCANCODE_COUNT) return false;
    return SDL3_InputFrame.keys_now[sc];
}

static void ensure_gamepad_open(void)
{
    if (s_gamepad != NULL) return;
    if (!s_gamepad_subsys_init) {
        if (!SDL_WasInit(SDL_INIT_GAMEPAD)) {
            if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) return;
        }
        s_gamepad_subsys_init = true;
    }
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids && count > 0) {
        s_gamepad = SDL_OpenGamepad(ids[0]);
    }
    if (ids) SDL_free(ids);
}

static void close_gamepad_if_disconnected(void)
{
    if (s_gamepad == NULL) return;
    if (!SDL_GamepadConnected(s_gamepad)) {
        SDL_CloseGamepad(s_gamepad);
        s_gamepad = NULL;
    }
}

static void resolved_clear(void) { s_resolved_count = 0; }

static void resolved_push(const char* name, const SDL3_ActionState* st)
{
    if (s_resolved_count >= SDL3_ACTIONS_MAX || name == NULL) return;
    sdl3_resolved_t* slot = &s_resolved[s_resolved_count++];
    size_t n = strlen(name);
    if (n >= sizeof(slot->name)) n = sizeof(slot->name) - 1;
    memcpy(slot->name, name, n);
    slot->name[n] = '\0';
    slot->state = *st;
}

/* ============================================================================
 * Public polled API
 * ============================================================================ */

const SDL3_ActionState* sdl3_action_state(const cels_action_t* handle)
{
    if (handle == NULL || handle->name == NULL) return NULL;
    for (int i = 0; i < s_resolved_count; i++) {
        if (strcmp(s_resolved[i].name, handle->name) == 0) {
            return &s_resolved[i].state;
        }
    }
    return NULL;
}

void sdl3_input_set_active_context(const char* name)
{
    if (name == NULL) name = "";
    size_t n = strlen(name);
    if (n >= SDL3_INPUT_CONTEXT_NAME_MAX) n = SDL3_INPUT_CONTEXT_NAME_MAX - 1;
    memcpy(SDL3_InputFrame.active_context, name, n);
    SDL3_InputFrame.active_context[n] = '\0';
}

const char* sdl3_input_active_context(void)
{
    return SDL3_InputFrame.active_context;
}

bool sdl3_input_quit_requested(void)
{
    return SDL3_InputFrame.quit_requested;
}

bool sdl3_key_held(SDL_Keycode key)
{
    return key_is_down(key);
}

bool sdl3_key_pressed(SDL_Keycode key)
{
    if (key == SDLK_UNKNOWN) return false;
    SDL_Scancode sc = SDL_GetScancodeFromKey(key, NULL);
    if (sc <= 0 || sc >= SDL_SCANCODE_COUNT) return false;
    return SDL3_InputFrame.keys_now[sc] && !SDL3_InputFrame.keys_prev[sc];
}

bool sdl3_key_released(SDL_Keycode key)
{
    if (key == SDLK_UNKNOWN) return false;
    SDL_Scancode sc = SDL_GetScancodeFromKey(key, NULL);
    if (sc <= 0 || sc >= SDL_SCANCODE_COUNT) return false;
    return !SDL3_InputFrame.keys_now[sc] && SDL3_InputFrame.keys_prev[sc];
}

float sdl3_mouse_x(void) { return SDL3_InputFrame.mouse_x; }
float sdl3_mouse_y(void) { return SDL3_InputFrame.mouse_y; }

bool sdl3_mouse_held(int button)
{
    return (SDL3_InputFrame.mouse_btns_now & SDL_BUTTON_MASK(button)) != 0;
}

bool sdl3_mouse_pressed(int button)
{
    Uint32 m = SDL_BUTTON_MASK(button);
    return (SDL3_InputFrame.mouse_btns_now & m) && !(SDL3_InputFrame.mouse_btns_prev & m);
}

bool sdl3_mouse_released(int button)
{
    Uint32 m = SDL_BUTTON_MASK(button);
    return !(SDL3_InputFrame.mouse_btns_now & m) && (SDL3_InputFrame.mouse_btns_prev & m);
}

/* ============================================================================
 * Systems
 * ============================================================================ */

/* 1. Snapshot SDL state into the InputFrame singleton. */
CEL_System(SDL3_InputFrameSnapshotSystem, .phase = OnLoad) {
    cel_run {
        memcpy(SDL3_InputFrame.keys_prev, SDL3_InputFrame.keys_now,
               sizeof(SDL3_InputFrame.keys_now));

        int numkeys = 0;
        const bool* keys = SDL_GetKeyboardState(&numkeys);
        if (keys) {
            int n = numkeys;
            if (n > SDL_SCANCODE_COUNT) n = SDL_SCANCODE_COUNT;
            memcpy(SDL3_InputFrame.keys_now, keys, (size_t)n * sizeof(bool));
        }

        SDL3_InputFrame.mouse_btns_prev = SDL3_InputFrame.mouse_btns_now;
        SDL3_InputFrame.mouse_btns_now = SDL_GetMouseState(
            &SDL3_InputFrame.mouse_x, &SDL3_InputFrame.mouse_y);

        SDL3_InputFrame.quit_requested = cels_should_quit();

        close_gamepad_if_disconnected();
        ensure_gamepad_open();
    }
}

/* 2a. Clear the side-table mirror once per frame.
 *
 * The phase system below uses cel_each, which flecs invokes ONCE PER
 * matching archetype. If we cleared the table at the top of the phase
 * system, every archetype invocation would wipe the entries the previous
 * invocation pushed -- polled queries would see only the LAST archetype's
 * actions. Using a separate cel_run system guarantees a single clear per
 * frame, independent of archetype layout. */
CEL_System(SDL3_InputResolvedClearSystem, .phase = OnLoad) {
    cel_run { resolved_clear(); }
}

/* 2b. Reset per-action accumulators (runs per archetype, that's fine -- it
 * mutates the action's own state, not a shared resource). */
CEL_System(SDL3_InputActionResetSystem, .phase = OnLoad) {
    cel_query(SDL3_ActionConfig, SDL3_ActionState);
    cel_each(SDL3_ActionConfig, SDL3_ActionState) {
        (void)SDL3_ActionConfig;
        cel_update(SDL3_ActionState) {
            SDL3_ActionState->raw_strength = 0.0f;
            SDL3_ActionState->raw_vector.x = 0.0f;
            SDL3_ActionState->raw_vector.y = 0.0f;
        }
    }
}

/* OQ-1: Multi-binding strength combination — MAX, not SUM.
 *
 * When multiple bindings are attached to the same action entity (e.g. both
 * SDLK_SPACE and SDL_GAMEPAD_BUTTON_SOUTH bound to "jump"), each binding
 * system writes:
 *
 *   raw_strength = maxf(raw_strength, <this_binding_contribution>)
 *
 * This means holding keyboard + gamepad simultaneously keeps raw_strength at
 * 1.0, not 2.0.  The same maxf pattern applies in every binary binding system
 * (3a-3c below) and the analog systems (3d-3g).  There is no double-counting.
 *
 * The multi-binding OR-strength / MAX-magnitude / circular-clipped-vector-sum
 * contract is documented in nucleus_input.h's polled-API doc block. */

/* 3a. Keyboard binding -> binary strength. */
CEL_System(SDL3_InputKeyBindingSystem, .phase = OnLoad) {
    cel_query(SDL3_ActionConfig, SDL3_ActionState, SDL3_KeyBinding);
    cel_each(SDL3_ActionConfig, SDL3_ActionState, SDL3_KeyBinding) {
        if (!action_context_active(SDL3_ActionConfig->context_name)) continue;
        if (!key_is_down(SDL3_KeyBinding->key)) continue;
        cel_update(SDL3_ActionState) {
            SDL3_ActionState->raw_strength = maxf(SDL3_ActionState->raw_strength, 1.0f);
        }
    }
}

/* 3b. Mouse button binding -> binary strength.
 * SDL3 mouse button numbering: 1=L, 2=M, 3=R (SDL_BUTTON_LEFT/MIDDLE/RIGHT). */
CEL_System(SDL3_InputMouseBtnBindingSystem, .phase = OnLoad) {
    cel_query(SDL3_ActionConfig, SDL3_ActionState, SDL3_MouseBtnBinding);
    cel_each(SDL3_ActionConfig, SDL3_ActionState, SDL3_MouseBtnBinding) {
        if (!action_context_active(SDL3_ActionConfig->context_name)) continue;
        Uint32 mask = SDL_BUTTON_MASK(SDL3_MouseBtnBinding->button);
        if ((SDL3_InputFrame.mouse_btns_now & mask) == 0) continue;
        cel_update(SDL3_ActionState) {
            SDL3_ActionState->raw_strength = maxf(SDL3_ActionState->raw_strength, 1.0f);
        }
    }
}

/* 3c. Gamepad button binding -> binary strength. */
CEL_System(SDL3_InputPadButtonBindingSystem, .phase = OnLoad) {
    cel_query(SDL3_ActionConfig, SDL3_ActionState, SDL3_PadButtonBinding);
    cel_each(SDL3_ActionConfig, SDL3_ActionState, SDL3_PadButtonBinding) {
        if (!action_context_active(SDL3_ActionConfig->context_name)) continue;
        if (s_gamepad == NULL) continue;
        if (!SDL_GetGamepadButton(s_gamepad, (SDL_GamepadButton)SDL3_PadButtonBinding->button))
            continue;
        cel_update(SDL3_ActionState) {
            SDL3_ActionState->raw_strength = maxf(SDL3_ActionState->raw_strength, 1.0f);
        }
    }
}

/* 3d. Gamepad analog axis (trigger / single-axis) -> abs strength. */
CEL_System(SDL3_InputPadAxisBindingSystem, .phase = OnLoad) {
    cel_query(SDL3_ActionConfig, SDL3_ActionState, SDL3_PadAxisBinding);
    cel_each(SDL3_ActionConfig, SDL3_ActionState, SDL3_PadAxisBinding) {
        if (!action_context_active(SDL3_ActionConfig->context_name)) continue;
        if (s_gamepad == NULL) continue;
        Sint16 raw = SDL_GetGamepadAxis(s_gamepad,
            (SDL_GamepadAxis)SDL3_PadAxisBinding->axis);
        float v = (float)raw / 32767.0f;
        if (v < 0) v = -v;
        cel_update(SDL3_ActionState) {
            SDL3_ActionState->raw_strength = maxf(SDL3_ActionState->raw_strength, v);
        }
    }
}

/* 3e. Gamepad analog stick -> 2D vector contribution. */
CEL_System(SDL3_InputPadStickBindingSystem, .phase = OnLoad) {
    cel_query(SDL3_ActionConfig, SDL3_ActionState, SDL3_PadStickBinding);
    cel_each(SDL3_ActionConfig, SDL3_ActionState, SDL3_PadStickBinding) {
        if (!action_context_active(SDL3_ActionConfig->context_name)) continue;
        if (s_gamepad == NULL) continue;
        SDL_GamepadAxis ax_x = SDL3_PadStickBinding->stick == 0
            ? SDL_GAMEPAD_AXIS_LEFTX : SDL_GAMEPAD_AXIS_RIGHTX;
        SDL_GamepadAxis ax_y = SDL3_PadStickBinding->stick == 0
            ? SDL_GAMEPAD_AXIS_LEFTY : SDL_GAMEPAD_AXIS_RIGHTY;
        float vx = (float)SDL_GetGamepadAxis(s_gamepad, ax_x) / 32767.0f;
        float vy = (float)SDL_GetGamepadAxis(s_gamepad, ax_y) / 32767.0f;
        cel_update(SDL3_ActionState) {
            SDL3_ActionState->raw_vector.x += vx;
            SDL3_ActionState->raw_vector.y += vy;
        }
    }
}

/* 3f. Composite scalar axis (two keys -> -1..+1). */
CEL_System(SDL3_InputAxisBindingSystem, .phase = OnLoad) {
    cel_query(SDL3_ActionConfig, SDL3_ActionState, SDL3_AxisBinding);
    cel_each(SDL3_ActionConfig, SDL3_ActionState, SDL3_AxisBinding) {
        if (!action_context_active(SDL3_ActionConfig->context_name)) continue;
        float v = 0.0f;
        if (key_is_down(SDL3_AxisBinding->negative_key)) v -= 1.0f;
        if (key_is_down(SDL3_AxisBinding->positive_key)) v += 1.0f;
        float abs_v = v < 0 ? -v : v;
        cel_update(SDL3_ActionState) {
            SDL3_ActionState->raw_strength = maxf(SDL3_ActionState->raw_strength, abs_v);
        }
    }
}

/* 3g. Composite 2D key axis (WASD + optional secondary set) -> 2D vector.
 * Both key sets feed the same vector axes; either set held in a given
 * direction contributes -1/+1 once. Set the secondary keys to SDLK_UNKNOWN
 * (the default zero-init value) to leave them unused. */
CEL_System(SDL3_InputKey2DAxisBindingSystem, .phase = OnLoad) {
    cel_query(SDL3_ActionConfig, SDL3_ActionState, SDL3_Key2DAxisBinding);
    cel_each(SDL3_ActionConfig, SDL3_ActionState, SDL3_Key2DAxisBinding) {
        if (!action_context_active(SDL3_ActionConfig->context_name)) continue;
        float dx = 0.0f, dy = 0.0f;
        if (key_is_down(SDL3_Key2DAxisBinding->neg_x)  ||
            key_is_down(SDL3_Key2DAxisBinding->neg_x2)) dx -= 1.0f;
        if (key_is_down(SDL3_Key2DAxisBinding->pos_x)  ||
            key_is_down(SDL3_Key2DAxisBinding->pos_x2)) dx += 1.0f;
        if (key_is_down(SDL3_Key2DAxisBinding->neg_y)  ||
            key_is_down(SDL3_Key2DAxisBinding->neg_y2)) dy -= 1.0f;
        if (key_is_down(SDL3_Key2DAxisBinding->pos_y)  ||
            key_is_down(SDL3_Key2DAxisBinding->pos_y2)) dy += 1.0f;
        cel_update(SDL3_ActionState) {
            SDL3_ActionState->raw_vector.x += dx;
            SDL3_ActionState->raw_vector.y += dy;
        }
    }
}

/* OQ-2: Deadzone remap for binary inputs (keyboard, mouse button, pad button).
 *
 * The cooked strength formula (Godot-compatible) is:
 *
 *   s = rs < dz ? 0.0f : (rs - dz) / (1.0f - dz)
 *
 * where rs = raw_strength and dz = deadzone (default 0.2f per OQ below).
 *
 * For binary inputs (keyboard, mouse button, pad button): raw_strength = 1.0f.
 * With the default dz = 0.2f:
 *
 *   s = (1.0f - 0.2f) / (1.0f - 0.2f) = 1.0f
 *
 * Binary inputs produce cooked strength 1.0 regardless of the deadzone value
 * (as long as dz < 1.0).  The deadzone only attenuates analog inputs whose
 * raw_strength falls between 0 and 1 — it removes hardware-drift noise near
 * the zero crossing.  A raw_strength < dz (e.g. a stick resting at 0.1) is
 * zeroed; raw_strength above dz is linearly remapped to [0, 1].
 *
 * OQ-3: Circular deadzone applied to raw_vector regardless of binding source.
 *
 * The phase system applies the same circular deadzone to SDL3_ActionState's
 * raw_vector whether the contribution came from a pad stick (SDL3_PadStickBinding)
 * or a keyboard composite (SDL3_Key2DAxisBinding / WASD).
 *
 * Keyboard-composite diagonal (W + D both held): dx=1, dy=1 → |v|=1.414.
 * The unit-circle clip (mag > 1.0f → vx /= mag; vy /= mag) brings it to
 * |v|=1.0 BEFORE the deadzone remap.  So nucleus_action_vec2() returns exactly
 * |v|=1.0 for diagonals — checklist item 4 in the input-demo.  The deadzone
 * remap then scales the magnitude with the same (mag - dz)/(1 - dz) formula
 * used for scalars.  The direction vector is preserved; only the magnitude
 * changes. */

/* 4. Apply deadzone + derive phase + mirror to side table.
 *
 * Deadzone math (Godot-compatible):
 *   strength    = raw_strength < dz ? 0 : (raw_strength - dz) / (1 - dz)
 *   vector:     compute magnitude of raw_vector (clamped to unit circle);
 *               apply same linear remap on magnitude to get final magnitude;
 *               rescale raw_vector by (final_mag / raw_mag). */
CEL_System(SDL3_InputActionPhaseSystem, .phase = OnLoad) {
    cel_query(SDL3_ActionConfig, SDL3_ActionState);
    cel_each(SDL3_ActionConfig, SDL3_ActionState) {
        cel_update(SDL3_ActionState) {
            float dz = SDL3_ActionConfig->deadzone > 0.0f
                ? SDL3_ActionConfig->deadzone : 0.2f;

            /* Scalar strength: linear remap above deadzone, clamp [0,1]. */
            float rs = clampf(SDL3_ActionState->raw_strength, 0.0f, 1.0f);
            float s  = rs < dz ? 0.0f : (rs - dz) / (1.0f - dz);
            SDL3_ActionState->raw_strength = rs;
            SDL3_ActionState->strength     = clampf(s, 0.0f, 1.0f);

            /* 2D vector: clamp raw to unit circle, apply circular deadzone,
             * rescale to final magnitude. */
            float vx = SDL3_ActionState->raw_vector.x;
            float vy = SDL3_ActionState->raw_vector.y;
            float mag = sqrtf(vx * vx + vy * vy);
            if (mag > 1.0f) {
                /* Clamp raw_vector to unit circle so downstream "raw" reads
                 * never exceed magnitude 1 either. */
                vx /= mag; vy /= mag;
                mag = 1.0f;
            }
            SDL3_ActionState->raw_vector.x = vx;
            SDL3_ActionState->raw_vector.y = vy;

            if (mag < dz) {
                SDL3_ActionState->vector.x = 0.0f;
                SDL3_ActionState->vector.y = 0.0f;
            } else {
                float final_mag = (mag - dz) / (1.0f - dz);
                float scale = final_mag / mag;
                SDL3_ActionState->vector.x = vx * scale;
                SDL3_ActionState->vector.y = vy * scale;
            }

            /* Derive phase from prev_active + current active flag.
             * Active = "has any signal this frame": strength > 0 OR vector nonzero. */
            bool active_now = SDL3_ActionState->strength > 0.0f ||
                              SDL3_ActionState->vector.x != 0.0f ||
                              SDL3_ActionState->vector.y != 0.0f;

            if (active_now && !SDL3_ActionState->prev_active) {
                SDL3_ActionState->phase = SDL3_ACTION_PRESSED;
            } else if (active_now && SDL3_ActionState->prev_active) {
                SDL3_ActionState->phase = SDL3_ACTION_HELD;
            } else if (!active_now && SDL3_ActionState->prev_active) {
                SDL3_ActionState->phase = SDL3_ACTION_RELEASED;
            } else {
                SDL3_ActionState->phase = SDL3_ACTION_IDLE;
            }
            SDL3_ActionState->prev_active = active_now;

            /* Mirror to side-table for polled lookup. */
            resolved_push(SDL3_ActionConfig->name, SDL3_ActionState);
        }
    }
}

/* ============================================================================
 * Compositions
 * ============================================================================ */

CEL_Composition(SDL3Action) {
    /* cels_composition_engine_register_root calls the factory once with
     * NULL props at registration time (cel.handle == NULL). Skip the body
     * so we don't leave a phantom entity with NULL config polluting every
     * (Config, State) query forever. Pair with stable `.id = "..."` on
     * every call site so cels reconciliation preserves the action entity
     * (and its phase-state machine) across parent recomposition. */
    if (cel.handle == NULL) return;

    cels_entity_t self = cels_get_current_entity();

    /* cel_has overwrites component data on every call. Unguarded
     * cel_has(SDL3_ActionState, .phase = IDLE) on every recomposition
     * would wipe prev_active and break the pressed/held/released edge
     * detection. Attach only on first composition; subsequent recompose
     * passes (reconciler reuses the entity by .id) leave the state
     * untouched. Same logic for ActionConfig so the deadzone tuning
     * isn't re-defaulted mid-game. */
    cels_ensure_component(&SDL3_ActionConfig_id, "SDL3_ActionConfig",
                          sizeof(SDL3_ActionConfig), CELS_ALIGNOF(SDL3_ActionConfig));
    if (cels_entity_get_component(self, SDL3_ActionConfig_id) == NULL) {
        cel_has(SDL3_ActionConfig,
            .name         = cel.handle->name,
            .context_name = s_compose_context,
            .deadzone     = cel.deadzone);
    }

    cels_ensure_component(&SDL3_ActionState_id, "SDL3_ActionState",
                          sizeof(SDL3_ActionState), CELS_ALIGNOF(SDL3_ActionState));
    if (cels_entity_get_component(self, SDL3_ActionState_id) == NULL) {
        cel_has(SDL3_ActionState, .phase = SDL3_ACTION_IDLE);
    }
}

CEL_Composition(SDL3InputContext) {
    if (cel.name == NULL) return;  /* phantom-registration guard */

    /* Option B (HYG-07 / ticket 040): fire a hard error when this composition
     * is entered during a recomposition pass.  The recompose path re-runs this
     * body, which resets s_compose_context and may re-register bindings --
     * silently breaking edge-detection for every action in the context (symptoms
     * 1-3 in ticket 040).
     *
     * Workaround: put the input map in a parent composition that NEVER calls
     * cel_watch (so it is never dirty-queued for recompose), and let the
     * reactive UI live in a cel_watch-ing child.  See the TodoApp / TodoList
     * split in examples/todo/main.c and the doc block in nucleus_input.h. */
    /* A hot-reload rebinds the root factory and recomposes the WHOLE root subtree
     * (the input map included), so this body is legitimately re-entered during a
     * reload recompose -- re-running it simply rewires the input map to the new
     * code. Only a REACTIVE (cel_watch-driven) recompose indicates the misuse this
     * guard targets, so suppress the error during a reload recompose. */
    if (cels_is_recomposing() && !cels_is_reloading()) {
        cels_error_raise(CELS_ERROR_ILLEGAL_OPERATION,
            "NucleusInputContext cannot live inside a composition that "
            "cel_watches state. Split: input map in a parent (no cel_watch), "
            "reactive UI in a child. See examples/todo/main.c TodoApp/TodoList "
            "and nucleus_input.h.");
    }

    /* The manual lifecycle-root tag that used to live here was the 9th (last)
     * in-body site from ticket 035.  It was removed because the _cel_compose_impl
     * macro (plan 01-01) now auto-tags CELS_LIFECYCLED_ROOT when no .lifecycle is
     * declared at the call site.  The SDL3Action site was removed in plan 01-02. */

    cel_has(SDL3_InputContext, .name = cel.name, .active = cel.active);

    /* Track compose-time context so SDL3Action calls inside this
     * composition's body block (the user's { ... } trailing this cel_init)
     * inherit it. v1 constraint: contexts do NOT nest -- the last
     * assignment wins for subsequent SDL3Action calls. */
    s_compose_context = cel.name;

    if (cel.active) {
        sdl3_input_set_active_context(cel.name);
    }
}

/* ============================================================================
 * Module registration
 * ============================================================================ */

void SDL3_InputActions_use(void)
{
    static bool registered = false;
    if (registered) return;
    registered = true;

    /* Singleton -- register + bind data pointer for cross-TU cel_read. */
    cels_register(SDL3_InputFrame);
    cels_state_bind(SDL3_InputFrame);

    /* Components -- cels_register on components is a no-op; ids resolve on
     * first cel_has/cel_query via cels_ensure_component (which dedups by
     * name through ecs_lookup, so all TUs share the same id). */
    cels_register(SDL3_ActionConfig, SDL3_ActionState, SDL3_KeyBinding);
    cels_register(SDL3_MouseBtnBinding, SDL3_PadButtonBinding,
                  SDL3_PadAxisBinding, SDL3_PadStickBinding);
    cels_register(SDL3_AxisBinding, SDL3_Key2DAxisBinding, SDL3_InputContext);

    /* Systems -- registration order within OnLoad determines execution order.
     * Snapshot first; per-action reset + per-frame side-table clear before
     * binding systems; phase last (mirrors to the now-empty side table). */
    cels_register(SDL3_InputFrameSnapshotSystem,
                  SDL3_InputResolvedClearSystem,
                  SDL3_InputActionResetSystem,
                  SDL3_InputKeyBindingSystem);
    cels_register(SDL3_InputMouseBtnBindingSystem,
                  SDL3_InputPadButtonBindingSystem,
                  SDL3_InputPadAxisBindingSystem,
                  SDL3_InputPadStickBindingSystem);
    cels_register(SDL3_InputAxisBindingSystem,
                  SDL3_InputKey2DAxisBindingSystem,
                  SDL3_InputActionPhaseSystem);

    /* Compositions. */
    cels_register(SDL3Action, SDL3InputContext);
}
