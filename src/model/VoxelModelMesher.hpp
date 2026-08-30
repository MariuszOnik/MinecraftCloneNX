#pragma once

#include "model/VoxelModel.hpp"

#include <cstdint>
#include <vector>

namespace voxelgame::vmodel {

// CPU geometry for one part: vertex-coloured, no texture. Positions are in the
// part's local voxel space (0..size); the renderer scales and places it.
struct ModelMeshData {
    std::vector<float> vertices;   // 3 per vertex
    std::vector<float> normals;    // 3 per vertex
    std::vector<std::uint8_t> colors;  // 4 per vertex (material colour * face shade)
    std::vector<std::uint16_t> indices;
    std::size_t quadCount = 0;

    [[nodiscard]] std::size_t VertexCount() const noexcept { return vertices.size() / 3; }
    [[nodiscard]] std::size_t TriangleCount() const noexcept { return indices.size() / 3; }
    [[nodiscard]] bool Empty() const noexcept { return indices.empty(); }
};

// Face-culled mesh of one part: a quad wherever a filled voxel meets empty space
// (grid edges count as empty). No greedy merge yet -- parts are small.
[[nodiscard]] ModelMeshData BuildPartMesh(const VoxelModelPart& part, const VoxelModel& model);

}  // namespace voxelgame::vmodel
