#include "model/Animation.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace voxelgame::vmodel {
namespace {

using nlohmann::json;

Vec3 Lerp(const Vec3 a, const Vec3 b, const float t) noexcept {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

// Value of a keyframe channel at `time`: clamp outside the range, linear inside.
Vec3 SampleChannel(const std::vector<Keyframe>& keys, const float time) noexcept {
    if (keys.empty()) {
        return {};
    }
    if (time <= keys.front().time) {
        return keys.front().value;
    }
    if (time >= keys.back().time) {
        return keys.back().value;
    }
    for (std::size_t i = 1; i < keys.size(); ++i) {
        if (time <= keys[i].time) {
            const Keyframe& lo = keys[i - 1];
            const Keyframe& hi = keys[i];
            const float span = hi.time - lo.time;
            const float t = span > 0.0F ? (time - lo.time) / span : 0.0F;
            return Lerp(lo.value, hi.value, t);
        }
    }
    return keys.back().value;
}

bool ReadKeyframes(const json& node, const char* key, std::vector<Keyframe>& out,
                   std::string& error) {
    const auto it = node.find(key);
    if (it == node.end()) {
        return true;
    }
    if (!it->is_array()) {
        error = std::string(key) + " must be an array of keyframes";
        return false;
    }
    for (const auto& frame : *it) {
        const auto value = frame.find("value");
        if (!frame.contains("t") || value == frame.end() || !value->is_array() ||
            value->size() != 3) {
            error = std::string(key) + " keyframe needs \"t\" and a [x, y, z] \"value\"";
            return false;
        }
        Keyframe k;
        k.time = frame.at("t").get<float>();
        k.value = {value->at(0).get<float>(), value->at(1).get<float>(),
                   value->at(2).get<float>()};
        out.push_back(k);
    }
    std::sort(out.begin(), out.end(),
              [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
    return true;
}

}  // namespace

std::optional<AnimationClip> ParseAnimationClip(const std::string_view jsonText,
                                                std::string& error) {
    json root = json::parse(jsonText.begin(), jsonText.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        error = "clip is not valid JSON";
        return std::nullopt;
    }

    AnimationClip clip;
    clip.name = root.value("name", std::string{});
    clip.duration = root.value("duration", 0.0F);
    clip.loop = root.value("loop", true);
    if (clip.duration <= 0.0F) {
        error = "clip needs a positive \"duration\"";
        return std::nullopt;
    }

    const auto tracks = root.find("tracks");
    if (tracks == root.end() || !tracks->is_array() || tracks->empty()) {
        error = "clip needs a non-empty \"tracks\" array";
        return std::nullopt;
    }
    for (const auto& node : *tracks) {
        AnimationTrack track;
        track.part = node.value("part", std::string{});
        if (track.part.empty()) {
            error = "every track needs a \"part\"";
            return std::nullopt;
        }
        if (!ReadKeyframes(node, "rotation", track.rotation, error) ||
            !ReadKeyframes(node, "position", track.position, error)) {
            error = "track \"" + track.part + "\": " + error;
            return std::nullopt;
        }
        if (track.rotation.empty() && track.position.empty()) {
            error = "track \"" + track.part + "\" has no keyframes";
            return std::nullopt;
        }
        clip.tracks.push_back(std::move(track));
    }

    const auto events = root.find("events");
    if (events != root.end() && events->is_array()) {
        for (const auto& node : *events) {
            AnimationEvent event;
            event.time = node.value("t", 0.0F);
            event.name = node.value("name", std::string{});
            if (!event.name.empty()) {
                clip.events.push_back(std::move(event));
            }
        }
        std::sort(clip.events.begin(), clip.events.end(),
                  [](const AnimationEvent& a, const AnimationEvent& b) { return a.time < b.time; });
    }
    return clip;
}

std::vector<PartPose> SamplePose(const AnimationClip& clip, const VoxelModel& model,
                                 const float time) {
    std::vector<PartPose> pose(model.parts.size());
    for (const AnimationTrack& track : clip.tracks) {
        int index = -1;
        for (std::size_t i = 0; i < model.parts.size(); ++i) {
            if (model.parts[i].name == track.part) {
                index = static_cast<int>(i);
                break;
            }
        }
        if (index < 0) {
            continue;
        }
        pose[static_cast<std::size_t>(index)].rotationDegrees =
            SampleChannel(track.rotation, time);
        pose[static_cast<std::size_t>(index)].positionOffset = SampleChannel(track.position, time);
    }
    return pose;
}

std::vector<PartPose> BlendPoses(const std::vector<PartPose>& a, const std::vector<PartPose>& b,
                                 const float w) {
    if (a.size() != b.size()) {
        return a;
    }
    const float t = std::clamp(w, 0.0F, 1.0F);
    std::vector<PartPose> out(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        out[i].rotationDegrees = Lerp(a[i].rotationDegrees, b[i].rotationDegrees, t);
        out[i].positionOffset = Lerp(a[i].positionOffset, b[i].positionOffset, t);
    }
    return out;
}

void AnimationState::Advance(const float dt, std::vector<std::string>* firedEvents) {
    if (clip == nullptr || clip->duration <= 0.0F) {
        return;
    }
    const float start = time;
    time += speed * dt;

    const auto collect = [&](const float from, const float to) {
        if (firedEvents == nullptr) {
            return;
        }
        for (const AnimationEvent& event : clip->events) {
            if (event.time >= from && event.time < to) {
                firedEvents->push_back(event.name);
            }
        }
    };

    if (clip->loop) {
        float end = time;
        if (end >= clip->duration) {
            collect(start, clip->duration);
            end = std::fmod(end, clip->duration);
            collect(0.0F, end);
            time = end;
        } else {
            collect(start, end);
        }
    } else {
        const float clamped = std::min(time, clip->duration);
        // Once we hit the end, an event sitting exactly on `duration` still fires.
        const float upper = clamped >= clip->duration ? clip->duration + 1.0e-4F : clamped;
        collect(start, upper);
        time = clamped;
    }
}

bool AnimationState::Finished() const noexcept {
    return clip != nullptr && !clip->loop && time >= clip->duration;
}

std::vector<PartPose> AnimationState::Sample(const VoxelModel& model) const {
    if (clip == nullptr) {
        return std::vector<PartPose>(model.parts.size());
    }
    return SamplePose(*clip, model, time);
}

void Animator::Play(const AnimationClip* clip, const float fadeSeconds) {
    if (clip == current_.clip) {
        return;
    }
    previous_ = current_;
    current_ = AnimationState{clip, 0.0F, 1.0F};
    if (fadeSeconds > 0.0F && previous_.clip != nullptr) {
        weight_ = 0.0F;
        weightRate_ = 1.0F / fadeSeconds;
    } else {
        weight_ = 1.0F;
        weightRate_ = 0.0F;
        previous_ = AnimationState{};
    }
}

void Animator::Update(const float dt, std::vector<std::string>* firedEvents) {
    current_.Advance(dt, firedEvents);
    if (weight_ < 1.0F) {
        previous_.Advance(dt, nullptr);
        weight_ = std::min(1.0F, weight_ + weightRate_ * dt);
        if (weight_ >= 1.0F) {
            previous_ = AnimationState{};
        }
    }
}

std::vector<PartPose> Animator::Pose(const VoxelModel& model) const {
    if (weight_ >= 1.0F || previous_.clip == nullptr) {
        return current_.Sample(model);
    }
    return BlendPoses(previous_.Sample(model), current_.Sample(model), weight_);
}

}  // namespace voxelgame::vmodel
