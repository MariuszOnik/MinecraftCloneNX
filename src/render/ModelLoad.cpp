#include "render/ModelLoad.hpp"

#include "model/AnimationBinary.hpp"
#include "model/VoxelModelBinary.hpp"
#include "model/VoxelModelLoader.hpp"
#include "platform/Assets.hpp"

#include <raylib.h>

#include <fstream>
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

}  // namespace

VoxelModelRenderMesh LoadModelMesh(const AssetPaths& assets, const std::string& base) {
    VoxelModelRenderMesh mesh;
    std::optional<vmodel::VoxelModel> model;
    std::string source;

    const AssetPaths::Resolved binary = assets.Resolve(base + ".vxm");
    if (binary.found) {
        model = vmodel::ReadVoxelModelBinaryFile(binary.path);
        source = binary.path;
        if (!model) {
            TraceLog(LOG_WARNING, "VOXEL: model binary '%s' invalid", binary.path.c_str());
        }
    }
    if (!model) {
        const AssetPaths::Resolved json = assets.Resolve(base + ".vxm.json");
        source = json.path;
        if (!json.found) {
            TraceLog(LOG_WARNING, "VOXEL: model '%s' not found", base.c_str());
            return mesh;
        }
        const auto text = ReadTextFile(json.path);
        std::string error = "unreadable";
        if (text) {
            model = vmodel::ParseVoxelModel(*text, error);
        }
        if (!model) {
            TraceLog(LOG_WARNING, "VOXEL: model '%s' invalid (%s)", json.path.c_str(), error.c_str());
            return mesh;
        }
    }

    if (!mesh.Upload(*model)) {
        TraceLog(LOG_WARNING, "VOXEL: model '%s' failed to upload", source.c_str());
        return mesh;
    }
    TraceLog(LOG_INFO, "VOXEL: loaded model '%s' (%zu parts)", source.c_str(), mesh.PartCount());
    return mesh;
}

std::optional<vmodel::AnimationClip> LoadAnimationClip(const AssetPaths& assets,
                                                      const std::string& base) {
    const AssetPaths::Resolved binary = assets.Resolve(base + ".vxa");
    if (binary.found) {
        if (auto clip = vmodel::ReadAnimationBinaryFile(binary.path)) {
            TraceLog(LOG_INFO, "VOXEL: loaded animation '%s' (%zu tracks)", binary.path.c_str(),
                     clip->tracks.size());
            return clip;
        }
        TraceLog(LOG_WARNING, "VOXEL: animation binary '%s' invalid", binary.path.c_str());
    }

    const AssetPaths::Resolved json = assets.Resolve(base + ".vxa.json");
    if (!json.found) {
        TraceLog(LOG_WARNING, "VOXEL: animation '%s' not found", base.c_str());
        return std::nullopt;
    }
    const auto text = ReadTextFile(json.path);
    std::string error = "unreadable";
    std::optional<vmodel::AnimationClip> clip;
    if (text) {
        clip = vmodel::ParseAnimationClip(*text, error);
    }
    if (!clip) {
        TraceLog(LOG_WARNING, "VOXEL: animation '%s' invalid (%s)", json.path.c_str(), error.c_str());
        return std::nullopt;
    }
    TraceLog(LOG_INFO, "VOXEL: loaded animation '%s' (%zu tracks, %.2fs)", json.path.c_str(),
             clip->tracks.size(), static_cast<double>(clip->duration));
    return clip;
}

}  // namespace voxelgame
