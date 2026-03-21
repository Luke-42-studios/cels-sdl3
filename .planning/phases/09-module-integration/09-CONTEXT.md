# Phase 9: Module Integration - Context

**Gathered:** 2026-03-21
**Status:** Ready for planning

<domain>
## Phase Boundary

Bundle all SDL3 providers into a single engine module with a la carte registration, correct ordered cleanup, and error reporting. Individual providers remain independently registrable. No new rendering or input capabilities — this phase organizes what exists.

</domain>

<decisions>
## Implementation Decisions

### A la carte granularity
- Explicit registration only — no auto-dependency pulling. SDL3_Window_use() does NOT auto-register Init or FrameLoop
- Bootstrap alone (SDL3_Init_use()) is a valid minimum unit — useful for headless or custom setups
- Missing dependency errors surface at runtime (natural ECS behavior), not at registration time
- One _use() function per phase: Init, Window, FrameLoop, Input, Renderer, Textures, Text (~7 functions)
- All registration must happen before the first frame — no late/dynamic registration

### Error reporting
- Callback-based: developer registers an error handler function
- Default handler prints to stderr if no callback registered (errors never silently swallowed)
- Callback receives context string (operation name, e.g., "create_renderer") plus SDL error string
- Callback is informational only — fire-and-forget, no control flow influence. cels-sdl3 decides recovery

### Cleanup behavior
- Automatic teardown when all windows close (continues existing behavior)
- Hardcoded reverse-dependency order: textures/fonts -> renderers -> windows -> SDL_Quit
- Best-effort cleanup: if one resource fails to destroy, log error via callback and continue destroying remaining resources
- Per-window resources (renderer, textures) clean up immediately when that window closes, not deferred to final shutdown
- Silent cleanup — no destruction logging unless errors occur (which go through error callback)
- No pre-shutdown hooks — developers write their own systems for custom teardown
- No leak detection — developers use external tools (Valgrind, ASan)

### Registration API feel
- All-in-one: SDL3_use(&config) — registers all providers with a config struct
- Individual: SDL3_Window_use(), SDL3_Input_use(), SDL3_Renderer_use(), etc. (PascalCase + _use())
- Config struct contains: error handler callback, SDL subsystem flags, log verbosity

### Claude's Discretion
- Config struct field types and defaults
- Internal registration implementation (CEL_DefineModule vs manual wiring)
- System ordering within the module
- How individual _use() functions interact with the config struct (inherit global config?)

</decisions>

<specifics>
## Specific Ideas

- SDL3_use() as the short, clean all-in-one name (not SDL3_Engine_use)
- Error callback signature: context + message pattern (e.g., `void (*handler)(const char *context, const char *message)`)
- Zero-arg individual _use() functions for simplicity — config only on the all-in-one

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 09-module-integration*
*Context gathered: 2026-03-21*
