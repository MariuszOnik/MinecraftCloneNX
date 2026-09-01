#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace voxelgame::battle {

// Grid facing. North is -Z, matching TileCenterWorld / camera conventions.
enum class Facing : std::uint8_t { North, East, South, West };

[[nodiscard]] float FacingYaw(Facing facing) noexcept;   // radians, for the model transform
[[nodiscard]] Facing FacingTowards(int fromX, int fromZ, int toX, int toZ) noexcept;

// A generational handle: stays invalid after the unit it named is removed.
struct UnitHandle {
    int index = -1;
    std::uint32_t generation = 0;

    [[nodiscard]] bool Valid() const noexcept { return index >= 0; }
    friend bool operator==(const UnitHandle& a, const UnitHandle& b) noexcept {
        return a.index == b.index && a.generation == b.generation;
    }
};

// A thin container. The real stat block (STR/DEX/... , class, AC, HP dice) lives
// in a Lua table keyed by the handle. C++ keeps only what rendering and
// pathfinding need: identity, position, and a few Lua-written mirrors.
struct Unit {
    int team = 0;   // 0 = player, 1 = enemy
    int tileX = 0;
    int tileZ = 0;
    Facing facing = Facing::South;
    std::string model = "humanoid";  // asset base name; the renderer caches it

    int hp = 1;          // Lua-written mirror, for the HP bar and the alive check
    int hpMax = 1;       // Lua-written mirror
    int moveTiles = 4;   // Lua writes this from the stat block before a move
    int jumpHeight = 1;  // Lua-set; C++ pathfinding's max step between tiles
};

// A small pool of units with stable generational handles. Not an ECS -- just
// enough to spawn, look up, iterate, and remove during a battle.
class UnitRegistry {
public:
    UnitHandle Spawn(const Unit& prototype);
    void Remove(UnitHandle handle);

    [[nodiscard]] bool Alive(UnitHandle handle) const noexcept;
    [[nodiscard]] Unit* Get(UnitHandle handle) noexcept;
    [[nodiscard]] const Unit* Get(UnitHandle handle) const noexcept;

    [[nodiscard]] std::size_t AliveCount() const noexcept { return aliveCount_; }
    [[nodiscard]] std::size_t TeamCount(int team) const noexcept;

    void ForEach(const std::function<void(UnitHandle, Unit&)>& fn);
    void ForEach(const std::function<void(UnitHandle, const Unit&)>& fn) const;

private:
    struct Slot {
        Unit unit;
        std::uint32_t generation = 1;
        bool alive = false;
    };

    [[nodiscard]] bool IndexValid(UnitHandle handle) const noexcept;

    std::vector<Slot> slots_;
    std::vector<int> freeList_;
    std::size_t aliveCount_ = 0;
};

}  // namespace voxelgame::battle
