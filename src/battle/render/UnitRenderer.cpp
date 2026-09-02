#include "battle/render/UnitRenderer.hpp"

#include "battle/TileGrid.hpp"
#include "model/ModelMath.hpp"
#include "render/ModelLoad.hpp"

namespace voxelgame::battle {
namespace {

Color TeamColor(const int team) noexcept {
    return team == 0 ? Color{80, 150, 235, 235} : Color{225, 95, 80, 235};
}

}  // namespace

UnitRenderer::UnitRenderer(const AssetPaths& assets) : assets_(assets) {
    idleClip_ = LoadAnimationClip(assets_, "animations/humanoid_idle");
    walkClip_ = LoadAnimationClip(assets_, "animations/humanoid_walk");
    if (idleClip_) {
        idleAnimator_.Play(&*idleClip_, 0.0F);
    }
    if (walkClip_) {
        walkAnimator_.Play(&*walkClip_, 0.0F);
    }
}

const VoxelModelRenderMesh& UnitRenderer::MeshFor(const std::string& name) const {
    const auto it = meshCache_.find(name);
    if (it != meshCache_.end()) {
        return it->second;
    }
    return meshCache_.emplace(name, LoadModelMesh(assets_, "models/" + name)).first->second;
}

void UnitRenderer::Update(const float dt) {
    idleAnimator_.Update(dt);
    walkAnimator_.Update(dt);
}

void UnitRenderer::BeginWalk(const int unitIndex) {
    walkIndex_ = unitIndex;
}

void UnitRenderer::SetWalk(const Vector3 worldPosition, const float yawRadians) {
    walkPos_ = worldPosition;
    walkYaw_ = yawRadians;
}

void UnitRenderer::EndWalk() {
    walkIndex_ = -1;
}

void UnitRenderer::Draw(const UnitRegistry& units, const TileGrid& grid) const {
    using vmodel::Mat4;

    units.ForEach([&](UnitHandle handle, const Unit& unit) {
        const bool walking = handle.index == walkIndex_;

        float fx;
        float fy;
        float fz;
        float yaw;
        if (walking) {
            fx = walkPos_.x;
            fy = walkPos_.y;
            fz = walkPos_.z;
            yaw = walkYaw_;
        } else {
            fx = static_cast<float>(unit.tileX) + 0.5F;
            fz = static_cast<float>(unit.tileZ) + 0.5F;
            fy = static_cast<float>(grid.At(unit.tileX, unit.tileZ).height);
            yaw = FacingYaw(unit.facing);
        }

        const VoxelModelRenderMesh& mesh = MeshFor(unit.model);
        if (mesh.Ready()) {
            const Mat4 root = Mat4::Translate(fx, fy, fz) *
                              Mat4::RotateXYZ({0.0F, -yaw, 0.0F}) * Mat4::Scale(mesh.VoxelSize());
            const vmodel::Animator& anim = walking ? walkAnimator_ : idleAnimator_;
            mesh.Draw(root, anim.Pose(mesh.Model()));
        }

        const Color ring = TeamColor(unit.team);
        DrawCylinderEx({fx, fy + 0.02F, fz}, {fx, fy + 0.07F, fz}, 0.46F, 0.46F, 20, ring);
        DrawCylinderWiresEx({fx, fy + 0.02F, fz}, {fx, fy + 0.08F, fz}, 0.46F, 0.46F, 20,
                            Fade(BLACK, 0.35F));
    });
}

}  // namespace voxelgame::battle
