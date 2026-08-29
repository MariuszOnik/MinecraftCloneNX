#include "app/Input.hpp"
#include "core/BuildInfo.hpp"
#include "platform/Assets.hpp"
#include "render/BlockAtlas.hpp"
#include "render/ChunkRenderMesh.hpp"
#include "render/Frustum.hpp"
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
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int kWorldSectionsY = 2;
constexpr std::uint32_t kDefaultSeed = 0xC0FFEEU;

// Streaming radii, in chunk columns, measured with Chebyshev distance.
constexpr int kLoadRadius = 5;
constexpr int kKeepRadius = 7;
constexpr int kColumnBudget = 2;      // new columns generated per frame
constexpr double kMeshBudgetMs = 4.0;  // time spent (re)meshing per frame

constexpr float kReach = 5.0F;
constexpr voxelgame::BlockId kPalette[] = {
    voxelgame::blocks::Grass,  voxelgame::blocks::Dirt,   voxelgame::blocks::Stone,
    voxelgame::blocks::Cobblestone, voxelgame::blocks::Planks, voxelgame::blocks::Wood,
    voxelgame::blocks::Sand,   voxelgame::blocks::Glass,  voxelgame::blocks::GlassPane,
};
constexpr int kPaletteCount = static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0]));

int SurfaceY(const voxelgame::World& world, const int x, const int z) {
    int y = world.BlocksY() - 1;
    while (y > 0 && !voxelgame::IsCollidableBlock(world.GetBlock(x, y, z))) {
        --y;
    }
    return y;
}

voxelgame::PlayerBody SpawnPlayer(voxelgame::World& world, const int chunkX, const int chunkZ) {
    world.EnsureColumn(chunkX, chunkZ);
    const int x = chunkX * voxelgame::ChunkSection::Size + 8;
    const int z = chunkZ * voxelgame::ChunkSection::Size + 8;
    // Drop in from a few blocks up so the first frames show the world.
    return voxelgame::PlayerBody({static_cast<float>(x) + 0.5F,
                                  static_cast<float>(SurfaceY(world, x, z) + 4),
                                  static_cast<float>(z) + 0.5F});
}

// A temporary glass + water showcase near spawn so transparency is visible.
// Slice 5 replaces this with the proper "chamber of panes" test scene.
void BuildShowcase(voxelgame::World& world, const int baseX, const int baseZ) {
    using namespace voxelgame;
    const int gy = SurfaceY(world, baseX, baseZ) + 1;

    // Sunken 4x4 water pool.
    for (int dz = 0; dz < 4; ++dz) {
        for (int dx = 0; dx < 4; ++dx) {
            world.SetBlock(baseX + dx, gy - 1, baseZ + dz, blocks::Water);
            world.SetBlock(baseX + dx, gy - 2, baseZ + dz, blocks::Water);
            world.SetBlock(baseX + dx, gy - 3, baseZ + dz, blocks::Sand);
        }
    }
    // A short glass wall, and a row of thin glass panes in front of it.
    for (int dy = 0; dy < 3; ++dy) {
        for (int dx = 0; dx < 4; ++dx) {
            world.SetBlock(baseX + dx, gy + dy, baseZ + 6, blocks::Glass);
            world.SetBlock(baseX + dx, gy + dy, baseZ + 3, blocks::GlassPane);
        }
    }
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
        const voxelgame::TerrainGenerator generator(seed);
        voxelgame::World world(kWorldSectionsY,
                               [&generator](voxelgame::World& w, int cx, int cz) {
                                   generator.FillColumn(w, cx, cz);
                               });
        voxelgame::ChunkMesher mesher;

        using SectionKey = std::array<int, 3>;  // {chunkX, sectionY, chunkZ}
        std::map<SectionKey, voxelgame::ChunkRenderMesh> meshes;
        std::map<SectionKey, std::size_t> sectionQuads;

        std::size_t quadTotal = 0;
        std::size_t triangleTotal = 0;
        double meshMilliseconds = 0.0;
        int meshRebuilds = 0;
        bool meshError = false;

        // (Re)builds one section's GPU mesh; an emptied section drops its mesh.
        const auto meshSection = [&](const int cx, const int sy, const int cz) {
            const SectionKey key{cx, sy, cz};
            const voxelgame::SectionMesh data = mesher.Build(world, cx, sy, cz, atlas.binding);
            if (data.Empty()) {
                meshes.erase(key);
                sectionQuads.erase(key);
            } else if (meshes[key].Upload(data, blockAtlas, tilingShader)) {
                sectionQuads[key] = data.QuadCount();
            } else {
                meshError = true;
            }
            world.MarkSectionMeshClean(cx, sy, cz);
            ++meshRebuilds;
        };

        const auto collectDirty = [&](const int pcx, const int pcz) {
            std::vector<SectionKey> dirty;
            world.ForEachLoadedSection([&](int cx, int sy, int cz) {
                if (world.SectionMeshDirty(cx, sy, cz)) {
                    dirty.push_back({cx, sy, cz});
                }
            });
            std::sort(dirty.begin(), dirty.end(), [&](const SectionKey& a, const SectionKey& b) {
                const int da = std::max(std::abs(a[0] - pcx), std::abs(a[2] - pcz));
                const int db = std::max(std::abs(b[0] - pcx), std::abs(b[2] - pcz));
                return da < db;
            });
            return dirty;
        };

        const auto refreshTotals = [&]() {
            quadTotal = 0;
            for (const auto& entry : sectionQuads) {
                quadTotal += entry.second;
            }
            triangleTotal = quadTotal * 2;
        };

        // Loads the ring around (pcx, pcz) up to a column budget, unloads columns
        // past the keep radius, and remeshes dirty sections within a time budget.
        const auto stream = [&](const int pcx, const int pcz, const int columnBudget,
                                const double meshBudgetMs) {
            int loads = 0;
            for (int r = 0; r <= kLoadRadius && loads < columnBudget; ++r) {
                for (int dz = -r; dz <= r && loads < columnBudget; ++dz) {
                    for (int dx = -r; dx <= r && loads < columnBudget; ++dx) {
                        if (std::max(std::abs(dx), std::abs(dz)) != r) {
                            continue;
                        }
                        if (world.EnsureColumn(pcx + dx, pcz + dz)) {
                            ++loads;
                        }
                    }
                }
            }

            std::vector<std::array<int, 2>> tooFar;
            world.ForEachLoadedColumn([&](int cx, int cz) {
                if (std::max(std::abs(cx - pcx), std::abs(cz - pcz)) > kKeepRadius) {
                    tooFar.push_back({cx, cz});
                }
            });
            for (const auto& column : tooFar) {
                world.UnloadColumn(column[0], column[1]);
                for (int sy = 0; sy < world.SectionsY(); ++sy) {
                    meshes.erase({column[0], sy, column[1]});
                    sectionQuads.erase({column[0], sy, column[1]});
                }
            }

            const auto meshStart = std::chrono::steady_clock::now();
            const std::vector<SectionKey> dirty = collectDirty(pcx, pcz);
            meshMilliseconds = 0.0;
            for (const SectionKey& section : dirty) {
                meshSection(section[0], section[1], section[2]);
                meshMilliseconds = std::chrono::duration<double, std::milli>(
                                       std::chrono::steady_clock::now() - meshStart)
                                       .count();
                if (meshMilliseconds > meshBudgetMs) {
                    break;
                }
            }
            refreshTotals();
        };

        voxelgame::PlayerBody player = SpawnPlayer(world, 0, 0);
        int playerChunkX = voxelgame::World::ToChunk(static_cast<int>(std::floor(player.Position().x)));
        int playerChunkZ = voxelgame::World::ToChunk(static_cast<int>(std::floor(player.Position().z)));

        // Fill and mesh a small spawn area synchronously; the rest streams in.
        for (int dz = -2; dz <= 2; ++dz) {
            for (int dx = -2; dx <= 2; ++dx) {
                world.EnsureColumn(playerChunkX + dx, playerChunkZ + dz);
            }
        }
        BuildShowcase(world, playerChunkX * voxelgame::ChunkSection::Size + 12,
                      playerChunkZ * voxelgame::ChunkSection::Size - 6);
        for (const SectionKey& section : collectDirty(playerChunkX, playerChunkZ)) {
            meshSection(section[0], section[1], section[2]);
        }
        refreshTotals();

        if (meshError) {
            result = 4;
        } else {
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

                playerChunkX =
                    voxelgame::World::ToChunk(static_cast<int>(std::floor(player.Position().x)));
                playerChunkZ =
                    voxelgame::World::ToChunk(static_cast<int>(std::floor(player.Position().z)));
                stream(playerChunkX, playerChunkZ, kColumnBudget, kMeshBudgetMs);
                if (meshError) {
                    result = 5;
                    break;
                }

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
                    if (!voxelgame::IsCollidableBlock(world.GetBlock(px, py, pz)) &&
                        !player.Intersects(px, py, pz)) {
                        worldChanged = world.SetBlock(px, py, pz, kPalette[heldBlock]);
                    }
                }
                if (worldChanged) {
                    // Edits must feel instant -- mesh what this change dirtied now.
                    for (const SectionKey& section : collectDirty(playerChunkX, playerChunkZ)) {
                        meshSection(section[0], section[1], section[2]);
                    }
                    refreshTotals();
                    if (meshError) {
                        result = 5;
                        break;
                    }
                }

                BeginDrawing();
                ClearBackground(Color{18, 22, 31, 255});

                const voxelgame::Frustum frustum = voxelgame::MakeFrustum(
                    camera, static_cast<float>(GetScreenWidth()) /
                                static_cast<float>(GetScreenHeight()));
                int drawnSections = 0;

                // Only the sections in view, computed once for all three passes.
                std::vector<const voxelgame::ChunkRenderMesh*> visible;
                std::vector<Vector3> visiblePos;
                for (const auto& entry : meshes) {
                    const Vector3 origin{
                        static_cast<float>(entry.first[0] * voxelgame::ChunkSection::Size),
                        static_cast<float>(entry.first[1] * voxelgame::ChunkSection::Size),
                        static_cast<float>(entry.first[2] * voxelgame::ChunkSection::Size)};
                    constexpr float s = static_cast<float>(voxelgame::ChunkSection::Size);
                    if (!voxelgame::AabbInFrustum(frustum, origin,
                                                  {origin.x + s, origin.y + s, origin.z + s})) {
                        continue;
                    }
                    visible.push_back(&entry.second);
                    visiblePos.push_back(origin);
                }
                drawnSections = static_cast<int>(visible.size());

                BeginMode3D(camera);
                // Pass 1: opaque.
                voxelgame::SetTilingShaderAlphaCutoff(tilingShader, 0.0F);
                for (std::size_t k = 0; k < visible.size(); ++k) {
                    visible[k]->DrawLayer(visiblePos[k], voxelgame::RenderLayer::Opaque);
                }
                // Pass 2: cutout -- alpha-tested, still writes depth.
                voxelgame::SetTilingShaderAlphaCutoff(tilingShader, 0.5F);
                for (std::size_t k = 0; k < visible.size(); ++k) {
                    visible[k]->DrawLayer(visiblePos[k], voxelgame::RenderLayer::Cutout);
                }
                // Pass 3: transparent -- blended, depth test on, depth write off,
                // sections drawn back-to-front so overlapping panes composite right.
                voxelgame::SetTilingShaderAlphaCutoff(tilingShader, 0.0F);
                std::vector<std::size_t> order;
                for (std::size_t k = 0; k < visible.size(); ++k) {
                    if (visible[k]->HasLayer(voxelgame::RenderLayer::Transparent)) {
                        order.push_back(k);
                    }
                }
                const auto distSq = [&](const std::size_t k) {
                    const float cx = visiblePos[k].x + 8.0F - camera.position.x;
                    const float cy = visiblePos[k].y + 8.0F - camera.position.y;
                    const float cz = visiblePos[k].z + 8.0F - camera.position.z;
                    return cx * cx + cy * cy + cz * cz;
                };
                std::sort(order.begin(), order.end(),
                          [&](std::size_t a, std::size_t b) { return distSq(a) > distSq(b); });
                rlDrawRenderBatchActive();
                rlDisableDepthMask();
                BeginBlendMode(BLEND_ALPHA);
                for (const std::size_t k : order) {
                    visible[k]->DrawLayer(visiblePos[k], voxelgame::RenderLayer::Transparent);
                }
                EndBlendMode();
                rlDrawRenderBatchActive();
                rlEnableDepthMask();

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
                DrawText(TextFormat("Seed: 0x%X  Chunk: %i,%i  Columns: %i  Drawn: %i/%i",
                                    static_cast<unsigned>(seed), playerChunkX, playerChunkZ,
                                    static_cast<int>(world.LoadedColumnCount()), drawnSections,
                                    static_cast<int>(meshes.size())),
                         24, 104, 18, LIGHTGRAY);
                DrawText(TextFormat("Quads: %i  Tris: %i  Stream: %.2f ms  Rebuilds: %i",
                                    static_cast<int>(quadTotal), static_cast<int>(triangleTotal),
                                    meshMilliseconds, meshRebuilds),
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
