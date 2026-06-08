# cels-sdl3 - SDL3-based providers for CELS framework
# INTERFACE library: sources compile in the consumer's context
cmake_minimum_required(VERSION 3.21)
project(cels-sdl3 VERSION 0.2.1 LANGUAGES C)

# ============================================================================
# Dependencies
# ============================================================================
# cels-sdl3 is an addon library -- the consumer must provide cels.
# Typical consumer CMakeLists.txt:
#
#   FetchContent_Declare(cels
#       GIT_REPOSITORY https://github.com/42-Galaxies/cels.git
#       GIT_TAG        v0.5.3)
#   FetchContent_Declare(cels-sdl3
#       GIT_REPOSITORY https://github.com/42-Galaxies/cels-sdl3.git
#       GIT_TAG        v0.1.0)
#   FetchContent_MakeAvailable(cels cels-sdl3)
#
#   target_link_libraries(your_app PRIVATE cels cels-sdl3)
#
if(NOT TARGET cels)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        # Top-level build (development) -- use sibling cels directory
        # Point FetchContent at cels's already-downloaded deps to avoid network clones
        set(FETCHCONTENT_SOURCE_DIR_FLECS
            "${CMAKE_CURRENT_SOURCE_DIR}/../cels/cmake-build-debug/_deps/flecs-src" CACHE PATH "")
        set(FETCHCONTENT_SOURCE_DIR_YYJSON
            "${CMAKE_CURRENT_SOURCE_DIR}/../cels/cmake-build-debug/_deps/yyjson-src" CACHE PATH "")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cels cels)
    else()
        message(FATAL_ERROR
            "cels-sdl3 requires the 'cels' target. "
            "Add cels via FetchContent or add_subdirectory before cels-sdl3.")
    endif()
endif()

# SDL3 ecosystem -- FetchContent only (no system package fallback)
include(FetchContent)

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.2
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG        release-3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG        release-3.2.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3 SDL3_image SDL3_ttf)

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

target_link_libraries(cels-sdl3 INTERFACE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
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
