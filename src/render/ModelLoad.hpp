#pragma once

#include "model/Animation.hpp"
#include "render/VoxelModelRenderer.hpp"

#include <optional>
#include <string>

namespace voxelgame {

class AssetPaths;

// Loads a voxel model by base name (e.g. "models/humanoid"): the compiled
// "<base>.vxm" binary if present, otherwise the "<base>.vxm.json" source. Every
// failure is logged; the result is an empty (not-ready) render mesh, never a
// silent fallback (PLAN.md 12).
[[nodiscard]] VoxelModelRenderMesh LoadModelMesh(const AssetPaths& assets, const std::string& base);

// Loads an animation clip by base name: "<base>.vxa" binary if present, else
// "<base>.vxa.json". nullopt on any failure.
[[nodiscard]] std::optional<vmodel::AnimationClip> LoadAnimationClip(const AssetPaths& assets,
                                                                    const std::string& base);

}  // namespace voxelgame
