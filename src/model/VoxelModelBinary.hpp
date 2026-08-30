#pragma once

#include "model/VoxelModel.hpp"

#include <optional>
#include <ostream>
#include <string>

namespace voxelgame::vmodel {

// Runtime model container: magic "VXM1" + version, then the model. Compiled from
// a .vxm.json by the asset compiler; the game loads this in preference to the
// JSON. No raw C++ structs on the wire (PLAN.md 6.3).
inline constexpr char kVoxelModelMagic[4] = {'V', 'X', 'M', '1'};
inline constexpr std::uint32_t kVoxelModelVersion = 1;

bool WriteVoxelModelBinary(std::ostream& out, const VoxelModel& model);
bool WriteVoxelModelBinaryFile(const std::string& path, const VoxelModel& model);

// nullopt if the stream is not a valid VXM1 of a supported version.
[[nodiscard]] std::optional<VoxelModel> ReadVoxelModelBinary(std::istream& in);
[[nodiscard]] std::optional<VoxelModel> ReadVoxelModelBinaryFile(const std::string& path);

}  // namespace voxelgame::vmodel
