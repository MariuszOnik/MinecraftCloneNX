#include "world/World.hpp"

#include <algorithm>

namespace voxelgame {

namespace {
constexpr int N = ChunkSection::Size;
}

World::World(const int sectionsX, const int sectionsY, const int sectionsZ)
    : sectionsX_(std::max(sectionsX, 1)),
      sectionsY_(std::max(sectionsY, 1)),
      sectionsZ_(std::max(sectionsZ, 1)),
      sections_(static_cast<std::size_t>(sectionsX_) * sectionsY_ * sectionsZ_) {}

bool World::InBounds(const int x, const int y, const int z) const noexcept {
    return x >= 0 && y >= 0 && z >= 0 && x < BlocksX() && y < BlocksY() && z < BlocksZ();
}

std::size_t World::SectionIndex(const int sx, const int sy, const int sz) const noexcept {
    return (static_cast<std::size_t>(sy) * sectionsZ_ + sz) * sectionsX_ + sx;
}

const ChunkSection& World::SectionAt(const int sx, const int sy, const int sz) const noexcept {
    return sections_[SectionIndex(sx, sy, sz)];
}

ChunkSection& World::SectionAt(const int sx, const int sy, const int sz) noexcept {
    return sections_[SectionIndex(sx, sy, sz)];
}

BlockId World::GetBlock(const int x, const int y, const int z) const noexcept {
    if (!InBounds(x, y, z)) {
        return blocks::Air;
    }
    return SectionAt(x / N, y / N, z / N).Get(x % N, y % N, z % N);
}

bool World::SetBlock(const int x, const int y, const int z, const BlockId block) noexcept {
    if (!InBounds(x, y, z)) {
        return false;
    }
    const int sx = x / N;
    const int sy = y / N;
    const int sz = z / N;
    const int lx = x % N;
    const int ly = y % N;
    const int lz = z % N;

    if (!SectionAt(sx, sy, sz).Set(lx, ly, lz, block)) {
        return false;
    }

    // A block on a shared face changes how the neighbouring section meshes too.
    if (lx == 0 && sx > 0) SectionAt(sx - 1, sy, sz).MarkMeshDirty();
    if (lx == N - 1 && sx < sectionsX_ - 1) SectionAt(sx + 1, sy, sz).MarkMeshDirty();
    if (ly == 0 && sy > 0) SectionAt(sx, sy - 1, sz).MarkMeshDirty();
    if (ly == N - 1 && sy < sectionsY_ - 1) SectionAt(sx, sy + 1, sz).MarkMeshDirty();
    if (lz == 0 && sz > 0) SectionAt(sx, sy, sz - 1).MarkMeshDirty();
    if (lz == N - 1 && sz < sectionsZ_ - 1) SectionAt(sx, sy, sz + 1).MarkMeshDirty();
    return true;
}

bool World::SectionMeshDirty(const int sx, const int sy, const int sz) const noexcept {
    return SectionAt(sx, sy, sz).IsMeshDirty();
}

void World::MarkSectionMeshClean(const int sx, const int sy, const int sz) noexcept {
    SectionAt(sx, sy, sz).MarkMeshClean();
}

std::size_t World::NonAirBlockCount() const noexcept {
    std::size_t total = 0;
    for (const ChunkSection& section : sections_) {
        total += section.NonAirBlockCount();
    }
    return total;
}

}  // namespace voxelgame
