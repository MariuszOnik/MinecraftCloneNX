#pragma once

#include <raylib.h>

namespace voxelgame {

// Builds the procedural block texture atlas: a horizontal strip of 16x16 tiles
// (grass top, grass side, dirt, stone). Deterministic, so PC and Switch render
// identical pixels. The caller owns the returned image and must UnloadImage it.
[[nodiscard]] Image GenerateBlockAtlasImage();

}  // namespace voxelgame
