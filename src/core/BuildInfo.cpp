#include "core/BuildInfo.hpp"

#include "voxelgame/build_config.hpp"
#include "platform/Platform.hpp"

namespace voxelgame {

BuildInfo GetBuildInfo() noexcept {
    return {
        "VoxelGame",
        VOXELGAME_VERSION,
        VOXELGAME_COMMIT,
        GetPlatformName(),
    };
}

bool HasValidBuildInfo(const BuildInfo& info) noexcept {
    return !info.name.empty() && !info.version.empty() && !info.commit.empty() &&
           !info.platform.empty();
}

std::string_view ShortCommit(const std::string_view commit) noexcept {
    constexpr std::size_t shortLength = 12;
    return commit.substr(0, commit.size() < shortLength ? commit.size() : shortLength);
}

}  // namespace voxelgame

