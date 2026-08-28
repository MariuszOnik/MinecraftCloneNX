#pragma once

#include <cstdint>

namespace voxelgame::atlas {

// The block atlas is a regular grid of square tiles, indexed left-to-right then
// top-to-bottom. Each tile is surrounded by a `Padding`-pixel gutter (its edge
// pixels extruded) so the greedy mesher can tile a tile across a merged quad
// without the sampler bleeding into a neighbour. Keeping the layout here lets the
// CPU mesher emit UVs without depending on raylib while the texture generator in
// the render layer fills the matching pixels.
inline constexpr int TileSize = 16;
inline constexpr int Padding = 1;
inline constexpr int CellStride = TileSize + 2 * Padding;
inline constexpr int Columns = 4;
inline constexpr int Rows = 4;
inline constexpr int TileCount = Columns * Rows;
inline constexpr int Width = CellStride * Columns;
inline constexpr int Height = CellStride * Rows;

enum Tile : std::uint8_t {
    GrassTop = 0,
    GrassSide = 1,
    Dirt = 2,
    Stone = 3,
    Cobblestone = 4,
    Planks = 5,
    WoodSide = 6,
    WoodTop = 7,
    Sand = 8,
    Gravel = 9,
    Bedrock = 10,
    Leaves = 11,
    Glass = 12,
};

// Normalised content rectangle of a tile within the atlas (inside its gutter).
struct TileRect {
    float u0;
    float v0;
    float u1;
    float v1;
};

// Content rect of `tile` in an atlas of `columns` x `rows` tiles, `tileSize` px
// each with a `padding` px gutter, in a `atlasWidth` x `atlasHeight` px image.
[[nodiscard]] constexpr TileRect TileRectOf(const int tile, const int columns, const int rows,
                                            const int atlasWidth, const int atlasHeight,
                                            const int tileSize, const int padding) noexcept {
    const int count = columns * rows;
    const int clamped = (tile < 0 || tile >= count) ? 0 : tile;
    const int column = clamped % columns;
    const int row = clamped / columns;
    const int stride = tileSize + 2 * padding;
    const float x0 = static_cast<float>(column * stride + padding);
    const float y0 = static_cast<float>(row * stride + padding);
    return TileRect{
        x0 / static_cast<float>(atlasWidth),
        y0 / static_cast<float>(atlasHeight),
        (x0 + static_cast<float>(tileSize)) / static_cast<float>(atlasWidth),
        (y0 + static_cast<float>(tileSize)) / static_cast<float>(atlasHeight),
    };
}

// Convenience overload for the compiled default grid.
[[nodiscard]] constexpr TileRect TileRectOf(const int tile) noexcept {
    return TileRectOf(tile, Columns, Rows, Width, Height, TileSize, Padding);
}

}  // namespace voxelgame::atlas
