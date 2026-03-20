/*
 * SDL3 Input - Event Drain, Per-Window Queue Buffering, Window Event Routing
 *
 * Called by SDL3_InputSystem in sdl3_module.c each frame. Implements a
 * single-pass event drain: polls all SDL events, routes window events
 * to sdl3_window_handle_event, and buffers everything else into per-window
 * SDL3_EventQueue components matched by windowID.
 *
 * CANNOT use cel_query/cel_each/cel_update (per-TU static ID constraint).
 * Receives a pre-built window table from the system in sdl3_module.c.
 */

#include <cels_sdl3.h>
#include "../sdl3_internal.h"
#include <SDL3/SDL.h>

/* ============================================================================
 * Helpers
 * ============================================================================ */

/*
 * Push an event into a window's queue (bounds-checked).
 * On overflow: silently drop. 64 events per frame per window is generous --
 * typical frames generate 5-20 events. If this drops events, the capacity
 * constant can be increased.
 */
static void push_event(SDL3_EventQueue* queue, const SDL_Event* event)
{
    if (queue->count < SDL3_EVENT_QUEUE_CAPACITY) {
        queue->events[queue->count++] = *event;
    }
    /* else: overflow -- silently drop */
}

/*
 * Extract windowID from an SDL event. SDL3 input events carry a windowID
 * field identifying which window had focus. The field lives at different
 * offsets depending on the event type union member.
 *
 * Returns 0 for events that have no associated window (app lifecycle,
 * device add/remove, etc.).
 */
static SDL_WindowID get_event_window_id(const SDL_Event* event)
{
    switch (event->type) {
    /* Keyboard */
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        return event->key.windowID;

    /* Text input */
    case SDL_EVENT_TEXT_INPUT:
        return event->text.windowID;
    case SDL_EVENT_TEXT_EDITING:
        return event->edit.windowID;

    /* Mouse */
    case SDL_EVENT_MOUSE_MOTION:
        return event->motion.windowID;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return event->button.windowID;
    case SDL_EVENT_MOUSE_WHEEL:
        return event->wheel.windowID;

    /* Window events (should not reach here -- routed separately) */
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        return event->window.windowID;

    default:
        return 0;
    }
}

/*
 * Find the window table entry matching a given windowID.
 * Linear scan is appropriate: typically 1-3 windows.
 * Returns NULL if no match (stale windowID, or windowID == 0).
 */
static SDL3_WindowEntry* find_window(SDL3_WindowTable* table, SDL_WindowID id)
{
    if (id == 0) return NULL;
    for (int i = 0; i < table->count; i++) {
        if (table->entries[i].window_id == id) {
            return &table->entries[i];
        }
    }
    return NULL;
}

/*
 * Check if an event is a window lifecycle event that should be routed
 * to sdl3_window_handle_event and NOT enter the raw queue.
 */
static bool is_window_event(Uint32 type)
{
    switch (type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        return true;
    default:
        return false;
    }
}

/*
 * Route a single event: window events go to state machine, input events
 * go to per-window queue, quit triggers application exit.
 *
 * Returns true if SDL_EVENT_QUIT was received (caller should return).
 */
static bool route_event(SDL3_WindowTable* table, const SDL_Event* event)
{
    if (event->type == SDL_EVENT_QUIT) {
        cels_request_quit();
        return true;
    }

    if (is_window_event(event->type)) {
        /* Route to matching window's state machine */
        SDL3_WindowEntry* entry = find_window(table, event->window.windowID);
        if (entry && entry->comp) {
            sdl3_window_handle_event(
                entry->comp, event->type,
                event->window.data1, event->window.data2);
        }
        return false;
    }

    /* All other events: buffer into matching window's queue */
    SDL_WindowID wid = get_event_window_id(event);
    SDL3_WindowEntry* entry = find_window(table, wid);
    if (entry && entry->queue) {
        push_event(entry->queue, event);
    }
    /* else: no matching window (windowID == 0 or stale) -- drop silently */

    return false;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/*
 * Single-pass event drain. Called once per frame from SDL3_InputSystem.
 *
 * The caller (sdl3_module.c) has already:
 *   1. Cleared all queues (count = 0)
 *   2. Built the window table with mutable component pointers
 *
 * This function:
 *   Phase A: Check minimized-pause (block with SDL_WaitEvent if all minimized)
 *   Phase B: Normal poll loop (SDL_PollEvent drains all pending events)
 */
void sdl3_input_drain_events(SDL3_WindowTable* table)
{
    SDL_Event event;

    /* --- Phase A: Minimized pause --- */
    if (table->count > 0 && table->count == table->minimized_count) {
        /* All windows minimized -- block until waking event (zero CPU) */
        if (SDL_WaitEvent(&event)) {
            if (route_event(table, &event)) {
                return;  /* quit received */
            }
        }
        return;  /* Skip normal poll loop this frame */
    }

    /* --- Phase B: Normal poll loop --- */
    while (SDL_PollEvent(&event)) {
        if (route_event(table, &event)) {
            return;  /* quit received */
        }
    }
}
