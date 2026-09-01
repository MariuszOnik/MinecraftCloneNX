#pragma once

#include <vector>

namespace voxelgame::battle {

class TileGrid;

struct PathStep {
    int x = 0;
    int z = 0;
};

// Tiles a unit can move to from a start tile: 4-connected, only onto walkable and
// unoccupied tiles, and only where the height difference between two tiles is
// within `jumpHeight`. Uniform step cost for now (terrain cost comes later).
class ReachableSet {
public:
    ReachableSet(int originX, int originZ, int sizeX, int sizeZ);

    void Set(int x, int z, int cost);
    [[nodiscard]] bool Contains(int x, int z) const noexcept;   // has a cost, incl. the start
    [[nodiscard]] int Cost(int x, int z) const noexcept;        // -1 if not reachable
    [[nodiscard]] const std::vector<PathStep>& Tiles() const noexcept { return tiles_; }

private:
    [[nodiscard]] bool InBounds(int x, int z) const noexcept;

    int originX_;
    int originZ_;
    int sizeX_;
    int sizeZ_;
    std::vector<int> cost_;        // -1 = unreachable
    std::vector<PathStep> tiles_;  // reachable tiles other than the start
};

// True if a unit may step directly between two adjacent tiles.
[[nodiscard]] bool CanStep(const TileGrid& grid, int fromX, int fromZ, int toX, int toZ,
                           int jumpHeight);

[[nodiscard]] ReachableSet ComputeReachable(const TileGrid& grid, int startX, int startZ,
                                            int moveBudget, int jumpHeight);

// Shortest tile path from start to goal under the same rules, both ends
// included. Empty if the goal is not reachable within no budget limit.
[[nodiscard]] std::vector<PathStep> ComputePath(const TileGrid& grid, int startX, int startZ,
                                                int goalX, int goalZ, int jumpHeight);

}  // namespace voxelgame::battle
