#include "core/BuildInfo.hpp"

#include <raylib.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

bool HasArgument(const int argc, char* argv[], const char* expected) {
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], expected) == 0) {
            return true;
        }
    }
    return false;
}

void PrintBuildInfo(const voxelgame::BuildInfo& info) {
    std::printf("VoxelGame %.*s | %.*s | commit %.*s\n",
                static_cast<int>(info.version.size()), info.version.data(),
                static_cast<int>(info.platform.size()), info.platform.data(),
                static_cast<int>(info.commit.size()), info.commit.data());
}

}  // namespace

int main(int argc, char* argv[]) {
    const voxelgame::BuildInfo build = voxelgame::GetBuildInfo();
    if (!voxelgame::HasValidBuildInfo(build)) {
        std::fputs("Invalid build information\n", stderr);
        return 2;
    }

    if (HasArgument(argc, argv, "--smoke-test")) {
        PrintBuildInfo(build);
        return 0;
    }

    const bool smokeWindow = HasArgument(argc, argv, "--smoke-window");
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(960, 540, "VoxelGame - M0");
    if (!IsWindowReady()) {
        std::fputs("raylib window initialization failed\n", stderr);
        return 3;
    }

    SetTargetFPS(60);
    int renderedFrames = 0;
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(Color{18, 22, 31, 255});

        DrawText("VOXEL GAME", 48, 48, 40, Color{116, 214, 140, 255});
        DrawText("M0 / reproducible build", 50, 98, 22, RAYWHITE);

        const std::string platformLine = "Platform: " + std::string(build.platform);
        const std::string versionLine = "Version:  " + std::string(build.version);
        const std::string commitLine =
            "Commit:   " + std::string(voxelgame::ShortCommit(build.commit));
        DrawText(platformLine.c_str(), 50, 172, 24, LIGHTGRAY);
        DrawText(versionLine.c_str(), 50, 208, 24, LIGHTGRAY);
        DrawText(commitLine.c_str(), 50, 244, 24, LIGHTGRAY);
        DrawText("PC: ESC closes | Switch: + closes", 50, 444, 18, GRAY);

        EndDrawing();

        ++renderedFrames;
        if (smokeWindow && renderedFrames >= 3) {
            break;
        }
    }

    CloseWindow();
    return 0;
}

