#include "render/ChunkRenderMesh.hpp"

#include <cstring>
#include <limits>

namespace voxelgame {

ChunkRenderMesh::~ChunkRenderMesh() {
    Unload();
}

bool ChunkRenderMesh::Upload(const MeshData& data) {
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
    mesh.colors = static_cast<unsigned char*>(MemAlloc(data.colors.size() * sizeof(std::uint8_t)));
    mesh.indices =
        static_cast<unsigned short*>(MemAlloc(data.indices.size() * sizeof(std::uint16_t)));

    if (mesh.vertices == nullptr || mesh.normals == nullptr || mesh.colors == nullptr ||
        mesh.indices == nullptr) {
        TraceLog(LOG_ERROR, "VOXEL: Failed to allocate GPU mesh staging buffers");
        if (mesh.vertices != nullptr) MemFree(mesh.vertices);
        if (mesh.normals != nullptr) MemFree(mesh.normals);
        if (mesh.colors != nullptr) MemFree(mesh.colors);
        if (mesh.indices != nullptr) MemFree(mesh.indices);
        return false;
    }

    std::memcpy(mesh.vertices, data.vertices.data(), data.vertices.size() * sizeof(float));
    std::memcpy(mesh.normals, data.normals.data(), data.normals.size() * sizeof(float));
    std::memcpy(mesh.colors, data.colors.data(), data.colors.size() * sizeof(std::uint8_t));
    std::memcpy(mesh.indices, data.indices.data(), data.indices.size() * sizeof(std::uint16_t));

    UploadMesh(&mesh, true);
    model_ = LoadModelFromMesh(mesh);
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
        UnloadModel(model_);
        model_ = {};
        ready_ = false;
    }
}

}  // namespace voxelgame
