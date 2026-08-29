#include "app/Input.hpp"
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
#include "world/PlayerBody.hpp"
#include "world/Raycast.hpp"
#include "world/TerrainGenerator.hpp"
#include "world/World.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int kWorldSectionsX = 5;
constexpr int kWorldSectionsY = 2;
constexpr int kWorldSectionsZ = 5;
constexpr std::uint32_t kDefaultSeed = 0xC0FFEEU;

constexpr float kReach = 5.0F;
constexpr voxelgame::BlockId kPalette[] = {
    voxelgame::blocks::Grass,  voxelgame::blocks::Dirt,   voxelgame::blocks::Stone,
    voxelgame::blocks::Cobblestone, voxelgame::blocks::Planks, voxelgame::blocks::Wood,
    voxelgame::blocks::Sand,   voxelgame::blocks::Glass,
};
constexpr int kPaletteCount = static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0]));

voxelgame::World CreateWorld(const std::uint32_t seed) {
    voxelgame::World world(kWorldSectionsX, kWorldSectionsY, kWorldSectionsZ);
    voxelgame::TerrainGenerator(seed).Generate(world);
    return world;
}

voxelgame::PlayerBody SpawnPlayer(const voxelgame::World& world) {
    const int x = world.BlocksX() / 2;
    const int z = world.BlocksZ() / 2;
    int y = world.BlocksY() - 1;
    while (y > 0 && !voxelgame::IsSolidBlock(world.GetBlock(x, y, z))) {
        --y;
    }
    // Drop in from a few blocks up so the first frames show the world.
    return voxelgame::PlayerBody({static_cast<float>(x) + 0.5F, static_cast<float>(y + 4),
                                  static_cast<float>(z) + 0.5F});
}

std::uint32_t SeedFromArgs(const int argc, char* argv[]) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::strcmp(argv[index], "--seed") == 0) {
            return static_cast<std::uint32_t>(std::strtoul(argv[index + 1], nullptr, 0));
        }
    }
    return kDefaultSeed;
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

    const std::uint32_t seed = SeedFromArgs(argc, argv);

    int result = 0;
    {
        voxelgame::World world = CreateWorld(seed);
        voxelgame::ChunkMesher mesher;
        std::vector<voxelgame::ChunkRenderMesh> meshes(
            static_cast<std::size_t>(world.SectionCount()));
        std::vector<std::size_t> sectionQuads(static_cast<std::size_t>(world.SectionCount()), 0);

        std::size_t quadTotal = 0;
        std::size_t triangleTotal = 0;
        double meshMilliseconds = 0.0;
        int meshRebuilds = 0;

        const auto sectionIndex = [&](const int sx, const int sy, const int sz) {
            return static_cast<std::size_t>((sy * world.SectionsZ() + sz) * world.SectionsX() + sx);
        };

        // Remeshes every section flagged dirty; returns false only on GPU error.
        const auto rebuildDirty = [&]() {
            const auto start = std::chrono::steady_clock::now();
            for (int sy = 0; sy < world.SectionsY(); ++sy) {
                for (int sz = 0; sz < world.SectionsZ(); ++sz) {
                    for (int sx = 0; sx < world.SectionsX(); ++sx) {
                        if (!world.SectionMeshDirty(sx, sy, sz)) {
                            continue;
                        }
                        const std::size_t index = sectionIndex(sx, sy, sz);
                        const voxelgame::MeshData data =
                            mesher.Build(world, sx, sy, sz, atlas.binding);
                        if (!meshes[index].Upload(data, blockAtlas, tilingShader)) {
                            return false;
                        }
                        sectionQuads[index] = data.quadCount;
                        world.MarkSectionMeshClean(sx, sy, sz);
                        ++meshRebuilds;
                    }
                }
            }
            meshMilliseconds =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                    .count();

            quadTotal = 0;
            for (const std::size_t count : sectionQuads) {
                quadTotal += count;
            }
            triangleTotal = quadTotal * 2;
            return true;
        };

        if (!rebuildDirty()) {
            result = 4;
        } else {
            voxelgame::PlayerBody player = SpawnPlayer(world);
            float yaw = 3.1415926F * 0.25F;
            float pitch = 0.15F;
            bool mouseLook = !smokeWindow;
            int heldBlock = 2;  // stone

            Camera3D camera{};
            camera.up = {0.0F, 1.0F, 0.0F};
            camera.fovy = 70.0F;
            camera.projection = CAMERA_PERSPECTIVE;

            int renderedFrames = 0;
            while (!WindowShouldClose()) {
                // Only react to input while focused, and release the cursor when
                // the window loses focus so an alt-tabbed game neither drifts nor
                // traps the mouse.
                const bool focused = IsWindowFocused();
                if (mouseLook && focused && !IsCursorHidden()) {
                    DisableCursor();
                } else if ((!mouseLook || !focused) && IsCursorHidden()) {
                    EnableCursor();
                }

                const voxelgame::FrameInput in =
                    focused ? voxelgame::PollFrameInput(mouseLook) : voxelgame::FrameInput{};
                if (in.toggleMouseLook) {
                    mouseLook = !mouseLook;
                    if (mouseLook) {
                        DisableCursor();
                    } else {
                        EnableCursor();
                    }
                }

                yaw += in.lookYaw;
                pitch = std::clamp(pitch + in.lookPitch, -1.55F, 1.55F);
                const float cy = std::cos(yaw);
                const float sy = std::sin(yaw);
                const float cp = std::cos(pitch);
                const float sp = std::sin(pitch);
                const Vector3 forward{sy * cp, sp, -cy * cp};
                const Vector3 forwardFlat{sy, 0.0F, -cy};
                const Vector3 rightFlat{cy, 0.0F, sy};

                float wishX = forwardFlat.x * in.moveForward + rightFlat.x * in.moveStrafe;
                float wishZ = forwardFlat.z * in.moveForward + rightFlat.z * in.moveStrafe;
                const float wishLen = std::sqrt(wishX * wishX + wishZ * wishZ);
                if (wishLen > 1.0F) {
                    wishX /= wishLen;
                    wishZ /= wishLen;
                }
                const float speed = in.sprint ? 7.5F : 4.5F;
                const float dt = std::min(GetFrameTime(), 0.05F);
                player.Step(world, {wishX * speed, 0.0F, wishZ * speed}, in.jump, dt);

                const voxelgame::Vec3 eye = player.EyePosition();
                camera.position = {eye.x, eye.y, eye.z};
                camera.target = {eye.x + forward.x, eye.y + forward.y, eye.z + forward.z};

                if (in.cycleBlock != 0) {
                    heldBlock = ((heldBlock + in.cycleBlock) % kPaletteCount + kPaletteCount) %
                                kPaletteCount;
                }

                const voxelgame::RaycastHit target = voxelgame::Raycast(
                    world, eye, {forward.x, forward.y, forward.z}, kReach);

                bool worldChanged = false;
                if (target.hit && in.breakBlock) {
                    worldChanged =
                        world.SetBlock(target.blockX, target.blockY, target.blockZ,
                                       voxelgame::blocks::Air);
                }
                if (target.hit && in.placeBlock) {
                    const int px = target.blockX + target.normalX;
                    const int py = target.blockY + target.normalY;
                    const int pz = target.blockZ + target.normalZ;
                    if (!voxelgame::IsSolidBlock(world.GetBlock(px, py, pz)) &&
                        !player.Intersects(px, py, pz)) {
                        worldChanged = world.SetBlock(px, py, pz, kPalette[heldBlock]);
                    }
                }
                if (worldChanged && !rebuildDirty()) {
                    result = 5;
                    break;
                }

                BeginDrawing();
                ClearBackground(Color{18, 22, 31, 255});

                BeginMode3D(camera);
                DrawGrid(64, 1.0F);
                for (int sy = 0; sy < world.SectionsY(); ++sy) {
                    for (int sz = 0; sz < world.SectionsZ(); ++sz) {
                        for (int sx = 0; sx < world.SectionsX(); ++sx) {
                            meshes[sectionIndex(sx, sy, sz)].Draw(
                                {static_cast<float>(sx * voxelgame::ChunkSection::Size),
                                 static_cast<float>(sy * voxelgame::ChunkSection::Size),
                                 static_cast<float>(sz * voxelgame::ChunkSection::Size)});
                        }
                    }
                }
                DrawBoundingBox({{0.0F, 0.0F, 0.0F},
                                 {static_cast<float>(world.BlocksX()),
                                  static_cast<float>(world.BlocksY()),
                                  static_cast<float>(world.BlocksZ())}},
                                Fade(SKYBLUE, 0.35F));
                if (target.hit) {
                    const Vector3 centre{static_cast<float>(target.blockX) + 0.5F,
                                         static_cast<float>(target.blockY) + 0.5F,
                                         static_cast<float>(target.blockZ) + 0.5F};
                    DrawCubeWires(centre, 1.02F, 1.02F, 1.02F, Fade(RAYWHITE, 0.9F));
                }
                EndMode3D();

                DrawRectangle(12, 12, 400, 230, Fade(BLACK, 0.72F));
                DrawText("VOXEL-FIRST / WORLD", 24, 22, 22, LIME);
                DrawText(TextFormat("Platform: %.*s", static_cast<int>(build.platform.size()),
                                    build.platform.data()),
                         24, 54, 18, RAYWHITE);
                DrawText(TextFormat("Commit: %.*s",
                                    static_cast<int>(voxelgame::ShortCommit(build.commit).size()),
                                    voxelgame::ShortCommit(build.commit).data()),
                         24, 76, 18, LIGHTGRAY);
                DrawText(TextFormat("Seed: 0x%X  Sections: %i  Blocks: %i",
                                    static_cast<unsigned>(seed), world.SectionCount(),
                                    static_cast<int>(world.NonAirBlockCount())),
                         24, 104, 18, LIGHTGRAY);
                DrawText(TextFormat("Quads: %i  Tris: %i  Mesh: %.2f ms",
                                    static_cast<int>(quadTotal), static_cast<int>(triangleTotal),
                                    meshMilliseconds),
                         24, 126, 18, LIGHTGRAY);
                DrawText(TextFormat("Player: %.1f %.1f %.1f  %s", static_cast<double>(eye.x),
                                    static_cast<double>(player.Position().y),
                                    static_cast<double>(eye.z),
                                    player.OnGround() ? "grounded" : "airborne"),
                         24, 148, 18, LIGHTGRAY);
                DrawText(TextFormat("FPS: %i  Atlas: %ix%i %s", GetFPS(), blockAtlas.width,
                                    blockAtlas.height, atlasSourceLabel),
                         24, 170, 18, LIGHTGRAY);
                DrawText(TextFormat("Held: %.*s  (Q/E or X/Y)",
                                    static_cast<int>(
                                        voxelgame::GetBlockDefinition(kPalette[heldBlock]).name.size()),
                                    voxelgame::GetBlockDefinition(kPalette[heldBlock]).name.data()),
                         24, 192, 18, target.hit ? LIME : LIGHTGRAY);
                DrawText("WASD/stick move  Space/A jump  LMB/ZR break  RMB/ZL place  Tab mouse",
                         24, 214, 15, GRAY);

                {
                    const int cx = GetScreenWidth() / 2;
                    const int cy = GetScreenHeight() / 2;
                    const Color tint = target.hit ? Color{120, 255, 140, 220}
                                                  : Fade(RAYWHITE, 0.6F);
                    DrawLine(cx - 7, cy, cx + 7, cy, tint);
                    DrawLine(cx, cy - 7, cx, cy + 7, tint);
                }

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
