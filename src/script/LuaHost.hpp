#pragma once

#include <string>

struct lua_State;

namespace voxelgame::script {

// A Lua 5.1 interpreter with a safe subset of the standard library (base, table,
// string, math -- no io/os/package). Thin on purpose: it loads chunks, calls
// global functions, and registers C functions. Game-specific bindings live with
// the systems that own them.
class LuaHost {
public:
    LuaHost();
    ~LuaHost();

    LuaHost(const LuaHost&) = delete;
    LuaHost& operator=(const LuaHost&) = delete;

    [[nodiscard]] lua_State* State() const noexcept { return state_; }
    [[nodiscard]] bool Ok() const noexcept { return state_ != nullptr; }

    // Loads and runs a chunk. false + `error` on a syntax or runtime error.
    bool DoFile(const std::string& path, std::string& error);
    bool DoString(const std::string& chunk, const std::string& chunkName, std::string& error);

    // Calls global `name()` with no arguments. A missing global is not an error
    // (returns true). A runtime error returns false + fills `error`.
    bool CallGlobal(const std::string& name, std::string& error);

    // Registers `fn` as a global C function. `context`, if given, is available to
    // `fn` as an upvalue (light userdata at lua_upvalueindex(1)).
    void RegisterFunction(const char* name, int (*fn)(lua_State*), void* context = nullptr);

private:
    lua_State* state_ = nullptr;
};

}  // namespace voxelgame::script
