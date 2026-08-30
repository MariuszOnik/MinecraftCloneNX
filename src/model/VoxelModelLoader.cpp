#include "model/VoxelModelLoader.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace voxelgame::vmodel {
namespace {

using nlohmann::json;

bool ReadVec3(const json& node, const char* key, Vec3& out, std::string& error) {
    const auto it = node.find(key);
    if (it == node.end()) {
        return true;  // optional, keep the default
    }
    if (!it->is_array() || it->size() != 3) {
        error = std::string(key) + " must be a [x, y, z] array";
        return false;
    }
    out = {it->at(0).get<float>(), it->at(1).get<float>(), it->at(2).get<float>()};
    return true;
}

bool ParsePalette(const json& root, VoxelModel& model, std::string& error) {
    const auto palette = root.find("palette");
    if (palette == root.end() || !palette->is_array() || palette->empty()) {
        error = "model needs a non-empty \"palette\" array";
        return false;
    }
    for (const auto& entry : *palette) {
        const auto color = entry.find("color");
        if (color == entry.end() || !color->is_array() ||
            (color->size() != 3 && color->size() != 4)) {
            error = "each palette entry needs a [r, g, b] or [r, g, b, a] \"color\"";
            return false;
        }
        ModelMaterial material;
        material.name = entry.value("name", std::string{});
        material.red = color->at(0).get<std::uint8_t>();
        material.green = color->at(1).get<std::uint8_t>();
        material.blue = color->at(2).get<std::uint8_t>();
        material.alpha = color->size() == 4 ? color->at(3).get<std::uint8_t>() : std::uint8_t{255};
        model.palette.push_back(std::move(material));
    }
    return true;
}

// Fills grid.voxels from either "fill" (a solid box of one palette index) or
// "voxels" (a flat array, x fastest then z then y).
bool ParseVoxels(const json& partNode, VoxelGrid& grid, std::size_t paletteSize,
                 const std::string& partName, std::string& error) {
    const std::size_t volume = grid.Volume();
    const auto fill = partNode.find("fill");
    const auto voxels = partNode.find("voxels");
    if ((fill == partNode.end()) == (voxels == partNode.end())) {
        error = "part \"" + partName + "\" needs exactly one of \"fill\" or \"voxels\"";
        return false;
    }

    grid.voxels.assign(volume, 0);
    if (fill != partNode.end()) {
        const auto index = fill->get<int>();
        if (index < 1 || static_cast<std::size_t>(index) > paletteSize) {
            error = "part \"" + partName + "\": \"fill\" is outside the palette";
            return false;
        }
        std::fill(grid.voxels.begin(), grid.voxels.end(), static_cast<std::uint8_t>(index));
        return true;
    }

    if (!voxels->is_array() || voxels->size() != volume) {
        error = "part \"" + partName + "\": \"voxels\" length must equal size x*y*z (" +
                std::to_string(volume) + ")";
        return false;
    }
    for (std::size_t i = 0; i < volume; ++i) {
        const auto index = voxels->at(i).get<int>();
        if (index < 0 || static_cast<std::size_t>(index) > paletteSize) {
            error = "part \"" + partName + "\": voxel " + std::to_string(i) +
                    " is outside the palette";
            return false;
        }
        grid.voxels[i] = static_cast<std::uint8_t>(index);
    }
    return true;
}

bool ParseParts(const json& root, VoxelModel& model, std::string& error) {
    const auto parts = root.find("parts");
    if (parts == root.end() || !parts->is_array() || parts->empty()) {
        error = "model needs a non-empty \"parts\" array";
        return false;
    }

    for (const auto& node : *parts) {
        VoxelModelPart part;
        part.name = node.value("name", std::string{});
        if (part.name.empty()) {
            error = "every part needs a \"name\"";
            return false;
        }

        const auto parent = node.find("parent");
        if (parent != node.end() && !parent->is_null()) {
            const auto parentName = parent->get<std::string>();
            int found = -1;
            for (std::size_t i = 0; i < model.parts.size(); ++i) {
                if (model.parts[i].name == parentName) {
                    found = static_cast<int>(i);
                    break;
                }
            }
            if (found < 0) {
                error = "part \"" + part.name + "\": parent \"" + parentName +
                        "\" is not an earlier part";
                return false;
            }
            part.parent = found;
        }

        const auto size = node.find("size");
        if (size == node.end() || !size->is_array() || size->size() != 3) {
            error = "part \"" + part.name + "\" needs a [x, y, z] \"size\"";
            return false;
        }
        part.grid.sizeX = size->at(0).get<int>();
        part.grid.sizeY = size->at(1).get<int>();
        part.grid.sizeZ = size->at(2).get<int>();
        if (part.grid.sizeX <= 0 || part.grid.sizeY <= 0 || part.grid.sizeZ <= 0) {
            error = "part \"" + part.name + "\": every \"size\" component must be positive";
            return false;
        }

        if (!ReadVec3(node, "position", part.position, error) ||
            !ReadVec3(node, "pivot", part.pivot, error) ||
            !ReadVec3(node, "rotation", part.rotationDegrees, error)) {
            error = "part \"" + part.name + "\": " + error;
            return false;
        }

        if (!ParseVoxels(node, part.grid, model.palette.size(), part.name, error)) {
            return false;
        }
        model.parts.push_back(std::move(part));
    }
    return true;
}

}  // namespace

std::optional<VoxelModel> ParseVoxelModel(const std::string_view jsonText, std::string& error) {
    json root = json::parse(jsonText.begin(), jsonText.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        error = "model is not valid JSON";
        return std::nullopt;
    }

    VoxelModel model;
    model.name = root.value("name", std::string{});
    model.voxelSize = root.value("voxelSize", 1.0F / 16.0F);
    if (model.voxelSize <= 0.0F) {
        error = "\"voxelSize\" must be positive";
        return std::nullopt;
    }

    if (!ParsePalette(root, model, error) || !ParseParts(root, model, error)) {
        return std::nullopt;
    }
    return model;
}

}  // namespace voxelgame::vmodel
