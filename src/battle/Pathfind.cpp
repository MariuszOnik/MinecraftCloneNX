#include "battle/Pathfind.hpp"

#include "battle/TileGrid.hpp"

#include <algorithm>
#include <cstdlib>
#include <deque>

namespace voxelgame::battle {
namespace {

constexpr int kDx[4] = {1, -1, 0, 0};
constexpr int kDz[4] = {0, 0, 1, -1};

}  // namespace

ReachableSet::ReachableSet(const int originX, const int originZ, const int sizeX, const int sizeZ)
    : originX_(originX),
      originZ_(originZ),
      sizeX_(std::max(sizeX, 0)),
      sizeZ_(std::max(sizeZ, 0)),
      cost_(static_cast<std::size_t>(sizeX_) * static_cast<std::size_t>(sizeZ_), -1) {}

bool ReachableSet::InBounds(const int x, const int z) const noexcept {
    return x >= originX_ && z >= originZ_ && x < originX_ + sizeX_ && z < originZ_ + sizeZ_;
}

void ReachableSet::Set(const int x, const int z, const int cost) {
    if (!InBounds(x, z)) {
        return;
    }
    const std::size_t i = static_cast<std::size_t>(x - originX_) +
                          static_cast<std::size_t>(sizeX_) * static_cast<std::size_t>(z - originZ_);
    if (cost_[i] < 0) {
        if (cost > 0) {
            tiles_.push_back({x, z});
        }
        cost_[i] = cost;
    }
}

bool ReachableSet::Contains(const int x, const int z) const noexcept {
    return Cost(x, z) >= 0;
}

int ReachableSet::Cost(const int x, const int z) const noexcept {
    if (!InBounds(x, z)) {
        return -1;
    }
    return cost_[static_cast<std::size_t>(x - originX_) +
                 static_cast<std::size_t>(sizeX_) * static_cast<std::size_t>(z - originZ_)];
}

bool CanStep(const TileGrid& grid, const int fromX, const int fromZ, const int toX, const int toZ,
             const int jumpHeight) {
    const Tile& to = grid.At(toX, toZ);
    if (!to.walkable || to.occupant >= 0) {
        return false;
    }
    const Tile& from = grid.At(fromX, fromZ);
    return std::abs(from.height - to.height) <= jumpHeight;
}

ReachableSet ComputeReachable(const TileGrid& grid, const int startX, const int startZ,
                              const int moveBudget, const int jumpHeight) {
    ReachableSet out(grid.OriginX(), grid.OriginZ(), grid.SizeX(), grid.SizeZ());
    if (!grid.At(startX, startZ).hasFloor) {
        return out;
    }

    // BFS: uniform step cost, so the first visit to a tile is the cheapest.
    out.Set(startX, startZ, 0);
    std::deque<PathStep> frontier{{startX, startZ}};

    while (!frontier.empty()) {
        const PathStep cur = frontier.front();
        frontier.pop_front();
        const int cost = out.Cost(cur.x, cur.z);
        if (cost >= moveBudget) {
            continue;
        }
        for (int d = 0; d < 4; ++d) {
            const int nx = cur.x + kDx[d];
            const int nz = cur.z + kDz[d];
            if (out.Cost(nx, nz) >= 0) {
                continue;  // already reached (cheaper or equal)
            }
            if (CanStep(grid, cur.x, cur.z, nx, nz, jumpHeight)) {
                out.Set(nx, nz, cost + 1);
                frontier.push_back({nx, nz});
            }
        }
    }
    return out;
}

std::vector<PathStep> ComputePath(const TileGrid& grid, const int startX, const int startZ,
                                  const int goalX, const int goalZ, const int jumpHeight) {
    if (startX == goalX && startZ == goalZ) {
        return {{startX, startZ}};
    }
    if (!grid.At(goalX, goalZ).walkable || grid.At(goalX, goalZ).occupant >= 0) {
        return {};
    }

    const int ox = grid.OriginX();
    const int oz = grid.OriginZ();
    const int sx = grid.SizeX();
    const int sz = grid.SizeZ();
    const auto index = [&](const int x, const int z) {
        return static_cast<std::size_t>(x - ox) + static_cast<std::size_t>(sx) *
                                                      static_cast<std::size_t>(z - oz);
    };
    const auto inBounds = [&](const int x, const int z) {
        return x >= ox && z >= oz && x < ox + sx && z < oz + sz;
    };

    std::vector<int> from(static_cast<std::size_t>(sx) * static_cast<std::size_t>(sz), -2);
    from[index(startX, startZ)] = -1;  // start has no predecessor
    std::deque<PathStep> frontier{{startX, startZ}};

    bool found = false;
    while (!frontier.empty() && !found) {
        const PathStep cur = frontier.front();
        frontier.pop_front();
        for (int d = 0; d < 4; ++d) {
            const int nx = cur.x + kDx[d];
            const int nz = cur.z + kDz[d];
            if (!inBounds(nx, nz) || from[index(nx, nz)] != -2) {
                continue;
            }
            if (!CanStep(grid, cur.x, cur.z, nx, nz, jumpHeight)) {
                continue;
            }
            from[index(nx, nz)] = d;
            if (nx == goalX && nz == goalZ) {
                found = true;
                break;
            }
            frontier.push_back({nx, nz});
        }
    }
    if (!found) {
        return {};
    }

    std::vector<PathStep> path;
    int x = goalX;
    int z = goalZ;
    while (!(x == startX && z == startZ)) {
        path.push_back({x, z});
        const int d = from[index(x, z)];
        x -= kDx[d];
        z -= kDz[d];
    }
    path.push_back({startX, startZ});
    std::reverse(path.begin(), path.end());
    return path;
}

}  // namespace voxelgame::battle
