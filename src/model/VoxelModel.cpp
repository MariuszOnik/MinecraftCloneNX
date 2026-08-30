#include "model/VoxelModel.hpp"

namespace voxelgame::vmodel {

namespace {
constexpr float kDegToRad = 3.14159265358979323846F / 180.0F;
}

const ModelMaterial* VoxelModel::Material(const std::uint8_t index) const noexcept {
    if (index == 0 || index > palette.size()) {
        return nullptr;
    }
    return &palette[static_cast<std::size_t>(index) - 1];
}

std::vector<Mat4> ResolvePartMatrices(const VoxelModel& model, const Mat4& root) {
    std::vector<Mat4> world(model.parts.size(), Mat4::Identity());
    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        const VoxelModelPart& part = model.parts[i];
        const Mat4 parent =
            part.parent < 0 ? root : world[static_cast<std::size_t>(part.parent)];
        const Vec3 r{part.rotationDegrees.x * kDegToRad, part.rotationDegrees.y * kDegToRad,
                     part.rotationDegrees.z * kDegToRad};
        const Mat4 local = Mat4::Translate(part.position.x, part.position.y, part.position.z) *
                           Mat4::Translate(part.pivot.x, part.pivot.y, part.pivot.z) *
                           Mat4::RotateXYZ(r) *
                           Mat4::Translate(-part.pivot.x, -part.pivot.y, -part.pivot.z);
        world[i] = parent * local;
    }
    return world;
}

}  // namespace voxelgame::vmodel
