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

    // The atlas texture and shader stay owned by the caller; the render mesh only
    // references them.
    bool Upload(const MeshData& data, Texture2D atlas, Shader shader);
    void Draw(Vector3 position) const;
    [[nodiscard]] bool IsReady() const noexcept { return ready_; }

private:
    void Unload() noexcept;

    Model model_{};
    bool ready_ = false;
};

}  // namespace voxelgame
