#include "world/ChunkMesher.hpp"

#include "world/Block.hpp"

#include <array>
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

std::uint8_t ShadeChannel(const std::uint8_t channel, const float shade) noexcept {
    return static_cast<std::uint8_t>(static_cast<float>(channel) * shade);
}

void AppendFace(MeshData& mesh, const FaceDefinition& face, const int x, const int y,
                const int z, const BlockColor color) {
    const auto firstVertex = static_cast<std::uint16_t>(mesh.VertexCount());
    for (const auto& corner : face.corners) {
        mesh.vertices.push_back(static_cast<float>(x) + corner[0]);
        mesh.vertices.push_back(static_cast<float>(y) + corner[1]);
        mesh.vertices.push_back(static_cast<float>(z) + corner[2]);

        mesh.normals.insert(mesh.normals.end(), face.normal.begin(), face.normal.end());

        mesh.colors.push_back(ShadeChannel(color.red, face.shade));
        mesh.colors.push_back(ShadeChannel(color.green, face.shade));
        mesh.colors.push_back(ShadeChannel(color.blue, face.shade));
        mesh.colors.push_back(color.alpha);
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

MeshData ChunkMesher::Build(const ChunkSection& section) const {
    MeshData mesh;
    mesh.vertices.reserve(ChunkSection::Volume * 3);
    mesh.normals.reserve(ChunkSection::Volume * 3);
    mesh.colors.reserve(ChunkSection::Volume * 4);
    mesh.indices.reserve(ChunkSection::Volume * 6);

    for (int y = 0; y < ChunkSection::Size; ++y) {
        for (int z = 0; z < ChunkSection::Size; ++z) {
            for (int x = 0; x < ChunkSection::Size; ++x) {
                const BlockId block = section.Get(x, y, z);
                if (!IsSolidBlock(block)) {
                    continue;
                }

                const BlockColor color = GetBlockDefinition(block).color;
                for (const FaceDefinition& face : faces) {
                    const BlockId neighbor =
                        section.Get(x + face.neighborX, y + face.neighborY, z + face.neighborZ);
                    if (!IsOccludingBlock(neighbor)) {
                        AppendFace(mesh, face, x, y, z, color);
                    }
                }
            }
        }
    }

    return mesh;
}

}  // namespace voxelgame
