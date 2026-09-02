#pragma once

#include "script/LuaHost.hpp"

#include <string>
#include <vector>

namespace voxelgame::battle {

class BattleMap;
class UnitRegistry;

// Hosts the Lua battle script and the C bindings it drives. Unit placement, turn
// order and the victory check all live in the script; C++ calls these entry
// points and exposes read-only unit queries as bindings.
class BattleScript {
public:
    BattleScript(BattleMap& map, UnitRegistry& units) noexcept;

    // Phase 1: loads `luaPath` and registers the bindings. Top-level script code
    // runs here, so `set_atlas(...)` takes effect before the caller meshes.
    bool Load(const std::string& luaPath, std::string& error);

    // Phase 2: calls `on_battle_start()` (unit placement) once the scene is set.
    bool Start(std::string& error);

    [[nodiscard]] bool Ok() const noexcept { return host_.Ok(); }
    [[nodiscard]] const std::string& AtlasName() const noexcept { return atlasName_; }

    // Turn flow. A missing Lua function is not an error -- sensible defaults
    // apply (spawn-order initiative, no turn-start effect, team-wipe victory).
    [[nodiscard]] std::vector<int> InitiativeOrder();
    void OnTurnBegin(int unitIndex);
    [[nodiscard]] std::string CheckVictory();  // "", "player", "enemy"

private:
    static int LuaSpawn(lua_State* state);
    static int LuaSetAtlas(lua_State* state);
    static int LuaUnitTile(lua_State* state);
    static int LuaUnitTeam(lua_State* state);
    static int LuaUnitAlive(lua_State* state);
    static int LuaCountTeam(lua_State* state);

    BattleMap& map_;
    UnitRegistry& units_;
    script::LuaHost host_;
    std::string atlasName_ = "atlases/blocks";
};

}  // namespace voxelgame::battle
