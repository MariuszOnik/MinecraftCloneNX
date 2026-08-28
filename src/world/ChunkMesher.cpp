#include "world/ChunkMesher.hpp"

#include "world/Block.hpp"
#include "world/BlockAtlasLayout.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace voxelgame {
namespace {

struct FaceDefinition {
    int neighborX;
    int neighborY;
    int neighborZ;
    std::array<std::array<float, 3>, 4> corners;
    std::array<float, 3> normal;
    float shade;
};

constexpr std::array<FaceDefinition, 6> faces{{
    {1, 0, 0, {{{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}}}, {1, 0, 0}, 0.82F},
    {-1, 0, 0, {{{0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}}}, {-1, 0, 0}, 0.72F},
    {0, 1, 0, {{{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}}}, {0, 1, 0}, 1.00F},
    {0, -1, 0, {{{0, 0, 1}, {0, 0, 0}, {1, 0, 0}, {1, 0, 1}}}, {0, -1, 0}, 0.48F},
    {0, 0, 1, {{{1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {0, 0, 1}}}, {0, 0, 1}, 0.90F},
    {0, 0, -1, {{{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}}, {0, 0, -1}, 0.64F},
}};

std::uint8_t ShadeChannel(const float shade) noexcept {
    return static_cast<std::uint8_t>(255.0F * shade);
}

// Local (u, v) of a face corner within its tile, in [0, 1]. Derived from the
// corner's block-space position so every face reads upright: block-space up
// (y = 1) maps to the top of the tile (v = 0), and side faces keep a stable
// horizontal axis.
std::array<float, 2> CornerTileUv(const FaceDefinition& face,
                                  const std::array<float, 3>& corner) noexcept {
    if (face.normal[1] != 0.0F) {  // top / bottom face
        return {corner[0], corner[2]};
    }
    if (face.normal[0] != 0.0F) {  // +X / -X face
        return {corner[2], 1.0F - corner[1]};
    }
    return {corner[0], 1.0F - corner[1]};  // +Z / -Z face
}

void AppendFace(MeshData& mesh, const FaceDefinition& face, const int x, const int y,
                const int z, const atlas::TileRect uv) {
    const auto firstVertex = static_cast<std::uint16_t>(mesh.VertexCount());

    const std::uint8_t shaded = ShadeChannel(face.shade);

    for (const auto& corner : face.corners) {
        mesh.vertices.push_back(static_cast<float>(x) + corner[0]);
        mesh.vertices.push_back(static_cast<float>(y) + corner[1]);
        mesh.vertices.push_back(static_cast<float>(z) + corner[2]);

        mesh.normals.insert(mesh.normals.end(), face.normal.begin(), face.normal.end());

        const std::array<float, 2> local = CornerTileUv(face, corner);
        mesh.uvs.push_back(uv.u0 + (uv.u1 - uv.u0) * local[0]);
        mesh.uvs.push_back(uv.v0 + (uv.v1 - uv.v0) * local[1]);

        mesh.colors.push_back(shaded);
        mesh.colors.push_back(shaded);
        mesh.colors.push_back(shaded);
        mesh.colors.push_back(255);
    }

    mesh.indices.push_back(firstVertex);
    mesh.indices.push_back(static_cast<std::uint16_t>(firstVertex + 1));
    mesh.indices.push_back(static_cast<std::uint16_t>(firstVertex + 2));
    mesh.indices.push_back(firstVertex);
    mesh.indices.push_back(static_cast<std::uint16_t>(firstVertex + 2));
    mesh.indices.push_back(static_cast<std::uint16_t>(firstVertex + 3));
    ++mesh.quadCount;
}

}  // namespace

MeshData ChunkMesher::Build(const ChunkSection& section, const BlockAtlasBinding& binding) const {
    MeshData mesh;
    mesh.vertices.reserve(ChunkSection::Volume * 3);
    mesh.normals.reserve(ChunkSection::Volume * 3);
    mesh.uvs.reserve(ChunkSection::Volume * 2);
    mesh.colors.reserve(ChunkSection::Volume * 4);
    mesh.indices.reserve(ChunkSection::Volume * 6);

    for (int y = 0; y < ChunkSection::Size; ++y) {
        for (int z = 0; z < ChunkSection::Size; ++z) {
            for (int x = 0; x < ChunkSection::Size; ++x) {
                const BlockId block = section.Get(x, y, z);
                if (!IsSolidBlock(block)) {
                    continue;
                }

                for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
                    const FaceDefinition& face = faces[faceIndex];
                    const BlockId neighbor =
                        section.Get(x + face.neighborX, y + face.neighborY, z + face.neighborZ);
                    if (!IsOccludingBlock(neighbor)) {
                        AppendFace(mesh, face, x, y, z,
                                   binding.FaceRect(block, static_cast<int>(faceIndex)));
                    }
                }
            }
        }
    }

    return mesh;
}

}  // namespace voxelgame
