#pragma once

#include "script/LuaHost.hpp"

#include <string>

namespace voxelgame::battle {

class BattleMap;
class UnitRegistry;

// Hosts the Lua battle script and the C bindings it drives. S3: a `spawn` global
// so unit placement lives in the script. The full Battle / Unit API grows here.
class BattleScript {
public:
    BattleScript(BattleMap& map, UnitRegistry& units) noexcept;

    // Loads `luaPath`, registers the bindings, and calls `on_battle_start()`.
    // false + `error` on any load / runtime failure.
    bool LoadBattle(const std::string& luaPath, std::string& error);

    [[nodiscard]] bool Ok() const noexcept { return host_.Ok(); }

private:
    static int LuaSpawn(lua_State* state);

    BattleMap& map_;
    UnitRegistry& units_;
    script::LuaHost host_;
};

}  // namespace voxelgame::battle
