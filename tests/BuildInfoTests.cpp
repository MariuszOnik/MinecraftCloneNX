#include "core/BuildInfo.hpp"

#include <iostream>

int main() {
    const voxelgame::BuildInfo info = voxelgame::GetBuildInfo();
    if (!voxelgame::HasValidBuildInfo(info)) {
        std::cerr << "Build info contains an empty required field\n";
        return 1;
    }
    if (voxelgame::ShortCommit(info.commit).size() > 12) {
        std::cerr << "Short commit exceeds 12 characters\n";
        return 2;
    }
    if (info.platform.find("PC") == std::string_view::npos) {
        std::cerr << "Native tests must identify a PC platform\n";
        return 3;
    }

    std::cout << "Build info OK: " << info.platform << " " << info.commit << '\n';
    return 0;
}

