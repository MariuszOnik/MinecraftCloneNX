#include "render/ChunkRenderMesh.hpp"

#include <rlgl.h>

#include <cstring>
#include <limits>

namespace voxelgame {
namespace {

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
    for (int i = 0; i < 3; ++i) {
        models_[i] = other.models_[i];
        ready_[i] = other.ready_[i];
        other.models_[i] = Model{};
        other.ready_[i] = false;
    }
}

ChunkRenderMesh& ChunkRenderMesh::operator=(ChunkRenderMesh&& other) noexcept {
    if (this != &other) {
        Unload();
        for (int i = 0; i < 3; ++i) {
            models_[i] = other.models_[i];
            ready_[i] = other.ready_[i];
            other.models_[i] = Model{};
            other.ready_[i] = false;
        }
    }
    return *this;
}

bool ChunkRenderMesh::Upload(const SectionMesh& data, const Texture2D atlas, const Shader shader) {
    Unload();
    bool ok = true;
    for (int i = 0; i < 3; ++i) {
        const MeshData& layer = data.layers[static_cast<std::size_t>(i)];
        if (layer.Empty()) {
            continue;
        }
        models_[i] = UploadLayer(layer, atlas, shader);
        ready_[i] = models_[i].meshCount > 0;
        ok = ok && ready_[i];
    }
    return ok;
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
    for (int i = 0; i < 3; ++i) {
        if (ready_[i]) {
            DropRefs(models_[i]);
            UnloadModel(models_[i]);
            models_[i] = Model{};
            ready_[i] = false;
        }
    }
}

}  // namespace voxelgame
