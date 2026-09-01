#include "battle/BattleMap.hpp"

#include "world/Block.hpp"
#include "world/ChunkSection.hpp"

#include <utility>

namespace voxelgame::battle {
namespace {

constexpr int kOriginX = 0;
constexpr int kOriginZ = 0;
constexpr int kSize = 24;
constexpr int kGroundTop = 3;  // grass Y; a unit's feet rest at kGroundTop + 1

// Fills a fresh column with the flat arena floor: stone up to the grass cap.
void FillFloor(World& world, const int chunkX, const int chunkZ) {
    const int baseX = chunkX * ChunkSection::Size;
    const int baseZ = chunkZ * ChunkSection::Size;
    for (int lz = 0; lz < ChunkSection::Size; ++lz) {
        for (int lx = 0; lx < ChunkSection::Size; ++lx) {
            const int x = baseX + lx;
            const int z = baseZ + lz;
            if (x < kOriginX || z < kOriginZ || x >= kOriginX + kSize || z >= kOriginZ + kSize) {
                continue;
            }
            for (int y = 0; y < kGroundTop; ++y) {
                world.SetBlock(x, y, z, blocks::Stone);
            }
            world.SetBlock(x, kGroundTop, z, blocks::Grass);
        }
    }
}

}  // namespace

BattleMap::BattleMap()
    : world_(1, [](World& w, int cx, int cz) { FillFloor(w, cx, cz); }),
      grid_(kOriginX, kOriginZ, kSize, kSize) {
    BuildArena();
    grid_.Rebuild(world_);
}

void BattleMap::BuildArena() {
    // Load every column the footprint touches (the filler lays the floor).
    for (int cz = 0; cz <= (kOriginZ + kSize - 1) / ChunkSection::Size; ++cz) {
        for (int cx = 0; cx <= (kOriginX + kSize - 1) / ChunkSection::Size; ++cx) {
            world_.EnsureColumn(cx, cz);
        }
    }

    // Arena features are map geometry, not player edits.
    world_.SetJournalling(false);

    const auto column = [&](int x, int z, int fromY, int toY, BlockId block) {
        for (int y = fromY; y <= toY; ++y) {
            world_.SetBlock(x, y, z, block);
        }
    };

    // A raised grass platform in the middle, reached by one step on its west side.
    for (int z = 8; z < 15; ++z) {
        for (int x = 8; x < 15; ++x) {
            world_.SetBlock(x, kGroundTop + 1, z, blocks::Stone);
            world_.SetBlock(x, kGroundTop + 2, z, blocks::Grass);
        }
    }
    for (int z = 10; z < 13; ++z) {
        world_.SetBlock(7, kGroundTop + 1, z, blocks::Stone);  // step: feet at +2
    }

    // A shallow water trench along the south edge (stone bed stays beneath).
    for (int x = kOriginX; x < kOriginX + kSize; ++x) {
        for (int z = 1; z < 4; ++z) {
            world_.SetBlock(x, kGroundTop, z, blocks::Water);
        }
    }

    // A few two-tall stone pillars for cover / blocked tiles.
    for (const auto& p : {std::pair{4, 18}, std::pair{19, 6}, std::pair{16, 17}, std::pair{6, 6}}) {
        column(p.first, p.second, kGroundTop + 1, kGroundTop + 2, blocks::Stone);
    }

    world_.SetJournalling(true);
}

}  // namespace voxelgame::battle
