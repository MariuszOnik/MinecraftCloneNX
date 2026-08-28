#include "world/ChunkMesher.hpp"

#include "world/Block.hpp"
#include "world/BlockAtlasLayout.hpp"

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

// Which tile corner each of the four face vertices maps to: (left/right, top/bottom).
constexpr std::array<std::array<int, 2>, 4> cornerUv{{
    {{0, 0}},
    {{0, 1}},
    {{1, 1}},
    {{1, 0}},
}};

static_assert(atlas::TileCount >= 1, "atlas must define at least one tile");

std::uint8_t ShadeChannel(const float shade) noexcept {
    return static_cast<std::uint8_t>(255.0F * shade);
}

void AppendFace(MeshData& mesh, const FaceDefinition& face, const int x, const int y,
                const int z, const std::uint8_t tile) {
    const auto firstVertex = static_cast<std::uint16_t>(mesh.VertexCount());

    const atlas::TileRect uv = atlas::TileRectOf(tile);
    const std::uint8_t shaded = ShadeChannel(face.shade);

    for (std::size_t corner = 0; corner < face.corners.size(); ++corner) {
        mesh.vertices.push_back(static_cast<float>(x) + face.corners[corner][0]);
        mesh.vertices.push_back(static_cast<float>(y) + face.corners[corner][1]);
        mesh.vertices.push_back(static_cast<float>(z) + face.corners[corner][2]);

        mesh.normals.insert(mesh.normals.end(), face.normal.begin(), face.normal.end());

        mesh.uvs.push_back(cornerUv[corner][0] == 0 ? uv.u0 : uv.u1);
        mesh.uvs.push_back(cornerUv[corner][1] == 0 ? uv.v0 : uv.v1);

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

MeshData ChunkMesher::Build(const ChunkSection& section) const {
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
                                   GetBlockFaceTile(block, static_cast<int>(faceIndex)));
                    }
                }
            }
        }
    }

    return mesh;
}

}  // namespace voxelgame
