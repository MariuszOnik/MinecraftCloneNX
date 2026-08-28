#include "world/BlockAtlasBinding.hpp"

#include "world/AtlasDescriptor.hpp"

namespace voxelgame {

BlockAtlasBinding::BlockAtlasBinding() noexcept {
    for (BlockId id = 0; id < blocks::Count; ++id) {
        table_[id] = GetBlockDefinition(id).faceTiles;
    }
}

void BlockAtlasBinding::Apply(const AtlasDescriptor& descriptor) {
    columns_ = descriptor.columns;
    rows_ = descriptor.rows;
    width_ = descriptor.atlasWidth;
    height_ = descriptor.atlasHeight;
    tileSize_ = descriptor.tileSize;
    padding_ = descriptor.padding;

    for (BlockId id = 0; id < blocks::Count; ++id) {
        const auto match = descriptor.blockFaceTiles.find(std::string(GetBlockDefinition(id).name));
        if (match != descriptor.blockFaceTiles.end()) {
            table_[id] = match->second;
        }
    }
}

std::uint8_t BlockAtlasBinding::FaceTile(const BlockId block, const int faceIndex) const noexcept {
    if (block >= blocks::Count || faceIndex < 0 || faceIndex >= 6) {
        return 0;
    }
    return table_[block][static_cast<std::size_t>(faceIndex)];
}

atlas::TileRect BlockAtlasBinding::FaceRect(const BlockId block,
                                           const int faceIndex) const noexcept {
    return atlas::TileRectOf(FaceTile(block, faceIndex), columns_, rows_, width_, height_,
                             tileSize_, padding_);
}

}  // namespace voxelgame
