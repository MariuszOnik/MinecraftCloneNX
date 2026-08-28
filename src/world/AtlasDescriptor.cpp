#include "world/AtlasDescriptor.hpp"

#include "world/BlockAtlasLayout.hpp"

#include <nlohmann/json.hpp>

#include <array>

namespace voxelgame {
namespace {

using nlohmann::json;

// Face slots in a "blocks" object entry map to these mesher face indices.
constexpr std::array<int, 1> kTopFaces{2};
constexpr std::array<int, 1> kBottomFaces{3};
constexpr std::array<int, 4> kSideFaces{0, 1, 4, 5};

std::optional<std::uint8_t> TileIndex(const json& tiles, const std::string& name, int columns,
                                      std::string& error) {
    const auto it = tiles.find(name);
    if (it == tiles.end() || !it->is_array() || it->size() != 2) {
        error = "tile '" + name + "' is missing or not a [column, row] pair";
        return std::nullopt;
    }
    const int column = it->at(0).get<int>();
    const int row = it->at(1).get<int>();
    if (column < 0 || row < 0 || column >= columns) {
        error = "tile '" + name + "' is outside the atlas grid";
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(row * columns + column);
}

bool ApplyFaces(BlockFaceTiles& faces, const json& blockValue, const json& tiles, int columns,
                std::string& error) {
    if (blockValue.is_string()) {
        const auto tile = TileIndex(tiles, blockValue.get<std::string>(), columns, error);
        if (!tile) {
            return false;
        }
        faces.fill(*tile);
        return true;
    }
    if (!blockValue.is_object()) {
        error = "block entry must be a tile name or an object";
        return false;
    }

    const auto assign = [&](const char* key, const auto& slots) -> bool {
        const auto it = blockValue.find(key);
        if (it == blockValue.end()) {
            return true;  // slot not overridden
        }
        const auto tile = TileIndex(tiles, it->get<std::string>(), columns, error);
        if (!tile) {
            return false;
        }
        for (const int face : slots) {
            faces[static_cast<std::size_t>(face)] = *tile;
        }
        return true;
    };

    return assign("sides", kSideFaces) && assign("top", kTopFaces) &&
           assign("bottom", kBottomFaces);
}

}  // namespace

std::optional<AtlasDescriptor> ParseAtlasDescriptor(const std::string_view jsonText,
                                                    std::string& error) {
    json root = json::parse(jsonText.begin(), jsonText.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        error = "descriptor is not valid JSON";
        return std::nullopt;
    }

    AtlasDescriptor descriptor;
    descriptor.texture = root.value("texture", std::string{});
    if (descriptor.texture.empty()) {
        error = "descriptor is missing \"texture\"";
        return std::nullopt;
    }

    const auto size = root.find("atlasSize");
    if (size == root.end() || !size->is_array() || size->size() != 2) {
        error = "descriptor is missing a [width, height] \"atlasSize\"";
        return std::nullopt;
    }
    descriptor.atlasWidth = size->at(0).get<int>();
    descriptor.atlasHeight = size->at(1).get<int>();
    descriptor.tileSize = root.value("tileSize", 0);
    if (descriptor.tileSize <= 0 || descriptor.atlasWidth % descriptor.tileSize != 0 ||
        descriptor.atlasHeight % descriptor.tileSize != 0) {
        error = "\"tileSize\" must be positive and divide the atlas size";
        return std::nullopt;
    }
    descriptor.columns = descriptor.atlasWidth / descriptor.tileSize;
    descriptor.rows = descriptor.atlasHeight / descriptor.tileSize;

    const auto tiles = root.find("tiles");
    const auto blocksNode = root.find("blocks");
    if (tiles == root.end() || !tiles->is_object() || blocksNode == root.end() ||
        !blocksNode->is_object()) {
        error = "descriptor needs \"tiles\" and \"blocks\" objects";
        return std::nullopt;
    }

    for (const auto& [name, value] : blocksNode->items()) {
        BlockFaceTiles faces{};
        faces.fill(0);
        if (!ApplyFaces(faces, value, *tiles, descriptor.columns, error)) {
            error = "block \"" + name + "\": " + error;
            return std::nullopt;
        }
        descriptor.blockFaceTiles.emplace(name, faces);
    }

    return descriptor;
}

}  // namespace voxelgame
