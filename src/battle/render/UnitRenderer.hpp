#pragma once

#include "battle/Unit.hpp"
#include "model/Animation.hpp"
#include "render/VoxelModelRenderer.hpp"

#include <optional>
#include <string>
#include <unordered_map>

namespace voxelgame {
class AssetPaths;
}

namespace voxelgame::battle {

class TileGrid;

// Draws battle units: their voxel model on the tile they stand on, facing their
// direction, with a team-coloured ring underfoot. Meshes are cached by asset
// name so many units of the same kind share one GPU upload.
class UnitRenderer {
public:
    explicit UnitRenderer(const AssetPaths& assets);

    void Update(float dt);  // advances the shared idle animation
    void Draw(const UnitRegistry& units, const TileGrid& grid) const;

private:
    [[nodiscard]] const VoxelModelRenderMesh& MeshFor(const std::string& name) const;

    const AssetPaths& assets_;
    mutable std::unordered_map<std::string, VoxelModelRenderMesh> meshCache_;
    std::optional<vmodel::AnimationClip> idleClip_;
    vmodel::Animator idleAnimator_;
};

}  // namespace voxelgame::battle
