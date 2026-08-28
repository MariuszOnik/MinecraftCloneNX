#pragma once

#include <raylib.h>

namespace voxelgame {

// Shader used for greedy-meshed chunks: a merged quad carries tile-unit UVs
// (0..w, 0..h), and the fragment stage repeats the tile via
// atlasUV = tileOrigin + fract(uv) * tileExtent. The caller owns the returned
// shader and must UnloadShader it.
[[nodiscard]] Shader LoadTilingShader();

// Sets the constant tile content size in normalised atlas coordinates. Call once
// after LoadTilingShader (and again only if the atlas changes).
void SetTilingShaderExtent(Shader shader, float extentU, float extentV);

}  // namespace voxelgame
