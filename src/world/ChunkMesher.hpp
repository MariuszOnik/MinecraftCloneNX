#pragma once

#include "world/Block.hpp"
#include "world/BlockAtlasBinding.hpp"
#include "world/ChunkSection.hpp"
#include "world/MeshData.hpp"

#include <array>
#include <cstddef>

namespace voxelgame {

class World;

// One section's geometry split by render layer (opaque / cutout / transparent),
// indexed by RenderLayer.
struct SectionMesh {
    std::array<MeshData, 3> layers;

    [[nodiscard]] MeshData& Layer(RenderLayer layer) noexcept {
        return layers[static_cast<std::size_t>(layer)];
    }
    [[nodiscard]] const MeshData& Layer(RenderLayer layer) const noexcept {
        return layers[static_cast<std::size_t>(layer)];
    }
    [[nodiscard]] bool Empty() const noexcept {
        return layers[0].Empty() && layers[1].Empty() && layers[2].Empty();
    }
    [[nodiscard]] std::size_t QuadCount() const noexcept {
        return layers[0].quadCount + layers[1].quadCount + layers[2].quadCount;
    }
};

class ChunkMesher {
public:
    // Meshes a section in isolation (every neighbour outside it reads as air).
    [[nodiscard]] SectionMesh Build(const ChunkSection& section,
                                    const BlockAtlasBinding& binding) const;

    // Meshes the section (chunkX, sectionY, chunkZ) of a world, culling against
    // the real neighbouring sections/columns across shared boundaries.
    [[nodiscard]] SectionMesh Build(const World& world, int chunkX, int sectionY, int chunkZ,
                                    const BlockAtlasBinding& binding) const;
};

}  // namespace voxelgame
