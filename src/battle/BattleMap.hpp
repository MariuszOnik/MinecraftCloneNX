#pragma once

#include "battle/TileGrid.hpp"
#include "world/World.hpp"

#include <utility>
#include <vector>

namespace voxelgame::battle {

// A single bounded battle arena: a small voxel World, built once and never
// streamed, plus the derived tile grid. No raylib -- the app meshes and draws it.
class BattleMap {
public:
    // Builds the default hand-made arena (24x24 tiles).
    BattleMap();

    [[nodiscard]] const World& GetWorld() const noexcept { return world_; }
    [[nodiscard]] World& GetWorld() noexcept { return world_; }
    [[nodiscard]] const TileGrid& Grid() const noexcept { return grid_; }
    [[nodiscard]] TileGrid& Grid() noexcept { return grid_; }  // occupancy is battle state

    [[nodiscard]] int OriginX() const noexcept { return grid_.OriginX(); }
    [[nodiscard]] int OriginZ() const noexcept { return grid_.OriginZ(); }
    [[nodiscard]] int SizeX() const noexcept { return grid_.SizeX(); }
    [[nodiscard]] int SizeZ() const noexcept { return grid_.SizeZ(); }

    using SpawnList = std::vector<std::pair<int, int>>;  // tile (x, z)
    [[nodiscard]] const SpawnList& Spawns(int team) const noexcept {
        return team == 0 ? playerSpawns_ : enemySpawns_;
    }

private:
    void BuildArena();

    World world_;
    TileGrid grid_;
    SpawnList playerSpawns_;
    SpawnList enemySpawns_;
};

}  // namespace voxelgame::battle
