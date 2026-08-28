#pragma once

#include <cstdint>
#include <string_view>

namespace voxelgame {

using BlockId = std::uint16_t;

enum class RenderLayer : std::uint8_t {
    Opaque,
    Cutout,
    Transparent,
};

struct BlockColor {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha;
};

struct BlockDefinition {
    std::string_view name;
    bool solid;
    RenderLayer layer;
    BlockColor color;
};

namespace blocks {
inline constexpr BlockId Air = 0;
inline constexpr BlockId Grass = 1;
inline constexpr BlockId Dirt = 2;
inline constexpr BlockId Stone = 3;
inline constexpr BlockId Count = 4;
}  // namespace blocks

[[nodiscard]] const BlockDefinition& GetBlockDefinition(BlockId block) noexcept;
[[nodiscard]] bool IsKnownBlock(BlockId block) noexcept;
[[nodiscard]] bool IsSolidBlock(BlockId block) noexcept;
[[nodiscard]] bool IsOccludingBlock(BlockId block) noexcept;

}  // namespace voxelgame
