#include "battle/BattleApp.hpp"

#include "battle/BattleCamera.hpp"
#include "battle/BattleMap.hpp"
#include "battle/BattleScript.hpp"
#include "battle/Unit.hpp"
#include "battle/render/UnitRenderer.hpp"
#include "battle/Pathfind.hpp"
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
#include <utility>
#include <vector>

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

            TileGrid& grid = map.Grid();  // occupancy mutates as units move
            const int gx0 = grid.OriginX();
            const int gz0 = grid.OriginZ();
            const int gx1 = gx0 + grid.SizeX() - 1;
            const int gz1 = gz0 + grid.SizeZ() - 1;
            int cursorX = (gx0 + gx1) / 2;
            int cursorZ = (gz0 + gz1) / 2;
            int selected = -1;  // unit slot index, -1 = none
            float stickRepeat = 0.0F;

            struct Walk {
                int unit = -1;
                std::vector<PathStep> path;
                float t = 0.0F;  // tiles travelled along the path
            } walk;

            const auto tileWorld = [&](const int tx, const int tz) {
                return Vector3{static_cast<float>(tx) + 0.5F,
                               static_cast<float>(grid.At(tx, tz).height),
                               static_cast<float>(tz) + 0.5F};
            };
            const auto moveCursor = [&](const int dx, const int dz) {
                cursorX = std::clamp(cursorX + dx, gx0, gx1);
                cursorZ = std::clamp(cursorZ + dz, gz0, gz1);
            };
            // Moves the cursor toward a screen direction. At our 45-degree iso
            // rotations the four world axes project to screen *diagonals*, so we
            // search the 8 neighbours (the cursor may step diagonally -- it is a
            // pointer, not a path) and pick the one whose screen projection best
            // matches. Works at every camera rotation, immune to stick sign.
            const auto stepForScreen = [&](const float sdx, const float sdy) {
                const Camera3D cam = camera.Camera();
                const Vector3 base = tileWorld(cursorX, cursorZ);
                const Vector2 o = GetWorldToScreen(base, cam);
                float best = 0.30F;  // require a real match, not a near-perpendicular one
                int bx = 0;
                int bz = 0;
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dz == 0) {
                            continue;
                        }
                        const Vector2 p = GetWorldToScreen(
                            {base.x + static_cast<float>(dx), base.y, base.z + static_cast<float>(dz)},
                            cam);
                        const float vx = p.x - o.x;
                        const float vy = p.y - o.y;
                        const float len = std::sqrt(vx * vx + vy * vy);
                        if (len < 1.0e-4F) {
                            continue;
                        }
                        const float score = (vx / len) * sdx + (vy / len) * sdy;
                        if (score > best) {
                            best = score;
                            bx = dx;
                            bz = dz;
                        }
                    }
                }
                moveCursor(bx, bz);
            };

            if (smokeWindow) {
                // Auto-select a player unit and start a short walk so the
                // screenshot catches a unit mid-move.
                units.ForEach([&](UnitHandle h, const Unit& u) {
                    if (selected < 0 && u.team == 0) {
                        selected = h.index;
                    }
                });
                const Unit* u = units.AtIndex(selected);
                if (u != nullptr) {
                    const auto p = ComputePath(grid, u->tileX, u->tileZ, u->tileX + 2,
                                               u->tileZ + 2, u->jumpHeight);
                    if (p.size() >= 2) {
                        walk.unit = selected;
                        walk.path = p;
                        grid.At(u->tileX, u->tileZ).occupant = -1;
                        unitRenderer.BeginWalk(selected);
                    }
                }
            }

            int frames = 0;
            while (!WindowShouldClose()) {
                const float dt = std::min(GetFrameTime(), 0.05F);
                const bool pad = IsGamepadAvailable(0);
                const bool walking = walk.unit >= 0;

#if defined(__SWITCH__)
                constexpr float kStickYSign = -1.0F;  // libnx reports "up" as +y
#else
                constexpr float kStickYSign = 1.0F;
#endif

                // --- cursor: d-pad / arrows step it one tile in the matching
                // screen direction; the left stick steps it on a repeat. Frozen
                // while a unit walks. ---
                if (!walking) {
                    if (IsKeyPressed(KEY_UP) ||
                        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) {
                        stepForScreen(0.0F, -1.0F);
                    }
                    if (IsKeyPressed(KEY_DOWN) ||
                        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) {
                        stepForScreen(0.0F, 1.0F);
                    }
                    if (IsKeyPressed(KEY_LEFT) ||
                        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) {
                        stepForScreen(-1.0F, 0.0F);
                    }
                    if (IsKeyPressed(KEY_RIGHT) ||
                        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) {
                        stepForScreen(1.0F, 0.0F);
                    }
                    stickRepeat -= dt;
                    if (pad) {
                        const float sx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
                        const float sy =
                            GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) * kStickYSign;
                        if (std::abs(sx) < 0.4F && std::abs(sy) < 0.4F) {
                            stickRepeat = 0.0F;
                        } else if (stickRepeat <= 0.0F) {
                            const bool horiz = std::abs(sx) >= std::abs(sy);
                            stepForScreen(horiz ? (sx > 0.0F ? 1.0F : -1.0F) : 0.0F,
                                          horiz ? 0.0F : (sy > 0.0F ? 1.0F : -1.0F));
                            stickRepeat = 0.15F;
                        }
                    }
                }

                float zoom = GetMouseWheelMove();
                float panF = 0.0F;
                float panR = 0.0F;
                if (IsKeyDown(KEY_W)) panF += 1.0F;
                if (IsKeyDown(KEY_S)) panF -= 1.0F;
                if (IsKeyDown(KEY_D)) panR += 1.0F;
                if (IsKeyDown(KEY_A)) panR -= 1.0F;
                if (pad) {
                    zoom += (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1) ? 1.0F : 0.0F) -
                            (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1) ? 1.0F : 0.0F);
                    panR += GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X);
                    panF -= GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) * kStickYSign;
                }
                if (IsKeyPressed(KEY_Q) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_TRIGGER_2)) {
                    camera.RotateLeft();
                }
                if (IsKeyPressed(KEY_E) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_2)) {
                    camera.RotateRight();
                }
                camera.Zoom(zoom);
                camera.Pan(std::clamp(panF, -1.0F, 1.0F), std::clamp(panR, -1.0F, 1.0F), dt);
                camera.Update(dt);
                unitRenderer.Update(dt);

                // Mouse cursor (PC): the topmost tile the ray crosses.
                const Ray ray = GetScreenToWorldRay(GetMousePosition(), camera.Camera());
                if (!smokeWindow && !pad && !walking && std::abs(ray.direction.y) > 1.0e-5F) {
                    float bestT = 1.0e9F;
                    for (int tz = grid.OriginZ(); tz < grid.OriginZ() + grid.SizeZ(); ++tz) {
                        for (int tx = grid.OriginX(); tx < grid.OriginX() + grid.SizeX(); ++tx) {
                            const Tile& tile = grid.At(tx, tz);
                            if (!tile.hasFloor) {
                                continue;
                            }
                            const float hitT =
                                (static_cast<float>(tile.height) - ray.position.y) / ray.direction.y;
                            if (hitT <= 0.0F || hitT >= bestT) {
                                continue;
                            }
                            const float hx = ray.position.x + ray.direction.x * hitT;
                            const float hz = ray.position.z + ray.direction.z * hitT;
                            if (hx >= static_cast<float>(tx) && hx < static_cast<float>(tx + 1) &&
                                hz >= static_cast<float>(tz) && hz < static_cast<float>(tz + 1)) {
                                bestT = hitT;
                                cursorX = tx;
                                cursorZ = tz;
                            }
                        }
                    }
                }

                // Movement range of the selected unit, and a path preview.
                ReachableSet reach(gx0, gz0, grid.SizeX(), grid.SizeZ());
                std::vector<PathStep> preview;
                const Unit* sel = units.AtIndex(selected);
                if (sel != nullptr && !walking) {
                    reach = ComputeReachable(grid, sel->tileX, sel->tileZ, sel->moveTiles,
                                             sel->jumpHeight);
                    if (reach.Contains(cursorX, cursorZ)) {
                        preview = ComputePath(grid, sel->tileX, sel->tileZ, cursorX, cursorZ,
                                              sel->jumpHeight);
                    }
                }

                // Confirm / cancel (LMB / A, RMB / B).
                const bool confirm = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
                                     IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
                const bool cancel = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
                                    IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
                if (!walking && cancel) {
                    selected = -1;
                } else if (!walking && confirm) {
                    const int occ = grid.At(cursorX, cursorZ).occupant;
                    const Unit* onTile = units.AtIndex(occ);
                    if (sel != nullptr && occ < 0 && reach.Contains(cursorX, cursorZ) &&
                        !preview.empty()) {
                        // Walk the selected unit to the target tile.
                        walk.unit = selected;
                        walk.path = preview;
                        walk.t = 0.0F;
                        grid.At(sel->tileX, sel->tileZ).occupant = -1;
                        unitRenderer.BeginWalk(walk.unit);
                    } else if (onTile != nullptr && onTile->team == 0) {
                        selected = (selected == occ) ? -1 : occ;
                        if (selected >= 0) {
                            camera.Follow(tileWorld(onTile->tileX, onTile->tileZ));
                        }
                    } else {
                        selected = -1;
                    }
                }

                // Advance a walk in progress.
                if (walk.unit >= 0) {
                    Unit* mover = units.AtIndex(walk.unit);
                    constexpr float kWalkSpeed = 4.5F;  // tiles per second
                    walk.t += kWalkSpeed * dt;
                    const int seg = static_cast<int>(walk.t);
                    if (mover == nullptr || walk.path.size() < 2 ||
                        seg >= static_cast<int>(walk.path.size()) - 1) {
                        if (mover != nullptr && walk.path.size() >= 2) {
                            const PathStep prev = walk.path[walk.path.size() - 2];
                            const PathStep goal = walk.path.back();
                            mover->tileX = goal.x;
                            mover->tileZ = goal.z;
                            mover->facing = FacingTowards(prev.x, prev.z, goal.x, goal.z);
                            grid.At(goal.x, goal.z).occupant = walk.unit;
                        }
                        unitRenderer.EndWalk();
                        walk.unit = -1;
                        selected = -1;  // one action per unit for now
                    } else {
                        const float frac = walk.t - static_cast<float>(seg);
                        const PathStep a = walk.path[static_cast<std::size_t>(seg)];
                        const PathStep b = walk.path[static_cast<std::size_t>(seg) + 1];
                        const Vector3 pa = tileWorld(a.x, a.z);
                        const Vector3 pb = tileWorld(b.x, b.z);
                        const Vector3 pos{pa.x + (pb.x - pa.x) * frac, pa.y + (pb.y - pa.y) * frac,
                                          pa.z + (pb.z - pa.z) * frac};
                        const float yaw = std::atan2(static_cast<float>(b.x - a.x),
                                                     -static_cast<float>(b.z - a.z));
                        unitRenderer.SetWalk(pos, yaw);
                        camera.Follow(pos);
                    }
                }

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

                // Movement-range fill for the selected unit.
                rlDrawRenderBatchActive();
                rlDisableDepthMask();
                BeginBlendMode(BLEND_ALPHA);
                for (const PathStep& t : reach.Tiles()) {
                    const float h = static_cast<float>(grid.At(t.x, t.z).height) + 0.03F;
                    const Vector3 c{static_cast<float>(t.x) + 0.5F, h,
                                    static_cast<float>(t.z) + 0.5F};
                    DrawPlane(c, {0.94F, 0.94F}, Color{70, 140, 255, 150});
                    DrawTileOutline(t.x, t.z, h, Color{150, 200, 255, 200});
                }
                for (const PathStep& t : preview) {
                    const float h = static_cast<float>(grid.At(t.x, t.z).height) + 0.06F;
                    DrawPlane({static_cast<float>(t.x) + 0.5F, h, static_cast<float>(t.z) + 0.5F},
                              {0.55F, 0.55F}, Color{255, 240, 130, 210});
                }
                EndBlendMode();
                rlDrawRenderBatchActive();
                rlEnableDepthMask();

                // Selected unit's tile and the cursor (hidden during a walk).
                if (sel != nullptr && !walking) {
                    DrawTileOutline(sel->tileX, sel->tileZ,
                                    static_cast<float>(grid.At(sel->tileX, sel->tileZ).height),
                                    Color{120, 255, 150, 255});
                }
                if (!walking) {
                    DrawTileOutline(cursorX, cursorZ,
                                    static_cast<float>(grid.At(cursorX, cursorZ).height),
                                    Color{255, 235, 90, 255});
                }

                EndMode3D();

                DrawRectangle(12, 12, 396, 118, Fade(BLACK, 0.72F));
                DrawText("VOXEL TACTICS  (S5)", 24, 22, 22, LIME);
                DrawText(TextFormat("Units: %i blue / %i red   Placement: %s",
                                    static_cast<int>(units.TeamCount(0)),
                                    static_cast<int>(units.TeamCount(1)),
                                    scriptOk ? "skirmish01.lua" : "default"),
                         24, 50, 17, scriptOk ? RAYWHITE : Color{240, 150, 90, 255});
                if (walking) {
                    DrawText("Moving...", 24, 72, 17, Color{255, 235, 120, 255});
                } else if (sel != nullptr) {
                    DrawText(TextFormat("Selected  move %i  jump %i  range %i   A to walk to cursor",
                                        sel->moveTiles, sel->jumpHeight,
                                        static_cast<int>(reach.Tiles().size())),
                             24, 72, 17, Color{140, 235, 170, 255});
                } else {
                    DrawText(TextFormat("Cursor %i,%i   A on a blue unit to select", cursorX,
                                        cursorZ),
                             24, 72, 17, LIGHTGRAY);
                }
                DrawText("D-pad/arrows cursor   A select/move   B cancel   Q/E rotate   R-stick pan",
                         24, 100, 15, GRAY);

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
