#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace voxelgame {

struct MeshData {
    std::vector<float> vertices;
    std::vector<float> normals;
    // Tile-space UVs. A greedy-merged quad spanning w x h tiles runs 0..w / 0..h
    // here; the tiling shader takes fract() to repeat within its tile.
    std::vector<float> uvs;
    // Per-vertex atlas origin (u0, v0) of the tile this face samples, so one draw
    // can mix tiles. All four vertices of a quad share it.
    std::vector<float> tileOrigins;
    std::vector<std::uint8_t> colors;
    std::vector<std::uint16_t> indices;
    std::size_t quadCount = 0;

    [[nodiscard]] std::size_t VertexCount() const noexcept { return vertices.size() / 3; }
    [[nodiscard]] std::size_t TriangleCount() const noexcept { return indices.size() / 3; }
    [[nodiscard]] bool Empty() const noexcept { return indices.empty(); }
};

}  // namespace voxelgame
