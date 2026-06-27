# cels-sdl3 - SDL3-based providers for CELS framework
# INTERFACE library: sources compile in the consumer's context
cmake_minimum_required(VERSION 3.21)
project(cels-sdl3 VERSION 0.2.2 LANGUAGES C)

# Vendor CPM — shared dependency cache: export CPM_SOURCE_CACHE=$HOME/.cache/CPM
if(NOT COMMAND CPMAddPackage)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)
endif()

# ============================================================================
# Dependencies
# ============================================================================
# cels-sdl3 is an addon library -- the consumer must provide cels.
# Typical consumer CMakeLists.txt:
#
#   CPMAddPackage(NAME cels      GITHUB_REPOSITORY Luke-42-studios/cels      GIT_TAG v0.7)
#   CPMAddPackage(NAME cels-sdl3 GITHUB_REPOSITORY Luke-42-studios/cels-sdl3 GIT_TAG master)
#
#   target_link_libraries(your_app PRIVATE cels cels-sdl3)
#
# Local-dev sibling override (replaces the trap):
#   cmake -S . -B build -DCPM_cels_SOURCE=../cels
#
# cels -- first-party dep; caller may override via -DCPM_cels_SOURCE=../cels
if(NOT TARGET cels)
    CPMAddPackage(
        NAME cels
        GITHUB_REPOSITORY Luke-42-studios/cels
        GIT_TAG v0.7
    )
endif()

# SDL3 ecosystem -- non-semver tags require explicit GIT_TAG (not gh: shorthand)
# Override locally: -DCPM_SDL3_SOURCE=<path>; shared cache: export CPM_SOURCE_CACHE=$HOME/.cache/CPM
#
# Emscripten provides SDL3/SDL3_image/SDL3_ttf via built-in ports; building from
# source conflicts with the port system. On WASM we skip the CPM fetch entirely
# and attach port flags so the consumer's link step activates the ports.
# On all other platforms the CPM path is byte-identical to what it was before.
if(NOT EMSCRIPTEN)
    CPMAddPackage(
        NAME SDL3
        GITHUB_REPOSITORY libsdl-org/SDL
        GIT_TAG release-3.4.2
    )
    CPMAddPackage(
        NAME SDL3_image
        GITHUB_REPOSITORY libsdl-org/SDL_image
        GIT_TAG release-3.4.0
    )
    CPMAddPackage(
        NAME SDL3_ttf
        GITHUB_REPOSITORY libsdl-org/SDL_ttf
        GIT_TAG release-3.2.2
    )
endif()

# ============================================================================
# INTERFACE library target
# ============================================================================
add_library(cels-sdl3 INTERFACE)

target_sources(cels-sdl3 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl3_module.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl3_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/loop/sdl3_loop.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/window/sdl3_window.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/input/sdl3_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/actions/sdl3_actions.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/sdl3_renderer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/sdl3_draw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/texture/sdl3_texture.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/text/sdl3_text.c
)

target_include_directories(cels-sdl3 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(EMSCRIPTEN)
    # Port-flag bypass: no from-source SDL3 fetch on WASM; ports are activated
    # via both compile and link flags:
    #   compile: tells emcc to expose the port's sysroot headers (SDL3/SDL.h etc.)
    #   link:    tells emcc to bundle the port libraries into the output
    target_compile_options(cels-sdl3 INTERFACE
        -sUSE_SDL=3
        -sUSE_SDL_IMAGE=3
        -sUSE_SDL_TTF=3
    )
    target_link_options(cels-sdl3 INTERFACE
        -sUSE_SDL=3
        -sUSE_SDL_IMAGE=3
        -sUSE_SDL_TTF=3
    )
else()
    target_link_libraries(cels-sdl3 INTERFACE
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf
    )
endif()
target_link_libraries(cels-sdl3 INTERFACE
    cels
    m
)

# ============================================================================
# Examples (only when building this repo directly)
# ============================================================================
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_executable(minimal examples/minimal/main.c)
    target_link_libraries(minimal PRIVATE cels-sdl3)
    set_target_properties(minimal PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

    add_executable(frame-loop examples/frame-loop/main.c)
    target_link_libraries(frame-loop PRIVATE cels-sdl3)
    set_target_properties(frame-loop PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

    add_executable(input examples/input/main.c)
    target_link_libraries(input PRIVATE cels-sdl3)
    set_target_properties(input PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

    add_executable(renderer examples/renderer/main.c)
    target_link_libraries(renderer PRIVATE cels-sdl3)
    set_target_properties(renderer PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

    add_executable(draw-primitives examples/draw-primitives/main.c)
    target_link_libraries(draw-primitives PRIVATE cels-sdl3)
    set_target_properties(draw-primitives PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

    add_executable(textures examples/textures/main.c)
    target_link_libraries(textures PRIVATE cels-sdl3)
    set_target_properties(textures PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

    # Copy texture assets to build directory so sdl3_set_asset_base_dir("assets") resolves
    add_custom_command(TARGET textures POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/textures/assets
        $<TARGET_FILE_DIR:textures>/assets
    )

    add_executable(text examples/text/main.c)
    target_link_libraries(text PRIVATE cels-sdl3)
    set_target_properties(text PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

    add_executable(demo examples/demo/main.c)
    target_link_libraries(demo PRIVATE cels-sdl3)
    set_target_properties(demo PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

    # Copy shared assets to build directory for demo and text examples
    add_custom_command(TARGET demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/assets
        $<TARGET_FILE_DIR:demo>/assets
    )

    # Tests (HYG-07 recompose reproducer and future additions)
    add_subdirectory(tests)
endif()
