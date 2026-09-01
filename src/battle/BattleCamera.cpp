#include "battle/BattleCamera.hpp"

#include <algorithm>
#include <cmath>

namespace voxelgame::battle {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kBaseYaw = kPi * 0.25F;   // 45 degrees
constexpr float kPitch = 0.61547971F;     // ~35.26 -- true 2:1 isometric
constexpr float kDistance = 60.0F;
constexpr float kMinHeight = 8.0F;
constexpr float kMaxHeight = 48.0F;
constexpr float kPanSpeed = 14.0F;

float EaseTo(const float value, const float target, const float rate, const float dt) noexcept {
    return value + (target - value) * std::min(1.0F, rate * dt);
}

}  // namespace

BattleCamera::BattleCamera()
    : yaw_(kBaseYaw), yawTarget_(kBaseYaw), orthoHeight_(24.0F), orthoTarget_(24.0F) {
    camera_.up = {0.0F, 1.0F, 0.0F};
    camera_.projection = CAMERA_ORTHOGRAPHIC;
    camera_.fovy = orthoHeight_;
}

void BattleCamera::SetBounds(const float minX, const float minZ, const float maxX,
                             const float maxZ) noexcept {
    minX_ = minX;
    minZ_ = minZ;
    maxX_ = maxX;
    maxZ_ = maxZ;
}

void BattleCamera::ClampFocus() noexcept {
    focusTarget_.x = std::clamp(focusTarget_.x, minX_, maxX_);
    focusTarget_.z = std::clamp(focusTarget_.z, minZ_, maxZ_);
}

void BattleCamera::FollowInstant(const Vector3 worldPoint) noexcept {
    focusTarget_ = worldPoint;
    ClampFocus();
    focus_ = focusTarget_;
}

void BattleCamera::Follow(const Vector3 worldPoint) noexcept {
    focusTarget_ = worldPoint;
    ClampFocus();
}

void BattleCamera::RotateLeft() noexcept {
    yawTarget_ -= kPi * 0.5F;
}

void BattleCamera::RotateRight() noexcept {
    yawTarget_ += kPi * 0.5F;
}

void BattleCamera::Pan(const float forward, const float right, const float dt) noexcept {
    float fx;
    float fz;
    float rx;
    float rz;
    MoveBasis(fx, fz, rx, rz);
    focusTarget_.x += (fx * forward + rx * right) * kPanSpeed * dt;
    focusTarget_.z += (fz * forward + rz * right) * kPanSpeed * dt;
    ClampFocus();
}

void BattleCamera::Zoom(const float wheelDelta) noexcept {
    orthoTarget_ = std::clamp(orthoTarget_ - wheelDelta * 2.0F, kMinHeight, kMaxHeight);
}

void BattleCamera::MoveBasis(float& forwardX, float& forwardZ, float& rightX,
                             float& rightZ) const noexcept {
    forwardX = std::sin(yaw_);
    forwardZ = -std::cos(yaw_);
    rightX = std::cos(yaw_);
    rightZ = std::sin(yaw_);
}

void BattleCamera::Update(const float dt) noexcept {
    focus_.x = EaseTo(focus_.x, focusTarget_.x, 10.0F, dt);
    focus_.y = EaseTo(focus_.y, focusTarget_.y, 10.0F, dt);
    focus_.z = EaseTo(focus_.z, focusTarget_.z, 10.0F, dt);
    yaw_ = EaseTo(yaw_, yawTarget_, 9.0F, dt);
    orthoHeight_ = EaseTo(orthoHeight_, orthoTarget_, 10.0F, dt);

    const Vector3 dir{std::sin(yaw_) * std::cos(kPitch), -std::sin(kPitch),
                      -std::cos(yaw_) * std::cos(kPitch)};
    camera_.position = {focus_.x - dir.x * kDistance, focus_.y - dir.y * kDistance,
                        focus_.z - dir.z * kDistance};
    camera_.target = focus_;
    camera_.fovy = orthoHeight_;
}

}  // namespace voxelgame::battle
