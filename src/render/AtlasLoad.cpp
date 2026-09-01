#include "render/AtlasLoad.hpp"

#include "platform/Assets.hpp"
#include "render/BlockAtlas.hpp"
#include "world/AtlasDescriptor.hpp"

#include <fstream>
#include <optional>
#include <sstream>

namespace voxelgame {
namespace {

std::optional<std::string> ReadTextFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

const char* SourceLabel(const AssetPaths::Origin origin, const bool procedural) {
    if (procedural) {
        return "procedural";
    }
    switch (origin) {
        case AssetPaths::Origin::SdCard:
            return "SD";
        case AssetPaths::Origin::Bundle:
            return "bundled";
        default:
            return "assets";
    }
}

}  // namespace

LoadedAtlas LoadBlockAtlas(const AssetPaths& assets, const std::string& base) {
    LoadedAtlas out;

    const std::string dir = base.substr(0, base.find_last_of('/') + 1);
    std::string textureRelative = base + ".png";

    const AssetPaths::Resolved descriptor = assets.Resolve(base + ".json");
    if (descriptor.found) {
        std::string error = "unreadable";
        std::optional<AtlasDescriptor> parsed;
        if (const auto text = ReadTextFile(descriptor.path)) {
            parsed = ParseAtlasDescriptor(*text, error);
        }
        if (parsed) {
            out.binding.Apply(*parsed);
            textureRelative = dir + parsed->texture;
            TraceLog(LOG_INFO, "VOXEL: atlas descriptor '%s' -> %s (%dx%d, %d px tiles)",
                     descriptor.path.c_str(), parsed->texture.c_str(), parsed->atlasWidth,
                     parsed->atlasHeight, parsed->tileSize);
        } else {
            TraceLog(LOG_WARNING, "VOXEL: atlas descriptor '%s' invalid (%s); using defaults",
                     descriptor.path.c_str(), error.c_str());
        }
    }

    const AssetPaths::Resolved image = assets.Resolve(textureRelative);
    Texture2D atlas{};
    if (image.found) {
        Image pixels = LoadImage(image.path.c_str());
        atlas = LoadTextureFromImage(pixels);
        UnloadImage(pixels);
    }
    if (atlas.id != 0) {
        out.sourceLabel = SourceLabel(image.origin, false);
        TraceLog(LOG_INFO, "VOXEL: loaded block atlas from '%s'", image.path.c_str());
    } else {
        out.sourceLabel = SourceLabel(image.origin, true);
        TraceLog(LOG_WARNING, "VOXEL: block atlas '%s' unavailable, using procedural fallback",
                 image.path.c_str());
        Image pixels = GenerateBlockAtlasImage();
        atlas = LoadTextureFromImage(pixels);
        UnloadImage(pixels);
        out.binding = BlockAtlasBinding{};  // procedural atlas uses the default grid
    }
    SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
    out.texture = atlas;
    return out;
}

}  // namespace voxelgame
