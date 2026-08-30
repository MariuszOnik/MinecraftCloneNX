#include "model/VoxelModelMesher.hpp"

#include <array>
#include <cstddef>

namespace voxelgame::vmodel {
namespace {

// Directional shading, indexed [axis][0 = +axis, 1 = -axis]. Matches the chunk
// mesher so models and terrain catch light the same way.
constexpr float kShade[3][2] = {
    {0.82F, 0.72F},  // +X, -X
    {1.00F, 0.48F},  // +Y, -Y
    {0.90F, 0.64F},  // +Z, -Z
};

std::uint8_t Modulate(const std::uint8_t channel, const float shade) noexcept {
    return static_cast<std::uint8_t>(static_cast<float>(channel) * shade);
}

void AppendFace(ModelMeshData& mesh, const int x, const int y, const int z, const int axis,
                const bool positive, const ModelMaterial& material) {
    const int uAxis = (axis + 1) % 3;
    const int vAxis = (axis + 2) % 3;
    const auto first = static_cast<std::uint16_t>(mesh.VertexCount());

    std::array<float, 3> normal{0.0F, 0.0F, 0.0F};
    normal[static_cast<std::size_t>(axis)] = positive ? 1.0F : -1.0F;

    const float shade = kShade[axis][positive ? 0 : 1];
    const std::array<std::uint8_t, 4> colour{Modulate(material.red, shade),
                                             Modulate(material.green, shade),
                                             Modulate(material.blue, shade), material.alpha};

    const std::array<std::array<int, 2>, 4> corners{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    for (const auto& corner : corners) {
        std::array<int, 3> pos{x, y, z};
        pos[static_cast<std::size_t>(axis)] += positive ? 1 : 0;
        pos[static_cast<std::size_t>(uAxis)] += corner[0];
        pos[static_cast<std::size_t>(vAxis)] += corner[1];
        mesh.vertices.insert(mesh.vertices.end(), {static_cast<float>(pos[0]),
                                                  static_cast<float>(pos[1]),
                                                  static_cast<float>(pos[2])});
        mesh.normals.insert(mesh.normals.end(), {normal[0], normal[1], normal[2]});
        mesh.colors.insert(mesh.colors.end(), colour.begin(), colour.end());
    }

    if (positive) {
        for (const int i : {0, 1, 2, 0, 2, 3}) {
            mesh.indices.push_back(static_cast<std::uint16_t>(first + i));
        }
    } else {
        for (const int i : {0, 2, 1, 0, 3, 2}) {
            mesh.indices.push_back(static_cast<std::uint16_t>(first + i));
        }
    }
    ++mesh.quadCount;
}

}  // namespace

ModelMeshData BuildPartMesh(const VoxelModelPart& part, const VoxelModel& model) {
    const VoxelGrid& grid = part.grid;
    ModelMeshData mesh;

    for (int y = 0; y < grid.sizeY; ++y) {
        for (int z = 0; z < grid.sizeZ; ++z) {
            for (int x = 0; x < grid.sizeX; ++x) {
                const std::uint8_t voxel = grid.At(x, y, z);
                if (voxel == 0) {
                    continue;
                }
                const ModelMaterial* material = model.Material(voxel);
                if (material == nullptr) {
                    continue;
                }
                static constexpr std::array<std::array<int, 3>, 6> faces{{
                    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                    {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
                }};
                for (const auto& face : faces) {
                    if (grid.At(x + face[0], y + face[1], z + face[2]) != 0) {
                        continue;
                    }
                    const int axis = face[0] != 0 ? 0 : (face[1] != 0 ? 1 : 2);
                    const bool positive = face[0] + face[1] + face[2] > 0;
                    AppendFace(mesh, x, y, z, axis, positive, *material);
                }
            }
        }
    }
    return mesh;
}

}  // namespace voxelgame::vmodel
