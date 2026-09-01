#include "battle/TileGrid.hpp"

#include "world/Block.hpp"
#include "world/World.hpp"

#include <algorithm>

namespace voxelgame::battle {
namespace {

Terrain TerrainOf(const BlockId block) noexcept {
    switch (block) {
        case blocks::Grass:
            return Terrain::Grass;
        case blocks::Dirt:
            return Terrain::Dirt;
        case blocks::Sand:
        case blocks::Gravel:
            return Terrain::Sand;
        case blocks::Planks:
        case blocks::Wood:
            return Terrain::Wood;
        case blocks::Water:
            return Terrain::Water;
        default:
            return Terrain::Stone;
    }
}

}  // namespace

TileGrid::TileGrid(const int originX, const int originZ, const int sizeX, const int sizeZ)
    : originX_(originX),
      originZ_(originZ),
      sizeX_(std::max(sizeX, 0)),
      sizeZ_(std::max(sizeZ, 0)),
      tiles_(static_cast<std::size_t>(sizeX_) * static_cast<std::size_t>(sizeZ_)) {}

std::size_t TileGrid::Index(const int tileX, const int tileZ) const noexcept {
    return static_cast<std::size_t>(tileX - originX_) +
           static_cast<std::size_t>(sizeX_) * static_cast<std::size_t>(tileZ - originZ_);
}

bool TileGrid::InBounds(const int tileX, const int tileZ) const noexcept {
    return tileX >= originX_ && tileZ >= originZ_ && tileX < originX_ + sizeX_ &&
           tileZ < originZ_ + sizeZ_;
}

const Tile& TileGrid::At(const int tileX, const int tileZ) const noexcept {
    return InBounds(tileX, tileZ) ? tiles_[Index(tileX, tileZ)] : outside_;
}

Tile& TileGrid::At(const int tileX, const int tileZ) noexcept {
    return InBounds(tileX, tileZ) ? tiles_[Index(tileX, tileZ)] : outside_;
}

std::size_t TileGrid::WalkableCount() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(tiles_.begin(), tiles_.end(), [](const Tile& t) { return t.walkable; }));
}

void TileGrid::Rebuild(const World& world) {
    for (int tz = originZ_; tz < originZ_ + sizeZ_; ++tz) {
        for (int tx = originX_; tx < originX_ + sizeX_; ++tx) {
            Tile& tile = tiles_[Index(tx, tz)];
            tile = Tile{};

            // First cube-shaped, non-air block from the top of the column.
            int surfaceY = -1;
            for (int y = world.BlocksY() - 1; y >= 0; --y) {
                const BlockId block = world.GetBlock(tx, y, tz);
                if (block != blocks::Air && IsCubeShaped(block)) {
                    surfaceY = y;
                    break;
                }
            }
            if (surfaceY < 0) {
                continue;  // empty column: no floor
            }

            const BlockId surface = world.GetBlock(tx, surfaceY, tz);
            tile.terrain = TerrainOf(surface);

            if (surface == blocks::Water || !IsCollidableBlock(surface)) {
                // Standing surface is liquid (or otherwise non-solid): a floor is
                // "there" but not something a unit can stand on this slice.
                tile.hasFloor = surface == blocks::Water;
                tile.height = tile.hasFloor ? surfaceY : 0;
                tile.walkable = false;
                continue;
            }

            tile.hasFloor = true;
            tile.height = surfaceY + 1;

            const bool headroom = !IsCollidableBlock(world.GetBlock(tx, tile.height, tz)) &&
                                  !IsCollidableBlock(world.GetBlock(tx, tile.height + 1, tz));
            tile.walkable = headroom;
        }
    }
}

}  // namespace voxelgame::battle
