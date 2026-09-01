#include "script/LuaHost.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace voxelgame::script {
namespace {

// Opens only the parts of the standard library a sandboxed game script needs.
void OpenSafeLibraries(lua_State* L) {
    struct Lib {
        const char* name;
        lua_CFunction open;
    };
    const Lib libs[] = {
        {"", luaopen_base},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
    };
    for (const Lib& lib : libs) {
        lua_pushcfunction(L, lib.open);
        lua_pushstring(L, lib.name);
        lua_call(L, 1, 0);
    }
}

// Runs the chunk currently on top of the stack (as produced by luaL_load*).
bool RunLoaded(lua_State* L, int loadStatus, std::string& error) {
    if (loadStatus != 0) {
        error = lua_tostring(L, -1) ? lua_tostring(L, -1) : "load failed";
        lua_pop(L, 1);
        return false;
    }
    if (lua_pcall(L, 0, 0, 0) != 0) {
        error = lua_tostring(L, -1) ? lua_tostring(L, -1) : "runtime error";
        lua_pop(L, 1);
        return false;
    }
    return true;
}

}  // namespace

LuaHost::LuaHost() {
    state_ = luaL_newstate();
    if (state_ != nullptr) {
        OpenSafeLibraries(state_);
    }
}

LuaHost::~LuaHost() {
    if (state_ != nullptr) {
        lua_close(state_);
    }
}

bool LuaHost::DoFile(const std::string& path, std::string& error) {
    if (state_ == nullptr) {
        error = "no Lua state";
        return false;
    }
    return RunLoaded(state_, luaL_loadfile(state_, path.c_str()), error);
}

bool LuaHost::DoString(const std::string& chunk, const std::string& chunkName, std::string& error) {
    if (state_ == nullptr) {
        error = "no Lua state";
        return false;
    }
    const int status = luaL_loadbuffer(state_, chunk.data(), chunk.size(), chunkName.c_str());
    return RunLoaded(state_, status, error);
}

bool LuaHost::CallGlobal(const std::string& name, std::string& error) {
    if (state_ == nullptr) {
        error = "no Lua state";
        return false;
    }
    lua_getglobal(state_, name.c_str());
    if (!lua_isfunction(state_, -1)) {
        lua_pop(state_, 1);
        return true;  // nothing to call
    }
    if (lua_pcall(state_, 0, 0, 0) != 0) {
        error = lua_tostring(state_, -1) ? lua_tostring(state_, -1) : "runtime error";
        lua_pop(state_, 1);
        return false;
    }
    return true;
}

void LuaHost::RegisterFunction(const char* name, int (*fn)(lua_State*), void* context) {
    if (state_ == nullptr) {
        return;
    }
    if (context != nullptr) {
        lua_pushlightuserdata(state_, context);
        lua_pushcclosure(state_, fn, 1);
    } else {
        lua_pushcfunction(state_, fn);
    }
    lua_setglobal(state_, name);
}

}  // namespace voxelgame::script
