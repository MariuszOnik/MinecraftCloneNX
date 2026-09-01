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

    // Phase 1: loads `luaPath` and registers the bindings. Top-level script code
    // runs here, so `set_atlas(...)` takes effect before the caller meshes.
    bool Load(const std::string& luaPath, std::string& error);

    // Phase 2: calls `on_battle_start()` (unit placement) once the scene is set.
    bool Start(std::string& error);

    [[nodiscard]] bool Ok() const noexcept { return host_.Ok(); }

    // Atlas base name the script asked for (default "atlases/blocks").
    [[nodiscard]] const std::string& AtlasName() const noexcept { return atlasName_; }

private:
    static int LuaSpawn(lua_State* state);
    static int LuaSetAtlas(lua_State* state);

    BattleMap& map_;
    UnitRegistry& units_;
    script::LuaHost host_;
    std::string atlasName_ = "atlases/blocks";
};

}  // namespace voxelgame::battle
