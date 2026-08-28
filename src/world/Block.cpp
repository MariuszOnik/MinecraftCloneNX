#include "world/Block.hpp"

#include <array>

namespace voxelgame {
namespace {

constexpr std::array<BlockDefinition, blocks::Count> definitions{{
    {"air", false, RenderLayer::Opaque, {0, 0, 0, 0}},
    {"grass", true, RenderLayer::Opaque, {92, 172, 88, 255}},
    {"dirt", true, RenderLayer::Opaque, {133, 92, 58, 255}},
    {"stone", true, RenderLayer::Opaque, {133, 139, 151, 255}},
}};

constexpr BlockDefinition unknownDefinition{
    "unknown", true, RenderLayer::Opaque, {255, 0, 255, 255}};

}  // namespace

const BlockDefinition& GetBlockDefinition(const BlockId block) noexcept {
    return block < definitions.size() ? definitions[block] : unknownDefinition;
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
