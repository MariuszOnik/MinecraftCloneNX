#include "core/BuildInfo.hpp"
#include "platform/Assets.hpp"
#include "render/BlockAtlas.hpp"
#include "render/ChunkRenderMesh.hpp"
#include "render/TilingShader.hpp"
#include "world/AtlasDescriptor.hpp"
#include "world/Block.hpp"
#include "world/BlockAtlasBinding.hpp"
#include "world/ChunkMesher.hpp"
#include "world/ChunkSection.hpp"

#include <raylib.h>

#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace {

constexpr int kMarkerX = 2;
constexpr int kMarkerY = 11;
constexpr int kMarkerZ = 2;

voxelgame::ChunkSection CreateTestSection() {
    using namespace voxelgame;
    ChunkSection section;
    constexpr int N = ChunkSection::Size;

    // Layered ground: bedrock floor, stone body, dirt, grass cap.
    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            const int height = 4 + ((x / 5 + z / 5) % 3);
            for (int y = 0; y <= height; ++y) {
                BlockId block = blocks::Stone;
                if (y == 0) {
                    block = blocks::Bedrock;
                } else if (y == height) {
                    block = blocks::Grass;
                } else if (y + 2 >= height) {
                    block = blocks::Dirt;
                }
                section.Set(x, y, z, block);
            }
        }
    }

    // Sand and gravel patch in a corner.
    for (int z = 1; z < 5; ++z) {
        for (int x = 1; x < 5; ++x) {
            section.Set(x, 5, z, ((x + z) % 2 == 0) ? blocks::Sand : blocks::Gravel);
        }
    }

    // Plank hut on a cobblestone base with a glass window and a doorway.
    const int hx = 9;
    const int hz = 9;
    for (int dz = 0; dz < 4; ++dz) {
        for (int dx = 0; dx < 4; ++dx) {
            section.Set(hx + dx, 6, hz + dz, blocks::Cobblestone);
            section.Set(hx + dx, 10, hz + dz, blocks::Planks);
            const bool wall = dx == 0 || dz == 0 || dx == 3 || dz == 3;
            for (int dy = 1; dy <= 3 && wall; ++dy) {
                BlockId b = blocks::Planks;
                if (dx == 2 && dz == 0 && dy == 2) {
                    b = blocks::Glass;
                } else if (dx == 1 && dz == 0 && dy == 1) {
                    b = blocks::Air;
                }
                section.Set(hx + dx, 6 + dy, hz + dz, b);
            }
        }
    }

    // Small tree: wood trunk with a leaf canopy.
    const int tx = 4;
    const int tz = 11;
    for (int dy = 1; dy <= 4; ++dy) {
        section.Set(tx, 6 + dy, tz, blocks::Wood);
    }
    for (int dy = 4; dy <= 6; ++dy) {
        for (int dz = -2; dz <= 2; ++dz) {
            for (int dx = -2; dx <= 2; ++dx) {
                if (std::abs(dx) + std::abs(dz) + std::abs(dy - 5) > 3) {
                    continue;
                }
                if (dx == 0 && dz == 0 && dy <= 4) {
                    continue;
                }
                section.Set(tx + dx, 6 + dy, tz + dz, blocks::Leaves);
            }
        }
    }

    section.Set(kMarkerX, kMarkerY, kMarkerZ, blocks::Glass);
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

const char* AtlasSourceLabel(const voxelgame::AssetPaths::Origin origin, const bool procedural) {
    if (procedural) {
        return "procedural";
    }
    switch (origin) {
        case voxelgame::AssetPaths::Origin::SdCard:
            return "SD";
        case voxelgame::AssetPaths::Origin::Bundle:
            return "bundled";
        default:
            return "assets";
    }
}

std::optional<std::string> ReadTextFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

struct LoadedAtlas {
    Texture2D texture{};
    voxelgame::BlockAtlasBinding binding{};
    const char* sourceLabel = "procedural";
};

// Loads the atlas descriptor and texture via the asset resolver (SD card first,
// then the bundled copy). A missing/invalid descriptor falls back to the compiled
// block->tile defaults; a missing texture falls back to the procedural atlas.
// Every fallback is logged so nothing fails silently.
LoadedAtlas LoadBlockAtlas(const voxelgame::AssetPaths& assets) {
    LoadedAtlas out;
    std::string textureRelative = "atlases/blocks.png";

    const voxelgame::AssetPaths::Resolved descriptor = assets.Resolve("atlases/blocks.json");
    if (descriptor.found) {
        std::string error = "unreadable";
        std::optional<voxelgame::AtlasDescriptor> parsed;
        if (const auto text = ReadTextFile(descriptor.path)) {
            parsed = voxelgame::ParseAtlasDescriptor(*text, error);
        }
        if (parsed) {
            out.binding.Apply(*parsed);
            textureRelative = "atlases/" + parsed->texture;
            TraceLog(LOG_INFO, "VOXEL: atlas descriptor '%s' -> %s (%dx%d, %d px tiles)",
                     descriptor.path.c_str(), parsed->texture.c_str(), parsed->atlasWidth,
                     parsed->atlasHeight, parsed->tileSize);
        } else {
            TraceLog(LOG_WARNING, "VOXEL: atlas descriptor '%s' invalid (%s); using defaults",
                     descriptor.path.c_str(), error.c_str());
        }
    }

    const voxelgame::AssetPaths::Resolved image = assets.Resolve(textureRelative);
    Texture2D atlas{};
    if (image.found) {
        Image pixels = LoadImage(image.path.c_str());
        atlas = LoadTextureFromImage(pixels);
        UnloadImage(pixels);
    }
    if (atlas.id != 0) {
        out.sourceLabel = AtlasSourceLabel(image.origin, false);
        TraceLog(LOG_INFO, "VOXEL: loaded block atlas from '%s'", image.path.c_str());
    } else {
        out.sourceLabel = AtlasSourceLabel(image.origin, true);
        TraceLog(LOG_WARNING, "VOXEL: block atlas '%s' unavailable, using procedural fallback",
                 image.path.c_str());
        Image pixels = voxelgame::GenerateBlockAtlasImage();
        atlas = LoadTextureFromImage(pixels);
        UnloadImage(pixels);
        out.binding = voxelgame::BlockAtlasBinding{};  // procedural atlas uses the default grid
    }
    SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
    out.texture = atlas;
    return out;
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

    if (HasArgument(argc, argv, "--export-atlas")) {
        Image image = voxelgame::GenerateBlockAtlasImage();
        const bool exported = ExportImage(image, "assets/atlases/blocks.png");
        UnloadImage(image);
        if (!exported) {
            std::fputs("Failed to export block atlas\n", stderr);
            return 4;
        }
        std::puts("Wrote assets/atlases/blocks.png");
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

    const voxelgame::AssetPaths assets(GetApplicationDirectory());
    const LoadedAtlas atlas = LoadBlockAtlas(assets);
    const Texture2D blockAtlas = atlas.texture;
    const char* const atlasSourceLabel = atlas.sourceLabel;

    const Shader tilingShader = voxelgame::LoadTilingShader();
    voxelgame::SetTilingShaderExtent(tilingShader, atlas.binding.TileExtentU(),
                                     atlas.binding.TileExtentV());

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
            meshData = mesher.Build(section, atlas.binding);
            const auto finish = std::chrono::steady_clock::now();
            meshMilliseconds =
                std::chrono::duration<double, std::milli>(finish - start).count();
            if (!renderMesh.Upload(meshData, blockAtlas, tilingShader)) {
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
                    section.Set(kMarkerX, kMarkerY, kMarkerZ,
                                markerBlockVisible ? voxelgame::blocks::Glass
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
                DrawText(TextFormat("Atlas: %ix%i %s (POINT)", blockAtlas.width,
                                    blockAtlas.height, atlasSourceLabel),
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

    UnloadShader(tilingShader);
    UnloadTexture(blockAtlas);
    CloseWindow();
    return result;
}
