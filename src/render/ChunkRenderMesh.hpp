#pragma once

#include "world/Block.hpp"        // RenderLayer
#include "world/ChunkMesher.hpp"  // SectionMesh

#include <raylib.h>

namespace voxelgame {

// One section's GPU geometry, one model per render layer. Atlas and shader are
// caller-owned; the render mesh only references them.
class ChunkRenderMesh {
public:
    ChunkRenderMesh() = default;
    ~ChunkRenderMesh();

    ChunkRenderMesh(const ChunkRenderMesh&) = delete;
    ChunkRenderMesh& operator=(const ChunkRenderMesh&) = delete;

    ChunkRenderMesh(ChunkRenderMesh&& other) noexcept;
    ChunkRenderMesh& operator=(ChunkRenderMesh&& other) noexcept;

    bool Upload(const SectionMesh& data, Texture2D atlas, Shader shader);
    void DrawLayer(Vector3 position, RenderLayer layer) const;
    [[nodiscard]] bool HasLayer(RenderLayer layer) const noexcept;

private:
    void Unload() noexcept;
    static void DropRefs(Model& model) noexcept;

    Model models_[3]{};
    bool ready_[3]{};
};

}  // namespace voxelgame
