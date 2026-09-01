#include "battle/BattleApp.hpp"

#include "battle/BattleMap.hpp"
#include "render/BlockAtlas.hpp"
#include "render/ChunkRenderMesh.hpp"
#include "render/TilingShader.hpp"
#include "world/BlockAtlasBinding.hpp"
#include "world/ChunkMesher.hpp"
#include "world/ChunkSection.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <vector>

namespace voxelgame::battle {
namespace {

constexpr float kIsoYaw = 0.78539816F;
constexpr float kIsoPitch = 0.61547971F;
constexpr float kCameraDistance = 80.0F;

bool HasArg(const int argc, char* argv[], const char* name) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0) {
            return true;
        }
    }
    return false;
}

const char* ArgValue(const int argc, char* argv[], const char* name) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0) {
            return argv[i + 1];
        }
    }
    return nullptr;
}

Vector3 IsoDirection() {
    return {std::sin(kIsoYaw) * std::cos(kIsoPitch), -std::sin(kIsoPitch),
            -std::cos(kIsoYaw) * std::cos(kIsoPitch)};
}

// Draws the outline of one tile's top face, lifted slightly to avoid z-fighting.
void DrawTileOutline(const int tileX, const int tileZ, const float y, const Color color) {
    const float x0 = static_cast<float>(tileX) + 0.05F;
    const float z0 = static_cast<float>(tileZ) + 0.05F;
    const float x1 = static_cast<float>(tileX) + 0.95F;
    const float z1 = static_cast<float>(tileZ) + 0.95F;
    const float h = y + 0.03F;
    DrawLine3D({x0, h, z0}, {x1, h, z0}, color);
    DrawLine3D({x1, h, z0}, {x1, h, z1}, color);
    DrawLine3D({x1, h, z1}, {x0, h, z1}, color);
    DrawLine3D({x0, h, z1}, {x0, h, z0}, color);
}

}  // namespace

int RunBattle(int argc, char* argv[]) {
    const bool smokeWindow = HasArg(argc, argv, "--smoke-window");
    const char* screenshot = ArgValue(argc, argv, "--screenshot");

    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(960, 540, "VoxelGame - battle");
    if (!IsWindowReady()) {
        return 3;
    }
    SetTargetFPS(60);

    // Procedural atlas + tiling shader (SD-card overrides are a later concern).
    Image atlasImage = GenerateBlockAtlasImage();
    const Texture2D atlas = LoadTextureFromImage(atlasImage);
    UnloadImage(atlasImage);
    SetTextureFilter(atlas, TEXTURE_FILTER_POINT);

    const Shader tilingShader = LoadTilingShader();
    const BlockAtlasBinding binding;
    SetTilingShaderExtent(tilingShader, binding.TileExtentU(), binding.TileExtentV());

    int result = 0;
    {
        BattleMap map;
        const ChunkMesher mesher;

        // Mesh every section of the (small, static) arena once.
        using SectionKey = std::array<int, 3>;
        std::map<SectionKey, ChunkRenderMesh> meshes;
        bool meshError = false;
        const int sectionsY = map.GetWorld().SectionsY();
        const int chunkSpanX = (map.OriginX() + map.SizeX() - 1) / ChunkSection::Size + 1;
        const int chunkSpanZ = (map.OriginZ() + map.SizeZ() - 1) / ChunkSection::Size + 1;
        for (int cz = 0; cz < chunkSpanZ; ++cz) {
            for (int cx = 0; cx < chunkSpanX; ++cx) {
                for (int sy = 0; sy < sectionsY; ++sy) {
                    const SectionMesh data = mesher.Build(map.GetWorld(), cx, sy, cz, binding);
                    if (data.Empty()) {
                        continue;
                    }
                    const SectionKey key{cx, sy, cz};
                    if (!meshes[key].Upload(data, atlas, tilingShader)) {
                        meshError = true;
                    }
                }
            }
        }

        if (meshError) {
            result = 4;
        } else {
            const Vector3 isoDir = IsoDirection();
            const Vector3 mapCentre{static_cast<float>(map.OriginX()) + map.SizeX() * 0.5F, 5.0F,
                                    static_cast<float>(map.OriginZ()) + map.SizeZ() * 0.5F};
            Vector3 pan{0.0F, 0.0F, 0.0F};
            float orthoHeight = static_cast<float>(map.SizeX());

            Camera3D camera{};
            camera.up = {0.0F, 1.0F, 0.0F};
            camera.projection = CAMERA_ORTHOGRAPHIC;

            int frames = 0;
            while (!WindowShouldClose()) {
                const float dt = std::min(GetFrameTime(), 0.05F);
                const float panSpeed = 12.0F * dt;
                float panX = 0.0F;
                float panZ = 0.0F;
                if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) panX += 1.0F;
                if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) panX -= 1.0F;
                if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) panZ += 1.0F;
                if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) panZ -= 1.0F;
                float zoom = GetMouseWheelMove() * 2.0F;
                if (IsGamepadAvailable(0)) {
                    panX += GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
                    panZ += GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
                    zoom += (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1) ? 1.0F : 0.0F) -
                            (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1) ? 1.0F : 0.0F);
                }
                pan.x += std::clamp(panX, -1.0F, 1.0F) * panSpeed;
                pan.z += std::clamp(panZ, -1.0F, 1.0F) * panSpeed;
                orthoHeight = std::clamp(orthoHeight - zoom, 8.0F, 48.0F);

                const Vector3 focus{mapCentre.x + pan.x, mapCentre.y, mapCentre.z + pan.z};
                camera.position = {focus.x - isoDir.x * kCameraDistance,
                                   focus.y - isoDir.y * kCameraDistance,
                                   focus.z - isoDir.z * kCameraDistance};
                camera.target = focus;
                camera.fovy = orthoHeight;

                BeginDrawing();
                ClearBackground(Color{20, 24, 33, 255});
                BeginMode3D(camera);

                SetTilingShaderAlphaCutoff(tilingShader, 0.0F);
                for (auto& [key, mesh] : meshes) {
                    const Vector3 origin{static_cast<float>(key[0] * ChunkSection::Size),
                                         static_cast<float>(key[1] * ChunkSection::Size),
                                         static_cast<float>(key[2] * ChunkSection::Size)};
                    mesh.DrawLayer(origin, RenderLayer::Opaque);
                }
                SetTilingShaderAlphaCutoff(tilingShader, 0.5F);
                for (auto& [key, mesh] : meshes) {
                    const Vector3 origin{static_cast<float>(key[0] * ChunkSection::Size),
                                         static_cast<float>(key[1] * ChunkSection::Size),
                                         static_cast<float>(key[2] * ChunkSection::Size)};
                    mesh.DrawLayer(origin, RenderLayer::Cutout);
                }
                SetTilingShaderAlphaCutoff(tilingShader, 0.0F);
                rlDrawRenderBatchActive();
                rlDisableDepthMask();
                BeginBlendMode(BLEND_ALPHA);
                for (const RenderLayer layer : {RenderLayer::Liquid, RenderLayer::Transparent}) {
                    for (auto& [key, mesh] : meshes) {
                        const Vector3 origin{static_cast<float>(key[0] * ChunkSection::Size),
                                             static_cast<float>(key[1] * ChunkSection::Size),
                                             static_cast<float>(key[2] * ChunkSection::Size)};
                        mesh.DrawLayer(origin, layer);
                    }
                }
                EndBlendMode();
                rlDrawRenderBatchActive();
                rlEnableDepthMask();

                // Tile grid: green outline on walkable tiles, dim red on tiles
                // that have a floor but are blocked.
                const TileGrid& grid = map.Grid();
                for (int tz = grid.OriginZ(); tz < grid.OriginZ() + grid.SizeZ(); ++tz) {
                    for (int tx = grid.OriginX(); tx < grid.OriginX() + grid.SizeX(); ++tx) {
                        const Tile& tile = grid.At(tx, tz);
                        if (tile.walkable) {
                            DrawTileOutline(tx, tz, static_cast<float>(tile.height),
                                            Color{90, 220, 120, 200});
                        } else if (tile.hasFloor) {
                            DrawTileOutline(tx, tz, static_cast<float>(tile.height),
                                            Color{200, 90, 80, 120});
                        }
                    }
                }

                EndMode3D();

                DrawRectangle(12, 12, 360, 96, Fade(BLACK, 0.72F));
                DrawText("BATTLE / ARENA (S1)", 24, 22, 22, LIME);
                DrawText(TextFormat("Arena: %ix%i  Walkable tiles: %i", map.SizeX(), map.SizeZ(),
                                    static_cast<int>(grid.WalkableCount())),
                         24, 52, 18, RAYWHITE);
                DrawText("Arrows/WASD pan   wheel zoom", 24, 78, 16, GRAY);

                EndDrawing();

                ++frames;
                if (smokeWindow && frames >= 6) {
                    if (screenshot != nullptr) {
                        TakeScreenshot(screenshot);
                    }
                    break;
                }
            }
        }
    }

    UnloadShader(tilingShader);
    UnloadTexture(atlas);
    CloseWindow();
    return result;
}

}  // namespace voxelgame::battle
