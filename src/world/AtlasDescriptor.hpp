#pragma once

#include "world/Block.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace voxelgame {

// Parsed contents of an atlas descriptor JSON (see assets/atlases/blocks.json).
// Describes the atlas grid and, per block name, the tile each cube face samples.
struct AtlasDescriptor {
    std::string texture;           // image filename, relative to the descriptor
    int atlasWidth = 0;
    int atlasHeight = 0;
    int tileSize = 0;              // tile content size in pixels
    int padding = 0;              // gutter pixels around each tile
    int columns = 0;
    int rows = 0;

    // Block name -> face tile indices, ordered +X, -X, +Y, -Y, +Z, -Z.
    std::map<std::string, BlockFaceTiles> blockFaceTiles;
};

// Parses a descriptor from JSON text. On failure returns nullopt and writes a
// human-readable reason into `error`.
[[nodiscard]] std::optional<AtlasDescriptor> ParseAtlasDescriptor(std::string_view json,
                                                                  std::string& error);

}  // namespace voxelgame
