#include "world/ChunkSection.hpp"

#include <algorithm>

namespace voxelgame {

ChunkSection::ChunkSection() noexcept {
    blocks_.fill(blocks::Air);
}

BlockId ChunkSection::Get(const int x, const int y, const int z) const noexcept {
    if (!InBounds(x, y, z)) {
        return blocks::Air;
    }
    return blocks_[Index(x, y, z)];
}

bool ChunkSection::Set(const int x, const int y, const int z, const BlockId block) noexcept {
    if (!InBounds(x, y, z)) {
        return false;
    }

    BlockId& current = blocks_[Index(x, y, z)];
    if (current == block) {
        return false;
    }

    current = block;
    dataDirty_ = true;
    meshDirty_ = true;
    return true;
}

void ChunkSection::Fill(const BlockId block) noexcept {
    if (std::all_of(blocks_.begin(), blocks_.end(),
                    [block](const BlockId current) { return current == block; })) {
        return;
    }
    blocks_.fill(block);
    dataDirty_ = true;
    meshDirty_ = true;
}

std::size_t ChunkSection::NonAirBlockCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        blocks_.begin(), blocks_.end(),
        [](const BlockId block) { return block != blocks::Air; }));
}

}  // namespace voxelgame
