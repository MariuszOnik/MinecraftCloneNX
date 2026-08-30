// voxelgame_assetc -- compiles editor-facing JSON assets into the game's
// versioned binary runtime formats (PLAN.md 6.3):
//
//   *.vxm.json  ->  *.vxm   (voxel model)
//   *.vxa.json  ->  *.vxa   (animation clip)
//
// Usage:
//   voxelgame_assetc <input.(vxm|vxa).json> [output]   compile one file
//   voxelgame_assetc --dir <directory>                 compile every asset under it
//
// The game still loads the JSON if the binary is absent, so this step is a
// build-time optimisation, not a hard requirement.

#include "model/Animation.hpp"
#include "model/AnimationBinary.hpp"
#include "model/VoxelModelBinary.hpp"
#include "model/VoxelModelLoader.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace {

std::optional<std::string> ReadFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

// "chicken.vxm.json" -> kind "vxm"; anything else -> empty.
std::string AssetKind(const fs::path& path) {
    const std::string name = path.filename().string();
    if (name.size() > 9 && name.substr(name.size() - 9) == ".vxm.json") {
        return "vxm";
    }
    if (name.size() > 9 && name.substr(name.size() - 9) == ".vxa.json") {
        return "vxa";
    }
    return {};
}

fs::path DefaultOutput(const fs::path& input) {
    fs::path out = input;
    out.replace_extension();  // "chicken.vxm.json" -> "chicken.vxm"
    return out;
}

bool CompileOne(const fs::path& input, const fs::path& output) {
    const std::string kind = AssetKind(input);
    if (kind.empty()) {
        std::cerr << "skip (not a .vxm.json / .vxa.json): " << input << '\n';
        return false;
    }
    const auto text = ReadFile(input);
    if (!text) {
        std::cerr << "error: cannot read " << input << '\n';
        return false;
    }

    std::string parseError = "unknown";
    bool ok = false;
    if (kind == "vxm") {
        const auto model = voxelgame::vmodel::ParseVoxelModel(*text, parseError);
        ok = model && voxelgame::vmodel::WriteVoxelModelBinaryFile(output.string(), *model);
    } else {
        const auto clip = voxelgame::vmodel::ParseAnimationClip(*text, parseError);
        ok = clip && voxelgame::vmodel::WriteAnimationBinaryFile(output.string(), *clip);
    }
    if (!ok) {
        std::cerr << "error: " << input << ": " << parseError << '\n';
        return false;
    }
    std::cout << input.filename().string() << " -> " << output.filename().string() << '\n';
    return true;
}

int CompileDir(const fs::path& dir) {
    if (!fs::is_directory(dir)) {
        std::cerr << "error: not a directory: " << dir << '\n';
        return 1;
    }
    int failures = 0;
    int compiled = 0;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file() || AssetKind(entry.path()).empty()) {
            continue;
        }
        if (CompileOne(entry.path(), DefaultOutput(entry.path()))) {
            ++compiled;
        } else {
            ++failures;
        }
    }
    std::cout << compiled << " compiled, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 3 && std::string(argv[1]) == "--dir") {
        return CompileDir(argv[2]);
    }
    if (argc == 2 || argc == 3) {
        const fs::path input = argv[1];
        const fs::path output = argc == 3 ? fs::path(argv[2]) : DefaultOutput(input);
        return CompileOne(input, output) ? 0 : 1;
    }
    std::cerr << "usage: voxelgame_assetc <input.(vxm|vxa).json> [output]\n"
              << "       voxelgame_assetc --dir <directory>\n";
    return 2;
}
