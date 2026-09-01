#include "battle/BattleScript.hpp"

#include "battle/BattleMap.hpp"
#include "battle/Unit.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace voxelgame::battle {

BattleScript::BattleScript(BattleMap& map, UnitRegistry& units) noexcept
    : map_(map), units_(units) {}

// spawn(team, tileX, tileZ) -> places a unit, marks the tile occupied, faces it
// toward the arena centre.
int BattleScript::LuaSpawn(lua_State* state) {
    auto* self = static_cast<BattleScript*>(lua_touserdata(state, lua_upvalueindex(1)));
    const int team = static_cast<int>(luaL_checkinteger(state, 1));
    const int tx = static_cast<int>(luaL_checkinteger(state, 2));
    const int tz = static_cast<int>(luaL_checkinteger(state, 3));

    if (self == nullptr) {
        return luaL_error(state, "spawn: no battle context");
    }
    TileGrid& grid = self->map_.Grid();
    if (!grid.InBounds(tx, tz)) {
        return luaL_error(state, "spawn: tile (%d,%d) is off the map", tx, tz);
    }

    const int centreX = self->map_.OriginX() + self->map_.SizeX() / 2;
    const int centreZ = self->map_.OriginZ() + self->map_.SizeZ() / 2;

    Unit unit;
    unit.team = team;
    unit.tileX = tx;
    unit.tileZ = tz;
    unit.facing = FacingTowards(tx, tz, centreX, centreZ);
    unit.hpMax = 10;
    unit.hp = 10;

    const UnitHandle handle = self->units_.Spawn(unit);
    grid.At(tx, tz).occupant = handle.index;
    lua_pushinteger(state, handle.index);
    return 1;
}

// set_atlas("atlases/blocks") -- chooses the block atlas for this scene. The
// name is a base (no extension); the engine resolves "<name>.json" + its PNG,
// SD-card first.
int BattleScript::LuaSetAtlas(lua_State* state) {
    auto* self = static_cast<BattleScript*>(lua_touserdata(state, lua_upvalueindex(1)));
    const char* name = luaL_checkstring(state, 1);
    if (self != nullptr && name != nullptr) {
        self->atlasName_ = name;
    }
    return 0;
}

bool BattleScript::Load(const std::string& luaPath, std::string& error) {
    if (!host_.Ok()) {
        error = "Lua state failed to start";
        return false;
    }
    host_.RegisterFunction("spawn", &LuaSpawn, this);
    host_.RegisterFunction("set_atlas", &LuaSetAtlas, this);
    return host_.DoFile(luaPath, error);
}

bool BattleScript::Start(std::string& error) {
    return host_.Ok() && host_.CallGlobal("on_battle_start", error);
}

}  // namespace voxelgame::battle
