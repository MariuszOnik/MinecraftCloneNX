#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace voxelgame {

using BlockId = std::uint16_t;

enum class RenderLayer : std::uint8_t {
    Opaque,
    Cutout,
    Transparent,
};

enum class BlockShape : std::uint8_t {
    Cube,  // full 1x1x1 cube, greedy-meshed
    Pane,  // thin cross in the block centre (glass pane)
};

struct BlockColor {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha;
};

// Atlas tile index per cube face, ordered to match the mesher's face table:
// +X, -X, +Y, -Y, +Z, -Z.
using BlockFaceTiles = std::array<std::uint8_t, 6>;

struct BlockDefinition {
    std::string_view name;
    bool solid;       // gets meshed (everything except air)
    bool collidable;  // stops the player and blocks a raycast (false for water)
    RenderLayer layer;
    BlockShape shape;
    BlockColor color;
    BlockFaceTiles faceTiles;
};

namespace blocks {
inline constexpr BlockId Air = 0;
inline constexpr BlockId Grass = 1;
inline constexpr BlockId Dirt = 2;
inline constexpr BlockId Stone = 3;
inline constexpr BlockId Cobblestone = 4;
inline constexpr BlockId Planks = 5;
inline constexpr BlockId Wood = 6;
inline constexpr BlockId Sand = 7;
inline constexpr BlockId Gravel = 8;
inline constexpr BlockId Bedrock = 9;
inline constexpr BlockId Leaves = 10;
inline constexpr BlockId Glass = 11;
inline constexpr BlockId Water = 12;
inline constexpr BlockId GlassPane = 13;
inline constexpr BlockId Count = 14;
}  // namespace blocks

[[nodiscard]] const BlockDefinition& GetBlockDefinition(BlockId block) noexcept;
[[nodiscard]] std::uint8_t GetBlockFaceTile(BlockId block, int faceIndex) noexcept;
[[nodiscard]] bool IsKnownBlock(BlockId block) noexcept;
[[nodiscard]] bool IsSolidBlock(BlockId block) noexcept;
[[nodiscard]] bool IsCollidableBlock(BlockId block) noexcept;
[[nodiscard]] bool IsOccludingBlock(BlockId block) noexcept;
[[nodiscard]] bool IsCubeShaped(BlockId block) noexcept;
[[nodiscard]] RenderLayer BlockRenderLayer(BlockId block) noexcept;

}  // namespace voxelgame
