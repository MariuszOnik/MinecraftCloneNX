#pragma once

#include <cstdint>

namespace voxelgame::atlas {

// The block atlas is a regular grid of square tiles, indexed left-to-right then
// top-to-bottom. Keeping the layout here lets the CPU mesher emit UVs without
// depending on raylib while the texture generator in the render layer fills the
// matching pixels. To adopt a different atlas, change these constants, remap the
// Tile values / per-block face tiles, and load a texture instead of generating
// one -- nothing else in the pipeline assumes a specific tile set.
inline constexpr int TileSize = 16;
inline constexpr int Columns = 4;
inline constexpr int Rows = 1;
inline constexpr int TileCount = Columns * Rows;
inline constexpr int Width = TileSize * Columns;
inline constexpr int Height = TileSize * Rows;

enum Tile : std::uint8_t {
    GrassTop = 0,
    GrassSide = 1,
    Dirt = 2,
    Stone = 3,
};

// Normalised UV rectangle of a tile within the atlas, inset by half a texel so
// nearest-filtered sampling never bleeds into a neighbouring tile.
struct TileRect {
    float u0;
    float v0;
    float u1;
    float v1;
};

// UV rect of `tile` in an atlas of `columns` x `rows` square tiles. `atlasWidth`
// / `atlasHeight` are the pixel dimensions, used only for the half-texel inset.
[[nodiscard]] constexpr TileRect TileRectOf(const int tile, const int columns, const int rows,
                                            const int atlasWidth,
                                            const int atlasHeight) noexcept {
    const int count = columns * rows;
    const int clamped = (tile < 0 || tile >= count) ? 0 : tile;
    const int column = clamped % columns;
    const int row = clamped / columns;
    const float insetU = 0.5F / static_cast<float>(atlasWidth);
    const float insetV = 0.5F / static_cast<float>(atlasHeight);
    const float tileU = 1.0F / static_cast<float>(columns);
    const float tileV = 1.0F / static_cast<float>(rows);
    return TileRect{
        static_cast<float>(column) * tileU + insetU,
        static_cast<float>(row) * tileV + insetV,
        static_cast<float>(column + 1) * tileU - insetU,
        static_cast<float>(row + 1) * tileV - insetV,
    };
}

// Convenience overload for the compiled default grid.
[[nodiscard]] constexpr TileRect TileRectOf(const int tile) noexcept {
    return TileRectOf(tile, Columns, Rows, Width, Height);
}

}  // namespace voxelgame::atlas
