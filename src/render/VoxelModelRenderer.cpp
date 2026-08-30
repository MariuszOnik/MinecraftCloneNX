#include "render/VoxelModelRenderer.hpp"

#include "model/VoxelModelMesher.hpp"

#include <rlgl.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

namespace voxelgame {
namespace {

Matrix ToRaylib(const vmodel::Mat4& m) noexcept {
    Matrix r;
    r.m0 = m.m[0];   r.m1 = m.m[1];   r.m2 = m.m[2];   r.m3 = m.m[3];
    r.m4 = m.m[4];   r.m5 = m.m[5];   r.m6 = m.m[6];   r.m7 = m.m[7];
    r.m8 = m.m[8];   r.m9 = m.m[9];   r.m10 = m.m[10]; r.m11 = m.m[11];
    r.m12 = m.m[12]; r.m13 = m.m[13]; r.m14 = m.m[14]; r.m15 = m.m[15];
    return r;
}

// Builds a raylib Mesh from CPU part geometry and uploads it to the GPU.
Mesh UploadPart(const vmodel::ModelMeshData& data) {
    Mesh mesh{};
    if (data.Empty()) {
        return mesh;
    }
    mesh.vertexCount = static_cast<int>(data.VertexCount());
    mesh.triangleCount = static_cast<int>(data.TriangleCount());
    mesh.vertices = static_cast<float*>(MemAlloc(data.vertices.size() * sizeof(float)));
    mesh.normals = static_cast<float*>(MemAlloc(data.normals.size() * sizeof(float)));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(data.colors.size() * sizeof(std::uint8_t)));
    mesh.indices =
        static_cast<unsigned short*>(MemAlloc(data.indices.size() * sizeof(std::uint16_t)));

    if (mesh.vertices == nullptr || mesh.normals == nullptr || mesh.colors == nullptr ||
        mesh.indices == nullptr) {
        TraceLog(LOG_ERROR, "VOXEL: failed to allocate model mesh staging buffers");
        MemFree(mesh.vertices);
        MemFree(mesh.normals);
        MemFree(mesh.colors);
        MemFree(mesh.indices);
        return Mesh{};
    }

    std::memcpy(mesh.vertices, data.vertices.data(), data.vertices.size() * sizeof(float));
    std::memcpy(mesh.normals, data.normals.data(), data.normals.size() * sizeof(float));
    std::memcpy(mesh.colors, data.colors.data(), data.colors.size() * sizeof(std::uint8_t));
    std::memcpy(mesh.indices, data.indices.data(), data.indices.size() * sizeof(std::uint16_t));

    UploadMesh(&mesh, false);
    return mesh;
}

}  // namespace

VoxelModelRenderMesh::~VoxelModelRenderMesh() {
    Unload();
}

VoxelModelRenderMesh::VoxelModelRenderMesh(VoxelModelRenderMesh&& other) noexcept
    : model_(std::move(other.model_)),
      meshes_(std::move(other.meshes_)),
      material_(other.material_),
      hasMaterial_(other.hasMaterial_),
      ready_(other.ready_) {
    other.material_ = Material{};
    other.hasMaterial_ = false;
    other.ready_ = false;
}

VoxelModelRenderMesh& VoxelModelRenderMesh::operator=(VoxelModelRenderMesh&& other) noexcept {
    if (this != &other) {
        Unload();
        model_ = std::move(other.model_);
        meshes_ = std::move(other.meshes_);
        material_ = other.material_;
        hasMaterial_ = other.hasMaterial_;
        ready_ = other.ready_;
        other.material_ = Material{};
        other.hasMaterial_ = false;
        other.ready_ = false;
    }
    return *this;
}

bool VoxelModelRenderMesh::Upload(const vmodel::VoxelModel& model) {
    Unload();

    std::vector<vmodel::ModelMeshData> parts;
    parts.reserve(model.parts.size());
    for (const vmodel::VoxelModelPart& part : model.parts) {
        vmodel::ModelMeshData data = vmodel::BuildPartMesh(part, model);
        if (data.VertexCount() > std::numeric_limits<std::uint16_t>::max()) {
            TraceLog(LOG_ERROR, "VOXEL: model part '%s' exceeds 16-bit index capacity",
                     part.name.c_str());
            return false;
        }
        parts.push_back(std::move(data));
    }

    model_ = model;
    material_ = LoadMaterialDefault();
    hasMaterial_ = true;
    meshes_.reserve(parts.size());
    for (const vmodel::ModelMeshData& data : parts) {
        meshes_.push_back(UploadPart(data));
    }
    ready_ = true;
    return true;
}

void VoxelModelRenderMesh::Draw(const vmodel::Mat4& root) const {
    if (!ready_) {
        return;
    }
    const std::vector<vmodel::Mat4> world = vmodel::ResolvePartMatrices(model_, root);
    for (std::size_t i = 0; i < meshes_.size(); ++i) {
        if (meshes_[i].vertexCount > 0) {
            DrawMesh(meshes_[i], material_, ToRaylib(world[i]));
        }
    }
}

void VoxelModelRenderMesh::Unload() noexcept {
    for (Mesh& mesh : meshes_) {
        if (mesh.vertexCount > 0) {
            UnloadMesh(mesh);
        }
    }
    meshes_.clear();
    if (hasMaterial_) {
        UnloadMaterial(material_);  // default shader/texture are ref-checked, not freed
        material_ = Material{};
        hasMaterial_ = false;
    }
    model_ = vmodel::VoxelModel{};
    ready_ = false;
}

}  // namespace voxelgame
