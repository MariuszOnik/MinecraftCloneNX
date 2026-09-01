#pragma once

#include "world/BlockAtlasBinding.hpp"

#include <raylib.h>

#include <string>

namespace voxelgame {

class AssetPaths;

struct LoadedAtlas {
    Texture2D texture{};
    BlockAtlasBinding binding{};
    const char* sourceLabel = "procedural";
};

// Loads a block atlas by base name (e.g. "atlases/blocks"): the "<base>.json"
// descriptor plus the PNG it names, resolved SD-card-first. A missing / invalid
// descriptor falls back to the compiled block->tile defaults; a missing texture
// falls back to the procedural atlas. Every fallback is logged.
[[nodiscard]] LoadedAtlas LoadBlockAtlas(const AssetPaths& assets, const std::string& base);

}  // namespace voxelgame
