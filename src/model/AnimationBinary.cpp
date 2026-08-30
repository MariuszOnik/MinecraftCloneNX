#include "model/AnimationBinary.hpp"

#include "model/BinaryIO.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <utility>

namespace voxelgame::vmodel {
namespace {

using namespace bin;

void WriteKeys(std::ostream& out, const std::vector<Keyframe>& keys) {
    WritePod(out, static_cast<std::uint32_t>(keys.size()));
    for (const Keyframe& key : keys) {
        WritePod(out, key.time);
        WritePod(out, key.value.x);
        WritePod(out, key.value.y);
        WritePod(out, key.value.z);
    }
}

bool ReadKeys(std::istream& in, std::vector<Keyframe>& keys) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count) || count > kMaxCount) {
        return false;
    }
    keys.resize(count);
    for (Keyframe& key : keys) {
        if (!ReadPod(in, key.time) || !ReadPod(in, key.value.x) || !ReadPod(in, key.value.y) ||
            !ReadPod(in, key.value.z)) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool WriteAnimationBinary(std::ostream& out, const AnimationClip& clip) {
    out.write(kAnimationMagic, sizeof(kAnimationMagic));
    WritePod(out, kAnimationVersion);
    WriteString(out, clip.name);
    WritePod(out, clip.duration);
    WritePod(out, static_cast<std::uint8_t>(clip.loop ? 1 : 0));

    WritePod(out, static_cast<std::uint32_t>(clip.tracks.size()));
    for (const AnimationTrack& track : clip.tracks) {
        WriteString(out, track.part);
        WriteKeys(out, track.rotation);
        WriteKeys(out, track.position);
    }

    WritePod(out, static_cast<std::uint32_t>(clip.events.size()));
    for (const AnimationEvent& event : clip.events) {
        WritePod(out, event.time);
        WriteString(out, event.name);
    }
    return out.good();
}

bool WriteAnimationBinaryFile(const std::string& path, const AnimationClip& clip) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    return out && WriteAnimationBinary(out, clip);
}

std::optional<AnimationClip> ReadAnimationBinary(std::istream& in) {
    char magic[4] = {};
    std::uint32_t version = 0;
    if (!in.read(magic, sizeof(magic)) ||
        std::memcmp(magic, kAnimationMagic, sizeof(magic)) != 0) {
        return std::nullopt;
    }
    if (!ReadPod(in, version) || version != kAnimationVersion) {
        return std::nullopt;
    }

    AnimationClip clip;
    std::uint8_t loop = 0;
    std::uint32_t trackCount = 0;
    if (!ReadString(in, clip.name) || !ReadPod(in, clip.duration) || clip.duration <= 0.0F ||
        !ReadPod(in, loop) || !ReadPod(in, trackCount) || trackCount > kMaxCount) {
        return std::nullopt;
    }
    clip.loop = loop != 0;

    clip.tracks.reserve(trackCount);
    for (std::uint32_t i = 0; i < trackCount; ++i) {
        AnimationTrack track;
        if (!ReadString(in, track.part) || !ReadKeys(in, track.rotation) ||
            !ReadKeys(in, track.position)) {
            return std::nullopt;
        }
        clip.tracks.push_back(std::move(track));
    }

    std::uint32_t eventCount = 0;
    if (!ReadPod(in, eventCount) || eventCount > kMaxCount) {
        return std::nullopt;
    }
    clip.events.reserve(eventCount);
    for (std::uint32_t i = 0; i < eventCount; ++i) {
        AnimationEvent event;
        if (!ReadPod(in, event.time) || !ReadString(in, event.name)) {
            return std::nullopt;
        }
        clip.events.push_back(std::move(event));
    }
    return clip;
}

std::optional<AnimationClip> ReadAnimationBinaryFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return ReadAnimationBinary(in);
}

}  // namespace voxelgame::vmodel
