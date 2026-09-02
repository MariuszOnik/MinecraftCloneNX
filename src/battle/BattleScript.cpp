#include "battle/BattleScript.hpp"

#include "battle/BattleMap.hpp"
#include "battle/Unit.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace voxelgame::battle {
namespace {

BattleScript* Self(lua_State* state) {
    return static_cast<BattleScript*>(lua_touserdata(state, lua_upvalueindex(1)));
}

}  // namespace

BattleScript::BattleScript(BattleMap& map, UnitRegistry& units) noexcept
    : map_(map), units_(units) {}

int BattleScript::LuaUnitTile(lua_State* state) {
    BattleScript* self = Self(state);
    const Unit* u = self != nullptr
                        ? self->units_.AtIndex(static_cast<int>(luaL_checkinteger(state, 1)))
                        : nullptr;
    if (u == nullptr) {
        return 0;
    }
    lua_pushinteger(state, u->tileX);
    lua_pushinteger(state, u->tileZ);
    return 2;
}

int BattleScript::LuaUnitTeam(lua_State* state) {
    BattleScript* self = Self(state);
    const Unit* u = self != nullptr
                        ? self->units_.AtIndex(static_cast<int>(luaL_checkinteger(state, 1)))
                        : nullptr;
    lua_pushinteger(state, u != nullptr ? u->team : -1);
    return 1;
}

int BattleScript::LuaUnitAlive(lua_State* state) {
    BattleScript* self = Self(state);
    const bool alive =
        self != nullptr &&
        self->units_.AtIndex(static_cast<int>(luaL_checkinteger(state, 1))) != nullptr;
    lua_pushboolean(state, alive ? 1 : 0);
    return 1;
}

int BattleScript::LuaCountTeam(lua_State* state) {
    BattleScript* self = Self(state);
    const int team = static_cast<int>(luaL_checkinteger(state, 1));
    lua_pushinteger(state,
                    self != nullptr ? static_cast<lua_Integer>(self->units_.TeamCount(team)) : 0);
    return 1;
}

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
    host_.RegisterFunction("unit_tile", &LuaUnitTile, this);
    host_.RegisterFunction("unit_team", &LuaUnitTeam, this);
    host_.RegisterFunction("unit_alive", &LuaUnitAlive, this);
    host_.RegisterFunction("count_team", &LuaCountTeam, this);
    return host_.DoFile(luaPath, error);
}

bool BattleScript::Start(std::string& error) {
    return host_.Ok() && host_.CallGlobal("on_battle_start", error);
}

std::vector<int> BattleScript::InitiativeOrder() {
    std::vector<int> order;
    lua_State* L = host_.State();
    if (L != nullptr) {
        lua_getglobal(L, "initiative");
        if (lua_isfunction(L, -1) && lua_pcall(L, 0, 1, 0) == 0 && lua_istable(L, -1)) {
            const int n = static_cast<int>(lua_objlen(L, -1));
            for (int i = 1; i <= n; ++i) {
                lua_rawgeti(L, -1, i);
                order.push_back(static_cast<int>(lua_tointeger(L, -1)));
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }
    if (order.empty()) {
        units_.ForEach([&](UnitHandle h, const Unit&) { order.push_back(h.index); });
    }
    return order;
}

void BattleScript::OnTurnBegin(const int unitIndex) {
    lua_State* L = host_.State();
    if (L == nullptr) {
        return;
    }
    lua_getglobal(L, "on_turn_begin");
    if (lua_isfunction(L, -1)) {
        lua_pushinteger(L, unitIndex);
        lua_pcall(L, 1, 0, 0);
    } else {
        lua_pop(L, 1);
    }
}

std::string BattleScript::CheckVictory() {
    lua_State* L = host_.State();
    std::string result;
    if (L != nullptr) {
        lua_getglobal(L, "check_victory");
        if (lua_isfunction(L, -1) && lua_pcall(L, 0, 1, 0) == 0) {
            if (const char* s = lua_tostring(L, -1)) {
                result = s;
            }
        }
        lua_pop(L, 1);
    }
    if (result.empty()) {
        if (units_.TeamCount(1) == 0 && units_.TeamCount(0) > 0) {
            result = "player";
        } else if (units_.TeamCount(0) == 0 && units_.TeamCount(1) > 0) {
            result = "enemy";
        }
    }
    return result;
}

}  // namespace voxelgame::battle
