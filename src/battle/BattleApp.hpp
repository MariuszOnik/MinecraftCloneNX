#pragma once

namespace voxelgame::battle {

// Entry point for the turn-based battle mode (--battle). Owns its own window and
// render loop, separate from the open-world sandbox in src/app/main.cpp.
int RunBattle(int argc, char* argv[]);

}  // namespace voxelgame::battle
