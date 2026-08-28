#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace voxelgame {

struct MeshData {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> uvs;
    std::vector<std::uint8_t> colors;
    std::vector<std::uint16_t> indices;
    std::size_t quadCount = 0;

    [[nodiscard]] std::size_t VertexCount() const noexcept { return vertices.size() / 3; }
    [[nodiscard]] std::size_t TriangleCount() const noexcept { return indices.size() / 3; }
    [[nodiscard]] bool Empty() const noexcept { return indices.empty(); }
};

}  // namespace voxelgame
