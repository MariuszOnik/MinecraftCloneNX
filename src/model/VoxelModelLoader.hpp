#pragma once

#include "model/VoxelModel.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace voxelgame::vmodel {

// Parses a .vxm.json source model. On failure returns nullopt and writes a
// human-readable reason into `error`. See assets/models/*.vxm.json for the shape.
[[nodiscard]] std::optional<VoxelModel> ParseVoxelModel(std::string_view jsonText,
                                                       std::string& error);

}  // namespace voxelgame::vmodel
