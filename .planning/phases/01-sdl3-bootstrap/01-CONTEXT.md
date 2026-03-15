# Phase 1: SDL3 Bootstrap - Context

**Gathered:** 2026-03-15
**Status:** Ready for planning

<domain>
## Phase Boundary

CMake scaffold, SDL3 FetchContent acquisition, and init/shutdown lifecycle via CELS observers. Developer can build a project that links against CELS and SDL3, initializes all required subsystems, and shuts down cleanly. No windowing, no rendering, no input -- just the build system and lifecycle foundation.

</domain>

<decisions>
## Implementation Decisions

### Dependency strategy
- FetchContent only for SDL3, SDL3_image, and SDL3_ttf -- no system package fallback
- Pin all SDL3 dependencies to specific release tags, not branch tracking
- CELS framework also acquired via FetchContent from git
- C standard matches whatever CELS targets (check CELS CMakeLists.txt during research)

### Library target design
- INTERFACE library, matching cels-ncurses pattern exactly
- Sources added as INTERFACE_SOURCES, compiled in consumer context
- Register in CelsDeps.cmake in Phase 1 so `cels_require(sdl3)` works from the start
- SDL3 headers exposed to consumers (INTERFACE include directories) -- consumers can use SDL3 types directly

### Init/shutdown API
- Initialization triggered automatically via CELS lifecycle observers (OnCreate), not explicit init calls
- Shutdown triggered automatically via OnRemove/OnDestroy observer -- tied to ECS world lifecycle
- SDL3Context singleton ECS entity holds init state -- observable, queryable, standard ECS pattern
- On init failure, return error code -- consumer decides how to handle. Do not abort.

### Source layout
- Match cels-ncurses directory structure: include/ for public headers, src/ for implementation, subdirectories by domain
- SDL3_ prefix for all public API functions and types (e.g., SDL3_Engine_use(), SDL3_Context)
- Full scaffold in Phase 1: create all subdirectories and stub headers for future phases (window/, input/, graphics/, etc.)
- Standalone development build supported with sibling cels directory auto-detection, same as cels-ncurses

### Claude's Discretion
- Exact SDL3 version tags to pin (verify current stable releases during research)
- Specific subdirectory names and stub header contents
- CMakeLists.txt structure details beyond the INTERFACE pattern
- SDL3Context component field design

</decisions>

<specifics>
## Specific Ideas

- "Everything will be built into one library" -- single INTERFACE target, consumer gets everything via `cels_require(sdl3)`
- Follow cels-ncurses patterns closely: INTERFACE library, sibling detection, domain subdirectories
- Use CELS observers (OnCreate/OnDestroy) for lifecycle -- same declarative ECS approach as the rest of the framework
- Hot-swapping may be added later but is not a Phase 1 concern

</specifics>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 01-sdl3-bootstrap*
*Context gathered: 2026-03-15*
