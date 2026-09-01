#include "battle/BattleApp.hpp"

#include "battle/BattleCamera.hpp"
#include "battle/BattleMap.hpp"
#include "battle/BattleScript.hpp"
#include "battle/Unit.hpp"
#include "battle/render/UnitRenderer.hpp"
#include "platform/Assets.hpp"
#include "render/AtlasLoad.hpp"
#include "render/ChunkRenderMesh.hpp"
#include "render/TilingShader.hpp"
#include "world/ChunkMesher.hpp"
#include "world/ChunkSection.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>

namespace voxelgame::battle {
namespace {

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

// Spawns each team's units on its map spawn tiles, facing the other side, and
// marks the tiles as occupied.
void SpawnUnits(const BattleMap& map, UnitRegistry& units, TileGrid& grid) {
    for (int team = 0; team < 2; ++team) {
        const auto& enemy = map.Spawns(1 - team);
        const int lookX = enemy.empty() ? 0 : enemy.front().first;
        const int lookZ = enemy.empty() ? 0 : enemy.front().second;
        for (const auto& [tx, tz] : map.Spawns(team)) {
            Unit unit;
            unit.team = team;
            unit.tileX = tx;
            unit.tileZ = tz;
            unit.facing = FacingTowards(tx, tz, lookX, lookZ);
            unit.hpMax = 10;
            unit.hp = 10;
            const UnitHandle handle = units.Spawn(unit);
            grid.At(tx, tz).occupant = handle.index;
        }
    }
}

}  // namespace

int RunBattle(int argc, char* argv[]) {
    const bool smokeWindow = HasArg(argc, argv, "--smoke-window");
    const char* screenshot = ArgValue(argc, argv, "--screenshot");

    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(960, 540, "Voxel Tactics");
    if (!IsWindowReady()) {
        return 3;
    }
    SetTargetFPS(60);

    const AssetPaths assets(GetApplicationDirectory());
    const Shader tilingShader = LoadTilingShader();

    int result = 0;
    {
        // The battle NRO ships no assets -- they live on the SD card. Warn early
        // if the deploy step was skipped.
        const bool assetsPresent = assets.Resolve("models/humanoid.vxm").found ||
                                   assets.Resolve("models/humanoid.vxm.json").found;
        if (!assetsPresent) {
            TraceLog(LOG_WARNING,
                     "BATTLE: assets not found -- deploy them to the SD card "
                     "(sdmc:/switch/voxeltactics/assets/)");
        }

        BattleMap map;
        UnitRegistry units;
        UnitRenderer unitRenderer(assets);

        // Phase 1: load the script (top-level code runs, so set_atlas takes
        // effect). A failure is logged loudly; placement falls back below.
        BattleScript scripting(map, units);
        const AssetPaths::Resolved scriptFile =
            assets.Resolve("scripts/battles/skirmish01.lua");
        std::string scriptError;
        bool scriptOk = scriptFile.found && scripting.Load(scriptFile.path, scriptError);

        // The atlas the scene asked for (SD-card first), then the tiling shader
        // extent for that grid.
        const LoadedAtlas atlas = LoadBlockAtlas(assets, scripting.AtlasName());
        SetTilingShaderExtent(tilingShader, atlas.binding.TileExtentU(),
                              atlas.binding.TileExtentV());

        // Mesh the small static arena once with the scene atlas.
        using SectionKey = std::array<int, 3>;
        std::map<SectionKey, ChunkRenderMesh> meshes;
        bool meshError = false;
        const ChunkMesher mesher;
        const int sectionsY = map.GetWorld().SectionsY();
        const int chunkSpanX = (map.OriginX() + map.SizeX() - 1) / ChunkSection::Size + 1;
        const int chunkSpanZ = (map.OriginZ() + map.SizeZ() - 1) / ChunkSection::Size + 1;
        for (int cz = 0; cz < chunkSpanZ; ++cz) {
            for (int cx = 0; cx < chunkSpanX; ++cx) {
                for (int sy = 0; sy < sectionsY; ++sy) {
                    const SectionMesh data =
                        mesher.Build(map.GetWorld(), cx, sy, cz, atlas.binding);
                    if (data.Empty()) {
                        continue;
                    }
                    if (!meshes[SectionKey{cx, sy, cz}].Upload(data, atlas.texture, tilingShader)) {
                        meshError = true;
                    }
                }
            }
        }

        // Phase 2: the script places the units. Loud log + hard-coded fallback.
        if (scriptOk && !scripting.Start(scriptError)) {
            scriptOk = false;
        }
        if (!scriptOk) {
            TraceLog(LOG_ERROR, "BATTLE: script '%s' failed (%s); using default placement",
                     scriptFile.path.c_str(),
                     scriptError.empty() ? "not found" : scriptError.c_str());
            SpawnUnits(map, units, map.Grid());
        }

        if (meshError) {
            result = 4;
        } else {
            const auto drawArenaLayer = [&](const RenderLayer layer) {
                for (auto& [key, mesh] : meshes) {
                    mesh.DrawLayer({static_cast<float>(key[0] * ChunkSection::Size),
                                    static_cast<float>(key[1] * ChunkSection::Size),
                                    static_cast<float>(key[2] * ChunkSection::Size)},
                                   layer);
                }
            };

            BattleCamera camera;
            camera.SetBounds(static_cast<float>(map.OriginX()), static_cast<float>(map.OriginZ()),
                             static_cast<float>(map.OriginX() + map.SizeX()),
                             static_cast<float>(map.OriginZ() + map.SizeZ()));
            camera.FollowInstant({static_cast<float>(map.OriginX()) + map.SizeX() * 0.5F, 5.0F,
                                  static_cast<float>(map.OriginZ()) + map.SizeZ() * 0.5F});

            int frames = 0;
            while (!WindowShouldClose()) {
                const float dt = std::min(GetFrameTime(), 0.05F);

                float panF = 0.0F;
                float panR = 0.0F;
                if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) panF += 1.0F;
                if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) panF -= 1.0F;
                if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) panR += 1.0F;
                if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) panR -= 1.0F;
                float zoom = GetMouseWheelMove();
                if (IsGamepadAvailable(0)) {
                    panR += GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
                    panF -= GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
                    zoom += (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1) ? 1.0F : 0.0F) -
                            (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1) ? 1.0F : 0.0F);
                }
                if (IsKeyPressed(KEY_Q) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_TRIGGER_2)) {
                    camera.RotateLeft();
                }
                if (IsKeyPressed(KEY_E) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_2)) {
                    camera.RotateRight();
                }
                camera.Pan(std::clamp(panF, -1.0F, 1.0F), std::clamp(panR, -1.0F, 1.0F), dt);
                camera.Zoom(zoom);
                camera.Update(dt);
                unitRenderer.Update(dt);

                BeginDrawing();
                ClearBackground(Color{20, 24, 33, 255});
                BeginMode3D(camera.Camera());

                SetTilingShaderAlphaCutoff(tilingShader, 0.0F);
                drawArenaLayer(RenderLayer::Opaque);
                SetTilingShaderAlphaCutoff(tilingShader, 0.5F);
                drawArenaLayer(RenderLayer::Cutout);
                SetTilingShaderAlphaCutoff(tilingShader, 0.0F);

                unitRenderer.Draw(units, map.Grid());

                rlDrawRenderBatchActive();
                rlDisableDepthMask();
                BeginBlendMode(BLEND_ALPHA);
                drawArenaLayer(RenderLayer::Liquid);
                drawArenaLayer(RenderLayer::Transparent);
                EndBlendMode();
                rlDrawRenderBatchActive();
                rlEnableDepthMask();

                const TileGrid& grid = map.Grid();
                for (int tz = grid.OriginZ(); tz < grid.OriginZ() + grid.SizeZ(); ++tz) {
                    for (int tx = grid.OriginX(); tx < grid.OriginX() + grid.SizeX(); ++tx) {
                        const Tile& tile = grid.At(tx, tz);
                        if (tile.walkable) {
                            DrawTileOutline(tx, tz, static_cast<float>(tile.height),
                                            Color{90, 220, 120, 90});
                        }
                    }
                }

                EndMode3D();

                DrawRectangle(12, 12, 380, 96, Fade(BLACK, 0.72F));
                DrawText("VOXEL TACTICS  (S3)", 24, 22, 22, LIME);
                DrawText(TextFormat("Units: %i blue / %i red   Placement: %s",
                                    static_cast<int>(units.TeamCount(0)),
                                    static_cast<int>(units.TeamCount(1)),
                                    scriptOk ? "skirmish01.lua" : "default (script failed)"),
                         24, 52, 18, scriptOk ? RAYWHITE : Color{240, 150, 90, 255});
                DrawText("WASD/stick pan   Q/E rotate   wheel/ZL-ZR zoom", 24, 78, 16, GRAY);

                EndDrawing();

                ++frames;
                if (smokeWindow && frames >= 12) {
                    if (screenshot != nullptr) {
                        TakeScreenshot(screenshot);
                    }
                    break;
                }
            }
        }

        UnloadTexture(atlas.texture);
    }

    UnloadShader(tilingShader);
    CloseWindow();
    return result;
}

}  // namespace voxelgame::battle
