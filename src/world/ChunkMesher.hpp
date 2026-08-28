#pragma once

#include "world/BlockAtlasBinding.hpp"
#include "world/ChunkSection.hpp"
#include "world/MeshData.hpp"

namespace voxelgame {

class ChunkMesher {
public:
    [[nodiscard]] MeshData Build(const ChunkSection& section,
                                 const BlockAtlasBinding& atlas) const;
};

}  // namespace voxelgame
