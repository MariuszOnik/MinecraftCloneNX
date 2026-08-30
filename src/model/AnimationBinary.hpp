#pragma once

#include "model/Animation.hpp"

#include <optional>
#include <ostream>
#include <string>

namespace voxelgame::vmodel {

// Runtime animation container: magic "VXA1" + version, then the clip. Compiled
// from a .vxa.json by the asset compiler; loaded in preference to the JSON.
inline constexpr char kAnimationMagic[4] = {'V', 'X', 'A', '1'};
inline constexpr std::uint32_t kAnimationVersion = 1;

bool WriteAnimationBinary(std::ostream& out, const AnimationClip& clip);
bool WriteAnimationBinaryFile(const std::string& path, const AnimationClip& clip);

[[nodiscard]] std::optional<AnimationClip> ReadAnimationBinary(std::istream& in);
[[nodiscard]] std::optional<AnimationClip> ReadAnimationBinaryFile(const std::string& path);

}  // namespace voxelgame::vmodel
