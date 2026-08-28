#pragma once

#include "world/Block.hpp"
#include "world/BlockAtlasLayout.hpp"

#include <array>
#include <cstdint>

namespace voxelgame {

struct AtlasDescriptor;

// Maps (block, cube face) to a tile in the active atlas. Starts from the block
// registry's compiled defaults; Apply() overlays an atlas descriptor so a
// different atlas (grid size and per-face tiles) can be swapped in at runtime
// without touching the registry.
class BlockAtlasBinding {
public:
    BlockAtlasBinding() noexcept;

    void Apply(const AtlasDescriptor& descriptor);

    [[nodiscard]] std::uint8_t FaceTile(BlockId block, int faceIndex) const noexcept;
    [[nodiscard]] atlas::TileRect FaceRect(BlockId block, int faceIndex) const noexcept;

    [[nodiscard]] int Columns() const noexcept { return columns_; }
    [[nodiscard]] int Rows() const noexcept { return rows_; }

    // Normalised size of one tile's content in the atlas, for the tiling shader's
    // fract() -> atlas mapping.
    [[nodiscard]] float TileExtentU() const noexcept {
        return static_cast<float>(tileSize_) / static_cast<float>(width_);
    }
    [[nodiscard]] float TileExtentV() const noexcept {
        return static_cast<float>(tileSize_) / static_cast<float>(height_);
    }

private:
    std::array<BlockFaceTiles, blocks::Count> table_{};
    int columns_ = atlas::Columns;
    int rows_ = atlas::Rows;
    int width_ = atlas::Width;
    int height_ = atlas::Height;
    int tileSize_ = atlas::TileSize;
    int padding_ = atlas::Padding;
};

}  // namespace voxelgame
