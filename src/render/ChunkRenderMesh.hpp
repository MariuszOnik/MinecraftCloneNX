#pragma once

#include "world/MeshData.hpp"

#include <raylib.h>

namespace voxelgame {

class ChunkRenderMesh {
public:
    ChunkRenderMesh() = default;
    ~ChunkRenderMesh();

    ChunkRenderMesh(const ChunkRenderMesh&) = delete;
    ChunkRenderMesh& operator=(const ChunkRenderMesh&) = delete;

    bool Upload(const MeshData& data);
    void Draw(Vector3 position) const;
    [[nodiscard]] bool IsReady() const noexcept { return ready_; }

private:
    void Unload() noexcept;

    Model model_{};
    bool ready_ = false;
};

}  // namespace voxelgame
