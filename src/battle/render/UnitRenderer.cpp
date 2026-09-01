#include "battle/render/UnitRenderer.hpp"

#include "battle/TileGrid.hpp"
#include "model/ModelMath.hpp"
#include "render/ModelLoad.hpp"

#include <raylib.h>

namespace voxelgame::battle {
namespace {

Color TeamColor(const int team) noexcept {
    return team == 0 ? Color{80, 150, 235, 235} : Color{225, 95, 80, 235};
}

}  // namespace

UnitRenderer::UnitRenderer(const AssetPaths& assets) : assets_(assets) {
    idleClip_ = LoadAnimationClip(assets_, "animations/humanoid_idle");
    if (idleClip_) {
        idleAnimator_.Play(&*idleClip_, 0.0F);
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
}

void UnitRenderer::Draw(const UnitRegistry& units, const TileGrid& grid) const {
    using vmodel::Mat4;

    units.ForEach([&](UnitHandle, const Unit& unit) {
        const Tile& tile = grid.At(unit.tileX, unit.tileZ);
        const float fx = static_cast<float>(unit.tileX) + 0.5F;
        const float fz = static_cast<float>(unit.tileZ) + 0.5F;
        const float fy = static_cast<float>(tile.height);

        const VoxelModelRenderMesh& mesh = MeshFor(unit.model);
        if (mesh.Ready()) {
            const Mat4 root = Mat4::Translate(fx, fy, fz) *
                              Mat4::RotateXYZ({0.0F, -FacingYaw(unit.facing), 0.0F}) *
                              Mat4::Scale(mesh.VoxelSize());
            mesh.Draw(root, idleAnimator_.Pose(mesh.Model()));
        }

        // Team disc under the unit: a flat coin so the side is readable at any
        // camera angle.
        const Color ring = TeamColor(unit.team);
        DrawCylinderEx({fx, fy + 0.02F, fz}, {fx, fy + 0.07F, fz}, 0.46F, 0.46F, 20, ring);
        DrawCylinderWiresEx({fx, fy + 0.02F, fz}, {fx, fy + 0.08F, fz}, 0.46F, 0.46F, 20,
                            Fade(BLACK, 0.35F));
    });
}

}  // namespace voxelgame::battle
