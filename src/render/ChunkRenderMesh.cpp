#include "render/ChunkRenderMesh.hpp"

#include <rlgl.h>

#include <cstring>
#include <limits>

namespace voxelgame {

ChunkRenderMesh::~ChunkRenderMesh() {
    Unload();
}

ChunkRenderMesh::ChunkRenderMesh(ChunkRenderMesh&& other) noexcept
    : model_(other.model_), ready_(other.ready_) {
    other.model_ = {};
    other.ready_ = false;
}

ChunkRenderMesh& ChunkRenderMesh::operator=(ChunkRenderMesh&& other) noexcept {
    if (this != &other) {
        Unload();
        model_ = other.model_;
        ready_ = other.ready_;
        other.model_ = {};
        other.ready_ = false;
    }
    return *this;
}

bool ChunkRenderMesh::Upload(const MeshData& data, const Texture2D atlas, const Shader shader) {
    Unload();
    if (data.Empty()) {
        TraceLog(LOG_INFO, "VOXEL: Empty section has no GPU mesh");
        return true;
    }
    if (data.VertexCount() > std::numeric_limits<std::uint16_t>::max()) {
        TraceLog(LOG_ERROR, "VOXEL: Mesh exceeds 16-bit index capacity");
        return false;
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
        TraceLog(LOG_ERROR, "VOXEL: Failed to allocate GPU mesh staging buffers");
        if (mesh.vertices != nullptr) MemFree(mesh.vertices);
        if (mesh.normals != nullptr) MemFree(mesh.normals);
        if (mesh.texcoords != nullptr) MemFree(mesh.texcoords);
        if (mesh.texcoords2 != nullptr) MemFree(mesh.texcoords2);
        if (mesh.colors != nullptr) MemFree(mesh.colors);
        if (mesh.indices != nullptr) MemFree(mesh.indices);
        return false;
    }

    std::memcpy(mesh.vertices, data.vertices.data(), data.vertices.size() * sizeof(float));
    std::memcpy(mesh.normals, data.normals.data(), data.normals.size() * sizeof(float));
    std::memcpy(mesh.texcoords, data.uvs.data(), data.uvs.size() * sizeof(float));
    std::memcpy(mesh.texcoords2, data.tileOrigins.data(),
                data.tileOrigins.size() * sizeof(float));
    std::memcpy(mesh.colors, data.colors.data(), data.colors.size() * sizeof(std::uint8_t));
    std::memcpy(mesh.indices, data.indices.data(), data.indices.size() * sizeof(std::uint16_t));

    UploadMesh(&mesh, true);
    model_ = LoadModelFromMesh(mesh);
    model_.materials[0].shader = shader;
    model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = atlas;
    ready_ = true;
    TraceLog(LOG_INFO, "VOXEL: Uploaded mesh: %i vertices, %i triangles", mesh.vertexCount,
             mesh.triangleCount);
    return true;
}

void ChunkRenderMesh::Draw(const Vector3 position) const {
    if (ready_) {
        DrawModel(model_, position, 1.0F, WHITE);
    }
}

void ChunkRenderMesh::Unload() noexcept {
    if (ready_) {
        // The atlas and shader are caller-owned; drop the references so
        // UnloadModel does not free resources still in use elsewhere.
        model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture.id = rlGetTextureIdDefault();
        model_.materials[0].shader.id = rlGetShaderIdDefault();
        UnloadModel(model_);
        model_ = {};
        ready_ = false;
    }
}

}  // namespace voxelgame
