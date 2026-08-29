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

    // Meshes the section (chunkX, sectionY, chunkZ) of a world, culling against
    // the real neighbouring sections/columns across shared boundaries.
    [[nodiscard]] MeshData Build(const World& world, int chunkX, int sectionY, int chunkZ,
                                 const BlockAtlasBinding& binding) const;
};

}  // namespace voxelgame
