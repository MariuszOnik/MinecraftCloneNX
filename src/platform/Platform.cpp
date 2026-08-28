#include "platform/Platform.hpp"

#include "voxelgame/build_config.hpp"

namespace voxelgame {

std::string_view GetPlatformName() noexcept {
#if defined(__SWITCH__)
    return "Nintendo Switch";
#elif defined(_WIN32)
    return "Windows PC";
#elif defined(__APPLE__)
    return "macOS PC";
#elif defined(__linux__)
    return "Linux PC";
#else
    return VOXELGAME_CONFIGURED_PLATFORM;
#endif
}

}  // namespace voxelgame

