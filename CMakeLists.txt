cmake_minimum_required(VERSION 3.25)

project(VoxelGame VERSION 0.0.1 LANGUAGES C CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(VOXELGAME_BUILD_COMMIT "" CACHE STRING
    "Commit hash embedded in the build (empty = auto-detect on every configure)")

# Effective commit: an explicit -D/cache override or $GITHUB_SHA wins; otherwise
# re-detect from git on every configure so local rebuilds never show a stale hash
# on the diagnostic screen.
set(_vg_commit "${VOXELGAME_BUILD_COMMIT}")
if(NOT _vg_commit AND DEFINED ENV{GITHUB_SHA})
    set(_vg_commit "$ENV{GITHUB_SHA}")
endif()
if(NOT _vg_commit)
    execute_process(
        COMMAND git rev-parse --verify HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE _vg_commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _vg_git_result
    )
    if(NOT _vg_git_result EQUAL 0)
        set(_vg_commit "unknown")
    endif()
endif()
set(VOXELGAME_BUILD_COMMIT "${_vg_commit}")
message(STATUS "VoxelGame build commit: ${VOXELGAME_BUILD_COMMIT}")

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

# raylib-nx otherwise fakes WASD/arrow keys and mouse motion from the pad, which
# fights our own analog stick + D-pad handling. We map every action ourselves.
if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch" AND TARGET raylib)
    target_compile_definitions(raylib PRIVATE NX_DISABLE_GAMEPAD_EMULATION)
endif()

# Treat raylib's headers as system headers so raymath.h's compound literals do
# not trip our -Wextra / -Wmissing-field-initializers.
if(TARGET raylib)
    get_target_property(_raylib_includes raylib INTERFACE_INCLUDE_DIRECTORIES)
    if(_raylib_includes)
        set_target_properties(raylib PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_raylib_includes}")
    endif()
endif()

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
    src/platform/Assets.cpp
    src/platform/Platform.cpp
)
target_include_directories(voxelgame_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

add_library(voxelgame_world STATIC
    src/world/AtlasDescriptor.cpp
    src/world/Block.cpp
    src/world/BlockAtlasBinding.cpp
    src/world/ChunkMesher.cpp
    src/world/ChunkSection.cpp
    src/world/PlayerBody.cpp
    src/world/Raycast.cpp
    src/world/TerrainGenerator.cpp
    src/world/World.cpp
    src/world/WorldSave.cpp
)
target_include_directories(voxelgame_world PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(voxelgame_world SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

add_library(voxelgame_model STATIC
    src/model/Animation.cpp
    src/model/AnimationBinary.cpp
    src/model/VoxelModel.cpp
    src/model/VoxelModelBinary.cpp
    src/model/VoxelModelLoader.cpp
    src/model/VoxelModelMesher.cpp
)
target_include_directories(voxelgame_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(voxelgame_model SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

add_library(voxelgame_render STATIC
    src/render/BlockAtlas.cpp
    src/render/ChunkRenderMesh.cpp
    src/render/Frustum.cpp
    src/render/TilingShader.cpp
    src/render/VoxelModelRenderer.cpp
)
target_include_directories(voxelgame_render PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(voxelgame_render PUBLIC voxelgame_world voxelgame_model PRIVATE raylib)

add_executable(voxelgame
    src/app/Input.cpp
    src/app/main.cpp
)
target_link_libraries(voxelgame PRIVATE voxelgame_core voxelgame_render raylib)

if(MINGW)
    target_link_options(voxelgame PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Host-only asset compiler: .vxm.json/.vxa.json -> .vxm/.vxa (PLAN.md 6.3).
if(NOT CMAKE_CROSSCOMPILING)
    add_executable(voxelgame_assetc tools/asset_compiler/main.cpp)
    target_link_libraries(voxelgame_assetc PRIVATE voxelgame_model)
    if(MINGW)
        target_link_options(voxelgame_assetc PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    if(MSVC)
        target_compile_options(voxelgame_assetc PRIVATE /W4 /permissive-)
    else()
        target_compile_options(voxelgame_assetc PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# Runtime assets. On desktop they are staged next to the executable so the game
# finds them via GetApplicationDirectory(); on Switch they are merged into the
# NRO's romfs image below.
set(VOXELGAME_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(MSVC)
    target_compile_options(voxelgame_core PRIVATE /W4 /permissive-)
    target_compile_options(voxelgame_world PRIVATE /W4 /permissive-)
    target_compile_options(voxelgame_model PRIVATE /W4 /permissive-)
    target_compile_options(voxelgame_render PRIVATE /W4 /permissive-)
    target_compile_options(voxelgame PRIVATE /W4 /permissive-)
else()
    target_compile_options(voxelgame_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(voxelgame_world PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(voxelgame_model PRIVATE -Wall -Wextra -Wpedantic)
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

    # Assemble the romfs image: the tracked romfs/ tree plus the shared assets
    # under assets/, mirroring the SD-card layout (sdmc:/switch/voxelgame/assets/).
    set(VOXELGAME_ROMFS_STAGE "${CMAKE_CURRENT_BINARY_DIR}/romfs-stage")
    file(MAKE_DIRECTORY "${VOXELGAME_ROMFS_STAGE}")
    add_custom_target(voxelgame_romfs ALL
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${VOXELGAME_ROMFS_STAGE}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/romfs" "${VOXELGAME_ROMFS_STAGE}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${VOXELGAME_ASSET_DIR}" "${VOXELGAME_ROMFS_STAGE}/assets"
        BYPRODUCTS "${VOXELGAME_ROMFS_STAGE}"
        COMMENT "Staging romfs (romfs/ + assets/ -> assets/)"
        VERBATIM
    )

    nx_create_nro(
        voxelgame_nro
        TARGET voxelgame
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/voxelgame.nro"
        NACP "${CMAKE_CURRENT_BINARY_DIR}/voxelgame.nacp"
        ROMFS "${VOXELGAME_ROMFS_STAGE}"
    )
    add_dependencies(voxelgame_nro voxelgame_romfs)
else()
    # Always mirror assets next to the executable, so editing a data file (atlas,
    # descriptor) takes effect without a source rebuild.
    add_custom_target(voxelgame_assets ALL
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${VOXELGAME_ASSET_DIR}" "$<TARGET_FILE_DIR:voxelgame>/assets"
        COMMENT "Staging assets next to voxelgame"
        VERBATIM
    )
    add_dependencies(voxelgame_assets voxelgame)
    install(TARGETS voxelgame RUNTIME DESTINATION .)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/build-info.txt" DESTINATION .)
    install(DIRECTORY "${VOXELGAME_ASSET_DIR}/" DESTINATION assets)
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

    add_executable(voxelgame_model_tests tests/VoxelModelTests.cpp)
    target_link_libraries(voxelgame_model_tests PRIVATE voxelgame_model)
    if(MINGW)
        target_link_options(voxelgame_model_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()

    add_test(NAME core.build_info COMMAND voxelgame_core_tests)
    add_test(NAME world.chunk_mesher COMMAND voxelgame_world_tests)
    add_test(NAME model.voxel_model COMMAND voxelgame_model_tests)
    add_test(NAME app.smoke COMMAND voxelgame --smoke-test)
endif()
