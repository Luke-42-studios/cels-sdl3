# cels-sdl3 tests -- only built when cels-sdl3 is the top-level CMake project.
#
# Tests are standalone executables using the utest.h single-header framework
# vendored in libs/cels/tests/vendor/.  Each test links cels-sdl3 directly
# (which transitively pulls in cels + SDL3 + flecs).
#
# DISPLAY REQUIREMENT: test_input_recompose exercises SDL event synthesis and
# requires SDL_INIT_EVENTS to be available.  It skips gracefully in headless
# CI via an SDL_Init() check at the start of each SDL-dependent test case.
# Run it manually with a working event subsystem (any Linux desktop session,
# or WSL2 with a display) to exercise the full regression net.

# This file is only included via add_subdirectory(tests) inside the
# if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR) guard in the root
# CMakeLists.txt, so there is no need for a separate top-level check here.

# Path to the utest.h vendor header (vendored once in sibling cels repo).
set(CELS_TEST_VENDOR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cels/tests/vendor"
    CACHE PATH "Path to cels test vendor headers (utest.h)")

# HYG-07 / ticket 040 recompose regression net.
# Links cels-sdl3 (which provides SDL3 + cels + flecs transitively).
add_executable(test_input_recompose
    "${CMAKE_CURRENT_SOURCE_DIR}/test_input_recompose.c")

target_link_libraries(test_input_recompose
    PRIVATE
        cels-sdl3
        flecs::flecs_static
        m)      # sqrtf in sdl3_actions.c (action vector magnitude)

target_include_directories(test_input_recompose
    PRIVATE
        "${CELS_TEST_VENDOR}")

set_target_properties(test_input_recompose
    PROPERTIES
        C_STANDARD     99
        C_STANDARD_REQUIRED ON)
