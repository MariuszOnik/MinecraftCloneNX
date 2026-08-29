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

using S = BlockShape;

// Fields: name, solid (meshed), collidable, layer, shape, colour, face tiles.
// Face order: +X, -X, +Y, -Y, +Z, -Z.
constexpr std::array<BlockDefinition, blocks::Count> definitions{{
    {"air", false, false, RenderLayer::Opaque, S::Cube, {0, 0, 0, 0}, AllFaces(Tile::Dirt)},
    {"grass", true, true, RenderLayer::Opaque, S::Cube, {92, 172, 88, 255},
     {{Tile::GrassSide, Tile::GrassSide, Tile::GrassTop, Tile::Dirt, Tile::GrassSide,
       Tile::GrassSide}}},
    {"dirt", true, true, RenderLayer::Opaque, S::Cube, {133, 92, 58, 255}, AllFaces(Tile::Dirt)},
    {"stone", true, true, RenderLayer::Opaque, S::Cube, {133, 139, 151, 255},
     AllFaces(Tile::Stone)},
    {"cobblestone", true, true, RenderLayer::Opaque, S::Cube, {122, 122, 122, 255},
     AllFaces(Tile::Cobblestone)},
    {"planks", true, true, RenderLayer::Opaque, S::Cube, {160, 128, 82, 255},
     AllFaces(Tile::Planks)},
    {"wood", true, true, RenderLayer::Opaque, S::Cube, {105, 82, 52, 255},
     {{Tile::WoodSide, Tile::WoodSide, Tile::WoodTop, Tile::WoodTop, Tile::WoodSide,
       Tile::WoodSide}}},
    {"sand", true, true, RenderLayer::Opaque, S::Cube, {216, 202, 150, 255}, AllFaces(Tile::Sand)},
    {"gravel", true, true, RenderLayer::Opaque, S::Cube, {128, 122, 118, 255},
     AllFaces(Tile::Gravel)},
    {"bedrock", true, true, RenderLayer::Opaque, S::Cube, {70, 70, 74, 255},
     AllFaces(Tile::Bedrock)},
    {"leaves", true, true, RenderLayer::Cutout, S::Cube, {74, 130, 52, 255},
     AllFaces(Tile::Leaves)},
    {"glass", true, true, RenderLayer::Transparent, S::Cube, {198, 226, 236, 128},
     AllFaces(Tile::Glass)},
    {"water", true, false, RenderLayer::Transparent, S::Cube, {60, 110, 190, 150},
     AllFaces(Tile::Water)},
    {"glass_pane", true, true, RenderLayer::Transparent, S::Pane, {198, 226, 236, 128},
     AllFaces(Tile::Glass)},
}};

constexpr BlockDefinition unknownDefinition{
    "unknown", true, true, RenderLayer::Opaque, S::Cube, {255, 0, 255, 255},
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

bool IsCollidableBlock(const BlockId block) noexcept {
    const BlockDefinition& definition = GetBlockDefinition(block);
    return definition.solid && definition.collidable;
}

bool IsOccludingBlock(const BlockId block) noexcept {
    const BlockDefinition& definition = GetBlockDefinition(block);
    return definition.solid && definition.layer == RenderLayer::Opaque &&
           definition.shape == BlockShape::Cube;
}

bool IsCubeShaped(const BlockId block) noexcept {
    return GetBlockDefinition(block).shape == BlockShape::Cube;
}

RenderLayer BlockRenderLayer(const BlockId block) noexcept {
    return GetBlockDefinition(block).layer;
}

}  // namespace voxelgame
