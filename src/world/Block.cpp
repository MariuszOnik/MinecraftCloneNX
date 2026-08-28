#include "world/Block.hpp"

#include "world/BlockAtlasLayout.hpp"

#include <array>

namespace voxelgame {
namespace {

using atlas::Tile;

constexpr BlockFaceTiles AllFaces(const Tile tile) {
    const auto t = static_cast<std::uint8_t>(tile);
    return {{t, t, t, t, t, t}};
}

// Face order: +X, -X, +Y, -Y, +Z, -Z.
constexpr std::array<BlockDefinition, blocks::Count> definitions{{
    {"air", false, RenderLayer::Opaque, {0, 0, 0, 0}, AllFaces(Tile::Dirt)},
    {"grass", true, RenderLayer::Opaque, {92, 172, 88, 255},
     {{Tile::GrassSide, Tile::GrassSide, Tile::GrassTop, Tile::Dirt, Tile::GrassSide,
       Tile::GrassSide}}},
    {"dirt", true, RenderLayer::Opaque, {133, 92, 58, 255}, AllFaces(Tile::Dirt)},
    {"stone", true, RenderLayer::Opaque, {133, 139, 151, 255}, AllFaces(Tile::Stone)},
    {"cobblestone", true, RenderLayer::Opaque, {122, 122, 122, 255}, AllFaces(Tile::Cobblestone)},
    {"planks", true, RenderLayer::Opaque, {160, 128, 82, 255}, AllFaces(Tile::Planks)},
    {"wood", true, RenderLayer::Opaque, {105, 82, 52, 255},
     {{Tile::WoodSide, Tile::WoodSide, Tile::WoodTop, Tile::WoodTop, Tile::WoodSide,
       Tile::WoodSide}}},
    {"sand", true, RenderLayer::Opaque, {216, 202, 150, 255}, AllFaces(Tile::Sand)},
    {"gravel", true, RenderLayer::Opaque, {128, 122, 118, 255}, AllFaces(Tile::Gravel)},
    {"bedrock", true, RenderLayer::Opaque, {70, 70, 74, 255}, AllFaces(Tile::Bedrock)},
    {"leaves", true, RenderLayer::Cutout, {74, 130, 52, 255}, AllFaces(Tile::Leaves)},
    {"glass", true, RenderLayer::Transparent, {198, 226, 236, 128}, AllFaces(Tile::Glass)},
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
