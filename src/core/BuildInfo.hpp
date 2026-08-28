#pragma once

#include <string_view>

namespace voxelgame {

struct BuildInfo {
    std::string_view name;
    std::string_view version;
    std::string_view commit;
    std::string_view platform;
};

[[nodiscard]] BuildInfo GetBuildInfo() noexcept;
[[nodiscard]] bool HasValidBuildInfo(const BuildInfo& info) noexcept;
[[nodiscard]] std::string_view ShortCommit(std::string_view commit) noexcept;

}  // namespace voxelgame

