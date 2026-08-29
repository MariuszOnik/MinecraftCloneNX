#include "world/WorldSave.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace voxelgame {
namespace {

constexpr char kMagic[4] = {'V', 'X', 'S', 'V'};
constexpr std::uint32_t kVersion = 1;

template <typename T>
void Write(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool Read(std::ifstream& in, T& value) {
    return static_cast<bool>(in.read(reinterpret_cast<char*>(&value), sizeof(T)));
}

}  // namespace

bool SaveWorld(const std::string& path, const WorldSave& save) {
    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out.write(kMagic, sizeof(kMagic));
        Write(out, kVersion);
        Write(out, save.seed);
        Write(out, save.playerX);
        Write(out, save.playerY);
        Write(out, save.playerZ);
        Write(out, save.yaw);
        Write(out, save.pitch);
        Write(out, static_cast<std::uint32_t>(save.edits.size()));
        for (const BlockEdit& edit : save.edits) {
            Write(out, static_cast<std::int32_t>(edit.x));
            Write(out, static_cast<std::int32_t>(edit.y));
            Write(out, static_cast<std::int32_t>(edit.z));
            Write(out, static_cast<std::uint16_t>(edit.block));
        }
        if (!out.good()) {
            return false;
        }
    }
    std::remove(path.c_str());
    return std::rename(temp.c_str(), path.c_str()) == 0;
}

std::optional<WorldSave> LoadWorld(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }

    char magic[4] = {};
    std::uint32_t version = 0;
    if (!in.read(magic, sizeof(magic)) || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
        return std::nullopt;
    }
    if (!Read(in, version) || version != kVersion) {
        return std::nullopt;
    }

    WorldSave save;
    std::uint32_t count = 0;
    if (!Read(in, save.seed) || !Read(in, save.playerX) || !Read(in, save.playerY) ||
        !Read(in, save.playerZ) || !Read(in, save.yaw) || !Read(in, save.pitch) ||
        !Read(in, count)) {
        return std::nullopt;
    }

    save.edits.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
        std::uint16_t block = 0;
        if (!Read(in, x) || !Read(in, y) || !Read(in, z) || !Read(in, block)) {
            return std::nullopt;
        }
        save.edits.push_back({x, y, z, block});
    }
    return save;
}

}  // namespace voxelgame
