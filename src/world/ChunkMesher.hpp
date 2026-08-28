#pragma once

#include "world/BlockAtlasBinding.hpp"
#include "world/ChunkSection.hpp"
#include "world/MeshData.hpp"

namespace voxelgame {

class World;

class ChunkMesher {
public:
    // Meshes a section in isolation (every neighbour outside it reads as air).
    [[nodiscard]] MeshData Build(const ChunkSection& section,
                                 const BlockAtlasBinding& binding) const;

    // Meshes one section of a world, culling against the real neighbouring
    // sections across shared boundaries.
    [[nodiscard]] MeshData Build(const World& world, int sectionX, int sectionY, int sectionZ,
                                 const BlockAtlasBinding& binding) const;
};

}  // namespace voxelgame
