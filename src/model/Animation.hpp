#pragma once

#include "model/VoxelModel.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace voxelgame::vmodel {

struct Keyframe {
    float time = 0.0F;  // seconds from the clip start
    Vec3 value;
};

// One part's animated channels. A rigid part only rotates and slides, so there
// is no scale channel. Either list may be empty (that channel stays at rest).
struct AnimationTrack {
    std::string part;
    std::vector<Keyframe> rotation;  // Euler degrees, added to the rest rotation
    std::vector<Keyframe> position;  // voxel units, added to the rest position
};

struct AnimationEvent {
    float time = 0.0F;
    std::string name;  // e.g. "footstep", "hit", "sound"
};

// A clip of transform tracks over `duration` seconds (PLAN.md 6.2).
struct AnimationClip {
    std::string name;
    float duration = 0.0F;
    bool loop = true;
    std::vector<AnimationTrack> tracks;
    std::vector<AnimationEvent> events;
};

// Parses a .vxa.json clip. On failure returns nullopt and writes a reason into
// `error`. See assets/animations/*.vxa.json for the shape.
[[nodiscard]] std::optional<AnimationClip> ParseAnimationClip(std::string_view jsonText,
                                                             std::string& error);

// Per-part pose for `model` at `time` seconds (linear interpolation; values
// before the first / after the last key clamp). Tracks whose part is not in the
// model are skipped. The caller wraps `time` for looping.
[[nodiscard]] std::vector<PartPose> SamplePose(const AnimationClip& clip, const VoxelModel& model,
                                               float time);

// Linear blend of two equal-length poses: w<=0 gives `a`, w>=1 gives `b`.
[[nodiscard]] std::vector<PartPose> BlendPoses(const std::vector<PartPose>& a,
                                               const std::vector<PartPose>& b, float w);

// A play head over one clip.
struct AnimationState {
    const AnimationClip* clip = nullptr;
    float time = 0.0F;
    float speed = 1.0F;

    // Advances `time` by speed*dt, wrapping when a looping clip passes its end.
    // Event names crossed during this step are appended to `firedEvents` in play
    // order (pass null to ignore them).
    void Advance(float dt, std::vector<std::string>* firedEvents = nullptr);

    // True once a non-looping clip has run past its duration.
    [[nodiscard]] bool Finished() const noexcept;

    [[nodiscard]] std::vector<PartPose> Sample(const VoxelModel& model) const;
};

}  // namespace voxelgame::vmodel
