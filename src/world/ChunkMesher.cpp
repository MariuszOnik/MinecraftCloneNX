#include "world/ChunkMesher.hpp"

#include "world/Block.hpp"
#include "world/BlockAtlasLayout.hpp"
#include "world/World.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace voxelgame {
namespace {

// Directional face shading, indexed [axis][0 = +axis, 1 = -axis].
constexpr float kShade[3][2] = {
    {0.82F, 0.72F},  // +X, -X
    {1.00F, 0.48F},  // +Y, -Y
    {0.90F, 0.64F},  // +Z, -Z
};

// Atlas binding face order is +X, -X, +Y, -Y, +Z, -Z.
constexpr int FaceIndex(const int axis, const bool positive) noexcept {
    return axis * 2 + (positive ? 0 : 1);
}

std::uint8_t ShadeChannel(const float shade) noexcept {
    return static_cast<std::uint8_t>(255.0F * shade);
}

// Two identical non-opaque blocks (glass-glass, water-water, leaves-leaves) hide
// the shared face between them, like opaque neighbours do (PLAN.md 5.4).
bool SameNonOpaque(const BlockId a, const BlockId b) noexcept {
    return a == b && a != blocks::Air && BlockRenderLayer(a) != RenderLayer::Opaque;
}

// A thin glass pane: two crossed quads through the block centre, each drawn from
// both sides so it reads from any angle.
void AppendCross(MeshData& mesh, const int bx, const int by, const int bz,
                 const atlas::TileRect uv) {
    constexpr float e = 0.02F;
    const float x = static_cast<float>(bx);
    const float y = static_cast<float>(by);
    const float z = static_cast<float>(bz);

    const std::array<std::array<std::array<float, 3>, 4>, 2> planes{{
        // x = bx + 0.5, spanning z and y
        {{{x + 0.5F, y, z}, {x + 0.5F, y, z + 1}, {x + 0.5F, y + 1, z + 1}, {x + 0.5F, y + 1, z}}},
        // z = bz + 0.5, spanning x and y
        {{{x, y, z + 0.5F}, {x + 1, y, z + 0.5F}, {x + 1, y + 1, z + 0.5F}, {x, y + 1, z + 0.5F}}},
    }};
    const std::array<std::array<float, 3>, 2> planeNormals{{{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}}};
    const std::array<std::array<float, 2>, 4> cornerUv{{{e, 1 - e}, {1 - e, 1 - e}, {1 - e, e},
                                                        {e, e}}};

    for (std::size_t p = 0; p < planes.size(); ++p) {
        for (int side = 0; side < 2; ++side) {
            const float sign = side == 0 ? 1.0F : -1.0F;
            const auto first = static_cast<std::uint16_t>(mesh.VertexCount());
            for (std::size_t c = 0; c < 4; ++c) {
                mesh.vertices.insert(mesh.vertices.end(), {planes[p][c][0], planes[p][c][1],
                                                          planes[p][c][2]});
                mesh.normals.insert(mesh.normals.end(),
                                    {planeNormals[p][0] * sign, planeNormals[p][1] * sign,
                                     planeNormals[p][2] * sign});
                mesh.uvs.push_back(cornerUv[c][0]);
                mesh.uvs.push_back(cornerUv[c][1]);
                mesh.tileOrigins.push_back(uv.u0);
                mesh.tileOrigins.push_back(uv.v0);
                mesh.colors.insert(mesh.colors.end(),
                                   {std::uint8_t{217}, std::uint8_t{217}, std::uint8_t{217},
                                    std::uint8_t{255}});
            }
            if (side == 0) {
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
    }
}

struct MaskCell {
    bool present = false;
    bool positive = false;
    std::uint8_t tile = 0;
    std::uint8_t layer = 0;  // RenderLayer
    float u0 = 0.0F;
    float v0 = 0.0F;

    [[nodiscard]] bool Matches(const MaskCell& other) const noexcept {
        return present && other.present && positive == other.positive && tile == other.tile &&
               layer == other.layer;
    }
};

// Nudge a merged quad's far edge just inside the last tile so fract() never
// wraps a coordinate of exactly `span` back to 0 and samples the wrong column.
constexpr float kEdgeBias = 1.0F / 512.0F;

void AppendQuad(MeshData& mesh, const int axis, const int uAxis, const int vAxis, const int plane,
                const int i, const int j, const int w, const int h, const MaskCell& cell) {
    const auto first = static_cast<std::uint16_t>(mesh.VertexCount());

    std::array<float, 3> normal{0.0F, 0.0F, 0.0F};
    normal[static_cast<std::size_t>(axis)] = cell.positive ? 1.0F : -1.0F;
    const std::uint8_t shade = ShadeChannel(kShade[axis][cell.positive ? 0 : 1]);

    // Tile repeats along the texture's horizontal / vertical axes.
    const float spanU = static_cast<float>(axis == 0 ? h : w);
    const float spanV = static_cast<float>(axis == 0 ? w : h);

    const std::array<std::array<int, 2>, 4> corners{{{0, 0}, {w, 0}, {w, h}, {0, h}}};
    for (const auto& corner : corners) {
        const int pu = corner[0];
        const int pv = corner[1];

        std::array<float, 3> pos{};
        pos[static_cast<std::size_t>(axis)] = static_cast<float>(plane);
        pos[static_cast<std::size_t>(uAxis)] = static_cast<float>(i + pu);
        pos[static_cast<std::size_t>(vAxis)] = static_cast<float>(j + pv);
        mesh.vertices.insert(mesh.vertices.end(), {pos[0], pos[1], pos[2]});
        mesh.normals.insert(mesh.normals.end(), {normal[0], normal[1], normal[2]});

        // Keep every face upright: block-space up maps to the top of the tile.
        float texU = 0.0F;
        float texV = 0.0F;
        if (axis == 1) {  // +Y / -Y: orientation is arbitrary
            texU = static_cast<float>(pu);
            texV = static_cast<float>(pv);
        } else if (axis == 0) {  // +X / -X: uAxis is world Y
            texU = static_cast<float>(pv);
            texV = static_cast<float>(w - pu);
        } else {  // +Z / -Z: vAxis is world Y
            texU = static_cast<float>(pu);
            texV = static_cast<float>(h - pv);
        }
        if (texU >= spanU) {
            texU = spanU - kEdgeBias;
        }
        if (texV >= spanV) {
            texV = spanV - kEdgeBias;
        }
        mesh.uvs.push_back(texU);
        mesh.uvs.push_back(texV);
        mesh.tileOrigins.push_back(cell.u0);
        mesh.tileOrigins.push_back(cell.v0);

        mesh.colors.insert(mesh.colors.end(), {shade, shade, shade, std::uint8_t{255}});
    }

    if (cell.positive) {
        for (const int index : {0, 1, 2, 0, 2, 3}) {
            mesh.indices.push_back(static_cast<std::uint16_t>(first + index));
        }
    } else {
        for (const int index : {0, 2, 1, 0, 3, 2}) {
            mesh.indices.push_back(static_cast<std::uint16_t>(first + index));
        }
    }
    ++mesh.quadCount;
}

template <typename BlockAt>
SectionMesh BuildMesh(const BlockAt& blockAt, const BlockAtlasBinding& binding) {
    constexpr int N = ChunkSection::Size;

    SectionMesh out;
    for (MeshData& mesh : out.layers) {
        mesh.vertices.reserve(ChunkSection::Volume);
        mesh.normals.reserve(ChunkSection::Volume);
        mesh.uvs.reserve(ChunkSection::Volume);
        mesh.tileOrigins.reserve(ChunkSection::Volume);
        mesh.colors.reserve(ChunkSection::Volume * 2);
        mesh.indices.reserve(ChunkSection::Volume);
    }

    std::array<MaskCell, static_cast<std::size_t>(N) * N> mask{};

    for (int axis = 0; axis < 3; ++axis) {
        const int uAxis = (axis + 1) % 3;
        const int vAxis = (axis + 2) % 3;
        std::array<int, 3> coord{};

        for (int slice = -1; slice < N; ++slice) {
            for (int b = 0; b < N; ++b) {
                for (int a = 0; a < N; ++a) {
                    coord[static_cast<std::size_t>(uAxis)] = a;
                    coord[static_cast<std::size_t>(vAxis)] = b;

                    coord[static_cast<std::size_t>(axis)] = slice;
                    const BlockId lo = blockAt(coord[0], coord[1], coord[2]);
                    coord[static_cast<std::size_t>(axis)] = slice + 1;
                    const BlockId hi = blockAt(coord[0], coord[1], coord[2]);

                    MaskCell cell;
                    if (IsSolidBlock(lo) && IsCubeShaped(lo) && !IsOccludingBlock(hi) &&
                        !SameNonOpaque(lo, hi)) {
                        cell.present = true;
                        cell.positive = true;
                        const int face = FaceIndex(axis, true);
                        cell.tile = binding.FaceTile(lo, face);
                        cell.layer = static_cast<std::uint8_t>(BlockRenderLayer(lo));
                        const atlas::TileRect rect = binding.FaceRect(lo, face);
                        cell.u0 = rect.u0;
                        cell.v0 = rect.v0;
                    } else if (IsSolidBlock(hi) && IsCubeShaped(hi) && !IsOccludingBlock(lo) &&
                               !SameNonOpaque(hi, lo)) {
                        cell.present = true;
                        cell.positive = false;
                        const int face = FaceIndex(axis, false);
                        cell.tile = binding.FaceTile(hi, face);
                        cell.layer = static_cast<std::uint8_t>(BlockRenderLayer(hi));
                        const atlas::TileRect rect = binding.FaceRect(hi, face);
                        cell.u0 = rect.u0;
                        cell.v0 = rect.v0;
                    }
                    mask[static_cast<std::size_t>(b) * N + a] = cell;
                }
            }

            for (int j = 0; j < N; ++j) {
                for (int i = 0; i < N;) {
                    const MaskCell start = mask[static_cast<std::size_t>(j) * N + i];
                    if (!start.present) {
                        ++i;
                        continue;
                    }

                    int w = 1;
                    while (i + w < N && mask[static_cast<std::size_t>(j) * N + i + w].Matches(start)) {
                        ++w;
                    }

                    int h = 1;
                    bool grow = true;
                    while (j + h < N && grow) {
                        for (int k = 0; k < w; ++k) {
                            if (!mask[static_cast<std::size_t>(j + h) * N + i + k].Matches(start)) {
                                grow = false;
                                break;
                            }
                        }
                        if (grow) {
                            ++h;
                        }
                    }

                    AppendQuad(out.layers[start.layer], axis, uAxis, vAxis, slice + 1, i, j, w, h,
                               start);

                    for (int l = 0; l < h; ++l) {
                        for (int k = 0; k < w; ++k) {
                            mask[static_cast<std::size_t>(j + l) * N + i + k].present = false;
                        }
                    }
                    i += w;
                }
            }
        }
    }

    // Non-cube blocks (glass panes) get their geometry emitted per voxel.
    for (int y = 0; y < N; ++y) {
        for (int z = 0; z < N; ++z) {
            for (int x = 0; x < N; ++x) {
                const BlockId block = blockAt(x, y, z);
                const BlockDefinition& def = GetBlockDefinition(block);
                if (def.solid && def.shape == BlockShape::Pane) {
                    AppendCross(out.Layer(def.layer), x, y, z,
                                binding.FaceRect(block, FaceIndex(0, true)));
                }
            }
        }
    }

    return out;
}

}  // namespace

SectionMesh ChunkMesher::Build(const ChunkSection& section, const BlockAtlasBinding& binding) const {
    return BuildMesh(
        [&section](const int x, const int y, const int z) noexcept {
            return section.Get(x, y, z);
        },
        binding);
}

SectionMesh ChunkMesher::Build(const World& world, const int chunkX, const int sectionY,
                               const int chunkZ, const BlockAtlasBinding& binding) const {
    const int baseX = chunkX * ChunkSection::Size;
    const int baseY = sectionY * ChunkSection::Size;
    const int baseZ = chunkZ * ChunkSection::Size;
    return BuildMesh(
        [&world, baseX, baseY, baseZ](const int x, const int y, const int z) noexcept {
            return world.GetBlock(baseX + x, baseY + y, baseZ + z);
        },
        binding);
}

}  // namespace voxelgame
