#pragma once

#include "world/ChunkSection.hpp"
#include "world/MeshData.hpp"

namespace voxelgame {

class ChunkMesher {
public:
    [[nodiscard]] MeshData Build(const ChunkSection& section) const;
};

}  // namespace voxelgame
