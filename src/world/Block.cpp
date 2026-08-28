#include "world/Block.hpp"

#include "world/BlockAtlasLayout.hpp"

#include <array>

namespace voxelgame {
namespace {

using atlas::Tile;

// Face order: +X, -X, +Y, -Y, +Z, -Z.
constexpr std::array<BlockDefinition, blocks::Count> definitions{{
    {"air", false, RenderLayer::Opaque, {0, 0, 0, 0},
     {{Tile::Dirt, Tile::Dirt, Tile::Dirt, Tile::Dirt, Tile::Dirt, Tile::Dirt}}},
    {"grass", true, RenderLayer::Opaque, {92, 172, 88, 255},
     {{Tile::GrassSide, Tile::GrassSide, Tile::GrassTop, Tile::Dirt, Tile::GrassSide,
       Tile::GrassSide}}},
    {"dirt", true, RenderLayer::Opaque, {133, 92, 58, 255},
     {{Tile::Dirt, Tile::Dirt, Tile::Dirt, Tile::Dirt, Tile::Dirt, Tile::Dirt}}},
    {"stone", true, RenderLayer::Opaque, {133, 139, 151, 255},
     {{Tile::Stone, Tile::Stone, Tile::Stone, Tile::Stone, Tile::Stone, Tile::Stone}}},
}};

constexpr BlockDefinition unknownDefinition{
    "unknown", true, RenderLayer::Opaque, {255, 0, 255, 255},
    {{Tile::Stone, Tile::Stone, Tile::Stone, Tile::Stone, Tile::Stone, Tile::Stone}}};

}  // namespace

const BlockDefinition& GetBlockDefinition(const BlockId block) noexcept {
    return block < definitions.size() ? definitions[block] : unknownDefinition;
}

std::uint8_t GetBlockFaceTile(const BlockId block, const int faceIndex) noexcept {
    const BlockFaceTiles& tiles = GetBlockDefinition(block).faceTiles;
    if (faceIndex < 0 || faceIndex >= static_cast<int>(tiles.size())) {
        return atlas::Tile::Stone;
    }
    return tiles[static_cast<std::size_t>(faceIndex)];
}

bool IsKnownBlock(const BlockId block) noexcept {
    return block < definitions.size();
}

bool IsSolidBlock(const BlockId block) noexcept {
    return GetBlockDefinition(block).solid;
}

bool IsOccludingBlock(const BlockId block) noexcept {
    const BlockDefinition& definition = GetBlockDefinition(block);
    return definition.solid && definition.layer == RenderLayer::Opaque;
}

}  // namespace voxelgame
