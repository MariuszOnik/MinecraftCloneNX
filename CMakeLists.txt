cmake_minimum_required(VERSION 3.25)

project(VoxelGame VERSION 0.0.1 LANGUAGES C CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(VOXELGAME_BUILD_COMMIT "" CACHE STRING "Commit hash embedded in the build")

if(NOT VOXELGAME_BUILD_COMMIT AND DEFINED ENV{GITHUB_SHA})
    set(VOXELGAME_BUILD_COMMIT "$ENV{GITHUB_SHA}")
endif()

if(NOT VOXELGAME_BUILD_COMMIT)
    execute_process(
        COMMAND git rev-parse --verify HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE VOXELGAME_BUILD_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE VOXELGAME_GIT_RESULT
    )
    if(NOT VOXELGAME_GIT_RESULT EQUAL 0)
        set(VOXELGAME_BUILD_COMMIT "unknown")
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    set(VOXELGAME_PLATFORM "Nintendo Switch")
    set(PLATFORM "NX" CACHE STRING "raylib platform" FORCE)
else()
    set(VOXELGAME_PLATFORM "PC")
    set(PLATFORM "Desktop" CACHE STRING "raylib platform" FORCE)
endif()

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib_nx
    URL "https://github.com/luizpestana/raylib-nx/archive/aa030655796637f14d3e66cb9b64bef136ed4ef7.tar.gz"
    URL_HASH "SHA256=0a1ff18a6932395b60f3b11697873d79787f7d2af2d8a15e0f911aa54a1e8a09"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(raylib_nx)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_config.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/voxelgame/build_config.hpp"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build-info.txt.in"
    "${CMAKE_CURRENT_BINARY_DIR}/build-info.txt"
    @ONLY
)

add_library(voxelgame_core STATIC
    src/core/BuildInfo.cpp
    src/platform/Platform.cpp
)
target_include_directories(voxelgame_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

add_library(voxelgame_world STATIC
    src/world/Block.cpp
    src/world/ChunkMesher.cpp
    src/world/ChunkSection.cpp
)
target_include_directories(voxelgame_world PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(voxelgame_render STATIC src/render/ChunkRenderMesh.cpp)
target_include_directories(voxelgame_render PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(voxelgame_render PUBLIC voxelgame_world PRIVATE raylib)

add_executable(voxelgame src/app/main.cpp)
target_link_libraries(voxelgame PRIVATE voxelgame_core voxelgame_render raylib)

if(MINGW)
    target_link_options(voxelgame PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(MSVC)
    target_compile_options(voxelgame_core PRIVATE /W4 /permissive-)
    target_compile_options(voxelgame_world PRIVATE /W4 /permissive-)
    target_compile_options(voxelgame_render PRIVATE /W4 /permissive-)
    target_compile_options(voxelgame PRIVATE /W4 /permissive-)
else()
    target_compile_options(voxelgame_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(voxelgame_world PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(voxelgame_render PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(voxelgame PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    nx_generate_nacp(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/voxelgame.nacp"
        NAME "Voxel Game"
        AUTHOR "Voxel Game contributors"
        VERSION "${PROJECT_VERSION}"
    )
    nx_create_nro(
        voxelgame_nro
        TARGET voxelgame
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/voxelgame.nro"
        NACP "${CMAKE_CURRENT_BINARY_DIR}/voxelgame.nacp"
        ROMFS "${CMAKE_CURRENT_SOURCE_DIR}/romfs"
    )
else()
    install(TARGETS voxelgame RUNTIME DESTINATION .)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/build-info.txt" DESTINATION .)
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(voxelgame_core_tests tests/BuildInfoTests.cpp)
    target_link_libraries(voxelgame_core_tests PRIVATE voxelgame_core)
    if(MINGW)
        target_link_options(voxelgame_core_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()

    add_executable(voxelgame_world_tests tests/ChunkMesherTests.cpp)
    target_link_libraries(voxelgame_world_tests PRIVATE voxelgame_world)
    if(MINGW)
        target_link_options(voxelgame_world_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()

    add_test(NAME core.build_info COMMAND voxelgame_core_tests)
    add_test(NAME world.chunk_mesher COMMAND voxelgame_world_tests)
    add_test(NAME app.smoke COMMAND voxelgame --smoke-test)
endif()
