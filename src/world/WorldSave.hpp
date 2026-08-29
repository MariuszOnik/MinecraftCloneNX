#pragma once

#include "world/Block.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace voxelgame {

struct BlockEdit {
    int x = 0;
    int y = 0;
    int z = 0;
    BlockId block = 0;
};

// One save slot: the world seed, where the player was, and every block the
// player changed relative to the generator.
struct WorldSave {
    std::uint32_t seed = 0;
    float playerX = 0.0F;
    float playerY = 0.0F;
    float playerZ = 0.0F;
    float yaw = 0.0F;
    float pitch = 0.0F;
    std::vector<BlockEdit> edits;
};

// Writes the save to `path` (temp file + rename, so a crash mid-write cannot
// corrupt an existing save). Returns false on any I/O error.
bool SaveWorld(const std::string& path, const WorldSave& save);

// Reads the save at `path`. nullopt if the file is missing or not a valid save.
[[nodiscard]] std::optional<WorldSave> LoadWorld(const std::string& path);

}  // namespace voxelgame
