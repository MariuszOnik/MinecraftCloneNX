#pragma once

#include "world/Block.hpp"        // RenderLayer
#include "world/ChunkMesher.hpp"  // SectionMesh

#include <raylib.h>

#include <vector>

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

    // Re-orders the blended layers' triangles far-to-near from `cameraLocal`
    // (camera position relative to the section origin) so overlapping panes and
    // water composite correctly. Cheap: a per-quad sort of a small mesh.
    void SortBlended(Vector3 cameraLocal);

private:
    struct BlendedGeom {
        std::vector<Vector3> quadCentres;             // section-local
        std::vector<unsigned short> baseIndices;      // 6 per quad, build order
        std::vector<unsigned short> scratch;
    };

    void Unload() noexcept;
    static void DropRefs(Model& model) noexcept;

    Model models_[kRenderLayerCount]{};
    bool ready_[kRenderLayerCount]{};
    BlendedGeom blended_[2]{};  // [0] = Liquid, [1] = Transparent
};

}  // namespace voxelgame
