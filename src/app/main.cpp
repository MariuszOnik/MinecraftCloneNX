#include "core/BuildInfo.hpp"
#include "render/BlockAtlas.hpp"
#include "render/ChunkRenderMesh.hpp"
#include "world/Block.hpp"
#include "world/ChunkMesher.hpp"
#include "world/ChunkSection.hpp"

#include <raylib.h>

#include <cstdio>
#include <chrono>
#include <cstring>
#include <string>

namespace {

voxelgame::ChunkSection CreateTestSection() {
    voxelgame::ChunkSection section;
    for (int z = 0; z < voxelgame::ChunkSection::Size; ++z) {
        for (int x = 0; x < voxelgame::ChunkSection::Size; ++x) {
            const int height = 3 + ((x / 4 + z / 4) % 3);
            for (int y = 0; y <= height; ++y) {
                voxelgame::BlockId block = voxelgame::blocks::Stone;
                if (y == height) {
                    block = voxelgame::blocks::Grass;
                } else if (y + 2 >= height) {
                    block = voxelgame::blocks::Dirt;
                }
                section.Set(x, y, z, block);
            }
        }
    }
    section.Set(8, 8, 8, voxelgame::blocks::Grass);
    return section;
}

bool HasArgument(const int argc, char* argv[], const char* expected) {
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], expected) == 0) {
            return true;
        }
    }
    return false;
}

void PrintBuildInfo(const voxelgame::BuildInfo& info) {
    std::printf("VoxelGame %.*s | %.*s | commit %.*s\n",
                static_cast<int>(info.version.size()), info.version.data(),
                static_cast<int>(info.platform.size()), info.platform.data(),
                static_cast<int>(info.commit.size()), info.commit.data());
}

}  // namespace

int main(int argc, char* argv[]) {
    const voxelgame::BuildInfo build = voxelgame::GetBuildInfo();
    if (!voxelgame::HasValidBuildInfo(build)) {
        std::fputs("Invalid build information\n", stderr);
        return 2;
    }

    if (HasArgument(argc, argv, "--smoke-test")) {
        PrintBuildInfo(build);
        return 0;
    }

    const bool smokeWindow = HasArgument(argc, argv, "--smoke-window");
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(960, 540, "VoxelGame - voxel-first");
    if (!IsWindowReady()) {
        std::fputs("raylib window initialization failed\n", stderr);
        return 3;
    }

    SetTargetFPS(60);

    Image atlasImage = voxelgame::GenerateBlockAtlasImage();
    Texture2D blockAtlas = LoadTextureFromImage(atlasImage);
    UnloadImage(atlasImage);
    SetTextureFilter(blockAtlas, TEXTURE_FILTER_POINT);

    int result = 0;
    {
        voxelgame::ChunkSection section = CreateTestSection();
        voxelgame::ChunkMesher mesher;
        voxelgame::ChunkRenderMesh renderMesh;
        voxelgame::MeshData meshData;
        double meshMilliseconds = 0.0;
        int meshRebuilds = 0;

        const auto rebuildMesh = [&]() {
            const auto start = std::chrono::steady_clock::now();
            meshData = mesher.Build(section);
            const auto finish = std::chrono::steady_clock::now();
            meshMilliseconds =
                std::chrono::duration<double, std::milli>(finish - start).count();
            if (!renderMesh.Upload(meshData, blockAtlas)) {
                return false;
            }
            section.MarkMeshClean();
            ++meshRebuilds;
            return true;
        };

        if (!rebuildMesh()) {
            result = 4;
        } else {
            Camera3D camera{{24.0F, 18.0F, 24.0F},
                            {8.0F, 3.0F, 8.0F},
                            {0.0F, 1.0F, 0.0F},
                            45.0F,
                            CAMERA_PERSPECTIVE};
            bool markerBlockVisible = true;
            int renderedFrames = 0;
            while (!WindowShouldClose()) {
                UpdateCamera(&camera, CAMERA_ORBITAL);

                const bool togglePressed =
                    IsKeyPressed(KEY_R) ||
                    (IsGamepadAvailable(0) &&
                     IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
                if (togglePressed) {
                    markerBlockVisible = !markerBlockVisible;
                    section.Set(8, 8, 8,
                                markerBlockVisible ? voxelgame::blocks::Grass
                                                   : voxelgame::blocks::Air);
                    if (!rebuildMesh()) {
                        result = 5;
                        break;
                    }
                }

                BeginDrawing();
                ClearBackground(Color{18, 22, 31, 255});

                BeginMode3D(camera);
                DrawGrid(32, 1.0F);
                renderMesh.Draw({0.0F, 0.0F, 0.0F});
                DrawBoundingBox({{0.0F, 0.0F, 0.0F}, {16.0F, 16.0F, 16.0F}},
                                Fade(SKYBLUE, 0.35F));
                EndMode3D();

                DrawRectangle(12, 12, 360, 206, Fade(BLACK, 0.72F));
                DrawText("VOXEL-FIRST / TEST SECTION", 24, 22, 22, LIME);
                DrawText(TextFormat("Platform: %.*s", static_cast<int>(build.platform.size()),
                                    build.platform.data()),
                         24, 54, 18, RAYWHITE);
                DrawText(TextFormat("Commit: %.*s",
                                    static_cast<int>(voxelgame::ShortCommit(build.commit).size()),
                                    voxelgame::ShortCommit(build.commit).data()),
                         24, 76, 18, LIGHTGRAY);
                DrawText(TextFormat("Blocks: %i", static_cast<int>(section.NonAirBlockCount())),
                         24, 104, 18, LIGHTGRAY);
                DrawText(TextFormat("Quads: %i  Triangles: %i",
                                    static_cast<int>(meshData.quadCount),
                                    static_cast<int>(meshData.TriangleCount())),
                         24, 126, 18, LIGHTGRAY);
                DrawText(TextFormat("Mesh: %.3f ms  Rebuilds: %i", meshMilliseconds,
                                    meshRebuilds),
                         24, 148, 18, LIGHTGRAY);
                DrawText(TextFormat("FPS: %i", GetFPS()), 24, 170, 18, LIGHTGRAY);
                DrawText(TextFormat("Atlas: %ix%i procedural (POINT)", blockAtlas.width,
                                    blockAtlas.height),
                         24, 192, 18, LIGHTGRAY);
                DrawText("R / gamepad A: toggle voxel + rebuild mesh", 20, 510, 18, GRAY);

                EndDrawing();

                ++renderedFrames;
                if (smokeWindow && renderedFrames >= 3) {
                    break;
                }
            }
        }
    }

    UnloadTexture(blockAtlas);
    CloseWindow();
    return result;
}
