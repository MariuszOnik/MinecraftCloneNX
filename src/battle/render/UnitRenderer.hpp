#pragma once

#include "battle/Unit.hpp"
#include "model/Animation.hpp"
#include "render/VoxelModelRenderer.hpp"

#include <raylib.h>

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
// name so many units of the same kind share one GPU upload. One unit at a time
// can be "walking" -- drawn at a free world position with the walk clip.
class UnitRenderer {
public:
    explicit UnitRenderer(const AssetPaths& assets);

    void Update(float dt);  // advances the idle + walk animations
    void Draw(const UnitRegistry& units, const TileGrid& grid) const;

    void BeginWalk(int unitIndex);
    void SetWalk(Vector3 worldPosition, float yawRadians);
    void EndWalk();

private:
    [[nodiscard]] const VoxelModelRenderMesh& MeshFor(const std::string& name) const;

    const AssetPaths& assets_;
    mutable std::unordered_map<std::string, VoxelModelRenderMesh> meshCache_;
    std::optional<vmodel::AnimationClip> idleClip_;
    std::optional<vmodel::AnimationClip> walkClip_;
    vmodel::Animator idleAnimator_;
    vmodel::Animator walkAnimator_;

    int walkIndex_ = -1;
    Vector3 walkPos_{};
    float walkYaw_ = 0.0F;
};

}  // namespace voxelgame::battle
