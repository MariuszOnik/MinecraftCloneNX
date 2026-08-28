#pragma once

#include "world/Block.hpp"

#include <array>
#include <cstddef>

namespace voxelgame {

class ChunkSection {
public:
    static constexpr int Size = 16;
    static constexpr std::size_t Volume = Size * Size * Size;

    ChunkSection() noexcept;

    [[nodiscard]] static constexpr bool InBounds(int x, int y, int z) noexcept {
        return x >= 0 && x < Size && y >= 0 && y < Size && z >= 0 && z < Size;
    }

    [[nodiscard]] static constexpr std::size_t Index(int x, int y, int z) noexcept {
        return static_cast<std::size_t>(x + Size * (z + Size * y));
    }

    [[nodiscard]] BlockId Get(int x, int y, int z) const noexcept;
    bool Set(int x, int y, int z, BlockId block) noexcept;
    void Fill(BlockId block) noexcept;

    [[nodiscard]] std::size_t NonAirBlockCount() const noexcept;
    [[nodiscard]] bool IsDataDirty() const noexcept { return dataDirty_; }
    [[nodiscard]] bool IsMeshDirty() const noexcept { return meshDirty_; }
    void MarkDataClean() noexcept { dataDirty_ = false; }
    void MarkMeshClean() noexcept { meshDirty_ = false; }
    // Forces a remesh even though this section's own blocks did not change --
    // used when a neighbouring section edits a block on the shared boundary.
    void MarkMeshDirty() noexcept { meshDirty_ = true; }

private:
    std::array<BlockId, Volume> blocks_{};
    bool dataDirty_ = false;
    bool meshDirty_ = true;
};

}  // namespace voxelgame
