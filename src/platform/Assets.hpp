#pragma once

#include <string>
#include <string_view>

namespace voxelgame {

// Resolves resource-relative paths (e.g. "atlases/blocks.png", "scripts/chicken.lua")
// against the runtime asset locations, in priority order:
//
//   Switch : sdmc:/switch/voxelgame/assets/<rel>   (editable on the SD card)
//            romfs:/assets/<rel>                    (bundled fallback in the .nro)
//   Desktop: <executable dir>/assets/<rel>
//
// The SD-card location lets us iterate on atlases, models and Lua scripts without
// rebuilding the NRO; the bundled copy keeps the .nro self-contained.
class AssetPaths {
public:
    enum class Origin {
        DesktopAssets,  // <exe dir>/assets
        SdCard,         // sdmc:/switch/voxelgame/assets
        Bundle,         // romfs:/assets (inside the .nro)
    };

    struct Resolved {
        std::string path;  // best candidate (existing one, or the primary if none exist)
        bool found;        // whether that path exists
        Origin origin;     // which location `path` points into
    };

    // desktopRoot: directory containing the "assets/" folder on desktop, typically
    // GetApplicationDirectory(). Ignored on Switch.
    explicit AssetPaths(std::string_view desktopRoot);

    [[nodiscard]] Resolved Resolve(std::string_view relative) const;

    // Writable path for the single world save: sdmc:/switch/voxelgame/<name> on
    // Switch, next to the executable on desktop.
    [[nodiscard]] std::string WritablePath(std::string_view name) const;

    // Location the SD-card assets are expected at, for diagnostics.
    [[nodiscard]] static std::string_view SdCardRoot() noexcept;

private:
    std::string desktopRoot_;
};

}  // namespace voxelgame
