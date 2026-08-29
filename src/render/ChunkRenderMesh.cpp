#include "render/ChunkRenderMesh.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <numeric>
#include <utility>

namespace voxelgame {
namespace {

constexpr int kLiquidIdx = static_cast<int>(RenderLayer::Liquid);
constexpr int kTransparentIdx = static_cast<int>(RenderLayer::Transparent);

// Builds one GPU model from a mesh layer. Returns a zeroed Model on empty input
// or failure (check by mesh count).
Model UploadLayer(const MeshData& data, const Texture2D atlas, const Shader shader) {
    if (data.Empty() || data.VertexCount() > std::numeric_limits<std::uint16_t>::max()) {
        if (!data.Empty()) {
            TraceLog(LOG_ERROR, "VOXEL: mesh layer exceeds 16-bit index capacity");
        }
        return Model{};
    }

    Mesh mesh{};
    mesh.vertexCount = static_cast<int>(data.VertexCount());
    mesh.triangleCount = static_cast<int>(data.TriangleCount());
    mesh.vertices = static_cast<float*>(MemAlloc(data.vertices.size() * sizeof(float)));
    mesh.normals = static_cast<float*>(MemAlloc(data.normals.size() * sizeof(float)));
    mesh.texcoords = static_cast<float*>(MemAlloc(data.uvs.size() * sizeof(float)));
    mesh.texcoords2 = static_cast<float*>(MemAlloc(data.tileOrigins.size() * sizeof(float)));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(data.colors.size() * sizeof(std::uint8_t)));
    mesh.indices =
        static_cast<unsigned short*>(MemAlloc(data.indices.size() * sizeof(std::uint16_t)));

    if (mesh.vertices == nullptr || mesh.normals == nullptr || mesh.texcoords == nullptr ||
        mesh.texcoords2 == nullptr || mesh.colors == nullptr || mesh.indices == nullptr) {
        TraceLog(LOG_ERROR, "VOXEL: failed to allocate GPU mesh staging buffers");
        MemFree(mesh.vertices);
        MemFree(mesh.normals);
        MemFree(mesh.texcoords);
        MemFree(mesh.texcoords2);
        MemFree(mesh.colors);
        MemFree(mesh.indices);
        return Model{};
    }

    std::memcpy(mesh.vertices, data.vertices.data(), data.vertices.size() * sizeof(float));
    std::memcpy(mesh.normals, data.normals.data(), data.normals.size() * sizeof(float));
    std::memcpy(mesh.texcoords, data.uvs.data(), data.uvs.size() * sizeof(float));
    std::memcpy(mesh.texcoords2, data.tileOrigins.data(), data.tileOrigins.size() * sizeof(float));
    std::memcpy(mesh.colors, data.colors.data(), data.colors.size() * sizeof(std::uint8_t));
    std::memcpy(mesh.indices, data.indices.data(), data.indices.size() * sizeof(std::uint16_t));

    UploadMesh(&mesh, true);
    Model model = LoadModelFromMesh(mesh);
    model.materials[0].shader = shader;
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = atlas;
    return model;
}

}  // namespace

ChunkRenderMesh::~ChunkRenderMesh() {
    Unload();
}

ChunkRenderMesh::ChunkRenderMesh(ChunkRenderMesh&& other) noexcept {
    for (int i = 0; i < kRenderLayerCount; ++i) {
        models_[i] = other.models_[i];
        ready_[i] = other.ready_[i];
        other.models_[i] = Model{};
        other.ready_[i] = false;
    }
    blended_[0] = std::move(other.blended_[0]);
    blended_[1] = std::move(other.blended_[1]);
}

ChunkRenderMesh& ChunkRenderMesh::operator=(ChunkRenderMesh&& other) noexcept {
    if (this != &other) {
        Unload();
        for (int i = 0; i < kRenderLayerCount; ++i) {
            models_[i] = other.models_[i];
            ready_[i] = other.ready_[i];
            other.models_[i] = Model{};
            other.ready_[i] = false;
        }
        blended_[0] = std::move(other.blended_[0]);
        blended_[1] = std::move(other.blended_[1]);
    }
    return *this;
}

bool ChunkRenderMesh::Upload(const SectionMesh& data, const Texture2D atlas, const Shader shader) {
    Unload();
    bool ok = true;
    for (int i = 0; i < kRenderLayerCount; ++i) {
        const MeshData& layer = data.layers[static_cast<std::size_t>(i)];
        if (layer.Empty()) {
            continue;
        }
        models_[i] = UploadLayer(layer, atlas, shader);
        ready_[i] = models_[i].meshCount > 0;
        ok = ok && ready_[i];

        if (ready_[i] && (i == kLiquidIdx || i == kTransparentIdx)) {
            BlendedGeom& geom = blended_[i == kLiquidIdx ? 0 : 1];
            geom.quadCentres.clear();
            geom.quadCentres.reserve(layer.quadCount);
            for (std::size_t q = 0; q < layer.quadCount; ++q) {
                Vector3 centre{0.0F, 0.0F, 0.0F};
                for (std::size_t v = 0; v < 4; ++v) {
                    const std::size_t base = (q * 4 + v) * 3;
                    centre.x += layer.vertices[base + 0];
                    centre.y += layer.vertices[base + 1];
                    centre.z += layer.vertices[base + 2];
                }
                geom.quadCentres.push_back({centre.x * 0.25F, centre.y * 0.25F, centre.z * 0.25F});
            }
            geom.baseIndices.assign(layer.indices.begin(), layer.indices.end());
        }
    }
    return ok;
}

void ChunkRenderMesh::SortBlended(const Vector3 cameraLocal) {
    for (int slot = 0; slot < 2; ++slot) {
        const int layer = slot == 0 ? kLiquidIdx : kTransparentIdx;
        BlendedGeom& geom = blended_[slot];
        if (!ready_[layer] || geom.quadCentres.size() < 2) {
            continue;
        }

        std::vector<int> order(geom.quadCentres.size());
        std::iota(order.begin(), order.end(), 0);
        const auto distSq = [&](const int q) {
            const float dx = geom.quadCentres[static_cast<std::size_t>(q)].x - cameraLocal.x;
            const float dy = geom.quadCentres[static_cast<std::size_t>(q)].y - cameraLocal.y;
            const float dz = geom.quadCentres[static_cast<std::size_t>(q)].z - cameraLocal.z;
            return dx * dx + dy * dy + dz * dz;
        };
        std::sort(order.begin(), order.end(),
                  [&](const int a, const int b) { return distSq(a) > distSq(b); });

        geom.scratch.clear();
        geom.scratch.reserve(geom.baseIndices.size());
        for (const int q : order) {
            for (int k = 0; k < 6; ++k) {
                geom.scratch.push_back(geom.baseIndices[static_cast<std::size_t>(q * 6 + k)]);
            }
        }
        UpdateMeshBuffer(models_[layer].meshes[0], 6, geom.scratch.data(),
                         static_cast<int>(geom.scratch.size() * sizeof(unsigned short)), 0);
    }
}

void ChunkRenderMesh::DrawLayer(const Vector3 position, const RenderLayer layer) const {
    const int i = static_cast<int>(layer);
    if (ready_[i]) {
        DrawModel(models_[i], position, 1.0F, WHITE);
    }
}

bool ChunkRenderMesh::HasLayer(const RenderLayer layer) const noexcept {
    return ready_[static_cast<int>(layer)];
}

void ChunkRenderMesh::DropRefs(Model& model) noexcept {
    if (model.materialCount > 0) {
        model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture.id = rlGetTextureIdDefault();
        model.materials[0].shader.id = rlGetShaderIdDefault();
    }
}

void ChunkRenderMesh::Unload() noexcept {
    for (int i = 0; i < kRenderLayerCount; ++i) {
        if (ready_[i]) {
            DropRefs(models_[i]);
            UnloadModel(models_[i]);
            models_[i] = Model{};
            ready_[i] = false;
        }
    }
    for (BlendedGeom& geom : blended_) {
        geom.quadCentres.clear();
        geom.baseIndices.clear();
        geom.scratch.clear();
    }
}

}  // namespace voxelgame
