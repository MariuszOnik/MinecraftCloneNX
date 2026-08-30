#pragma once

#include "model/ModelMath.hpp"
#include "model/VoxelModel.hpp"

#include <raylib.h>

#include <vector>

namespace voxelgame {

// GPU geometry for a whole voxel model: one vertex-coloured mesh per part, drawn
// through the part hierarchy. The mesh is uploaded once; only the per-part
// matrices change between frames, so animating a part never re-meshes it.
class VoxelModelRenderMesh {
public:
    VoxelModelRenderMesh() = default;
    ~VoxelModelRenderMesh();

    VoxelModelRenderMesh(const VoxelModelRenderMesh&) = delete;
    VoxelModelRenderMesh& operator=(const VoxelModelRenderMesh&) = delete;
    VoxelModelRenderMesh(VoxelModelRenderMesh&& other) noexcept;
    VoxelModelRenderMesh& operator=(VoxelModelRenderMesh&& other) noexcept;

    // Meshes and uploads every part. Returns false (and uploads nothing) if a
    // part's geometry will not fit a 16-bit index buffer.
    bool Upload(const vmodel::VoxelModel& model);

    // Draws the model with `root` applied to every part (world placement + the
    // model's voxelSize scale belong in `root`).
    void Draw(const vmodel::Mat4& root) const;

    [[nodiscard]] bool Ready() const noexcept { return ready_; }
    [[nodiscard]] std::size_t PartCount() const noexcept { return meshes_.size(); }
    [[nodiscard]] float VoxelSize() const noexcept { return model_.voxelSize; }

private:
    void Unload() noexcept;

    vmodel::VoxelModel model_;   // kept for the part hierarchy at draw time
    std::vector<Mesh> meshes_;   // one per part; vertexCount 0 for an empty part
    Material material_{};
    bool hasMaterial_ = false;
    bool ready_ = false;
};

}  // namespace voxelgame
