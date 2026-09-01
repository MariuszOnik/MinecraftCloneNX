#include "platform/Assets.hpp"

#include <fstream>

namespace voxelgame {
namespace {

// SD-card app folder. The battle build overrides this to "voxeltactics" so its
// editable assets sit next to their own NRO (see VOXELGAME_SD_DIR in CMake).
#ifndef VOXELGAME_SD_DIR
#define VOXELGAME_SD_DIR "voxelgame"
#endif
constexpr std::string_view kSdCardRoot = "sdmc:/switch/" VOXELGAME_SD_DIR "/assets/";
constexpr std::string_view kSdCardWritableRoot = "sdmc:/switch/" VOXELGAME_SD_DIR "/";
constexpr std::string_view kRomfsRoot = "romfs:/assets/";

bool FileExists(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return file.good();
}

std::string Join(const std::string_view root, const std::string_view relative) {
    std::string joined;
    joined.reserve(root.size() + relative.size());
    joined.append(root);
    joined.append(relative);
    return joined;
}

}  // namespace

AssetPaths::AssetPaths(const std::string_view desktopRoot) : desktopRoot_(desktopRoot) {}

std::string_view AssetPaths::SdCardRoot() noexcept {
    return kSdCardRoot;
}

std::string AssetPaths::WritablePath(const std::string_view name) const {
#if defined(__SWITCH__)
    return Join(kSdCardWritableRoot, name);
#else
    return Join(desktopRoot_, name);
#endif
}

AssetPaths::Resolved AssetPaths::Resolve(const std::string_view relative) const {
#if defined(__SWITCH__)
    const std::string sdCard = Join(kSdCardRoot, relative);
    if (FileExists(sdCard)) {
        return {sdCard, true, Origin::SdCard};
    }
    const std::string bundled = Join(kRomfsRoot, relative);
    return {bundled, FileExists(bundled), Origin::Bundle};
#else
    const std::string desktop = Join(desktopRoot_ + "assets/", relative);
    return {desktop, FileExists(desktop), Origin::DesktopAssets};
#endif
}

}  // namespace voxelgame
