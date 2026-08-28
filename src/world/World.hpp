#pragma once

#include "world/Block.hpp"
#include "world/ChunkSection.hpp"

#include <cstddef>
#include <vector>

namespace voxelgame {

// A fixed grid of 16^3 chunk sections addressed by world block coordinates.
// Streaming and unbounded worlds come later; this is the static M3 world.
class World {
public:
    World(int sectionsX, int sectionsY, int sectionsZ);

    [[nodiscard]] int SectionsX() const noexcept { return sectionsX_; }
    [[nodiscard]] int SectionsY() const noexcept { return sectionsY_; }
    [[nodiscard]] int SectionsZ() const noexcept { return sectionsZ_; }
    [[nodiscard]] int SectionCount() const noexcept {
        return sectionsX_ * sectionsY_ * sectionsZ_;
    }

    [[nodiscard]] int BlocksX() const noexcept { return sectionsX_ * ChunkSection::Size; }
    [[nodiscard]] int BlocksY() const noexcept { return sectionsY_ * ChunkSection::Size; }
    [[nodiscard]] int BlocksZ() const noexcept { return sectionsZ_ * ChunkSection::Size; }

    // World-coordinate block access. Out-of-world reads return Air; out-of-world
    // writes are rejected.
    [[nodiscard]] BlockId GetBlock(int x, int y, int z) const noexcept;
    bool SetBlock(int x, int y, int z, BlockId block) noexcept;

    [[nodiscard]] const ChunkSection& SectionAt(int sx, int sy, int sz) const noexcept;
    [[nodiscard]] ChunkSection& SectionAt(int sx, int sy, int sz) noexcept;

    [[nodiscard]] bool SectionMeshDirty(int sx, int sy, int sz) const noexcept;
    void MarkSectionMeshClean(int sx, int sy, int sz) noexcept;

    [[nodiscard]] std::size_t NonAirBlockCount() const noexcept;

private:
    [[nodiscard]] bool InBounds(int x, int y, int z) const noexcept;
    [[nodiscard]] std::size_t SectionIndex(int sx, int sy, int sz) const noexcept;

    int sectionsX_;
    int sectionsY_;
    int sectionsZ_;
    std::vector<ChunkSection> sections_;
};

}  // namespace voxelgame
