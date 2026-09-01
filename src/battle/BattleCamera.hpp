#pragma once

#include <raylib.h>

namespace voxelgame::battle {

// Isometric battle camera: orthographic, a fixed downward pitch, a yaw that snaps
// in 90-degree steps, wheel zoom, and a focus point that pans within the map and
// eases toward whatever it is told to follow.
class BattleCamera {
public:
    BattleCamera();

    void SetBounds(float minX, float minZ, float maxX, float maxZ) noexcept;

    void FollowInstant(Vector3 worldPoint) noexcept;  // snap now
    void Follow(Vector3 worldPoint) noexcept;         // ease toward

    void RotateLeft() noexcept;
    void RotateRight() noexcept;
    void Pan(float forward, float right, float dt) noexcept;  // relative to yaw
    void Zoom(float wheelDelta) noexcept;

    void Update(float dt) noexcept;  // eases state, rebuilds the Camera3D

    [[nodiscard]] const Camera3D& Camera() const noexcept { return camera_; }
    [[nodiscard]] float Yaw() const noexcept { return yaw_; }

    // Unit-length world XZ basis for the current yaw: forward is "into the
    // screen", right is "screen right". Movement / cursor input uses this.
    void MoveBasis(float& forwardX, float& forwardZ, float& rightX, float& rightZ) const noexcept;

private:
    void ClampFocus() noexcept;

    Camera3D camera_{};
    Vector3 focus_{};
    Vector3 focusTarget_{};
    float yaw_;
    float yawTarget_;
    float orthoHeight_;
    float orthoTarget_;
    float minX_ = -1e6F, minZ_ = -1e6F, maxX_ = 1e6F, maxZ_ = 1e6F;
};

}  // namespace voxelgame::battle
