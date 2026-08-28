#pragma once

namespace voxelgame {

class World;

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

// An axis-aligned player capsule (box) that walks and falls through a voxel
// World. Collision is resolved one axis at a time so the body slides along walls
// and floors. No rendering dependency, so the motion is unit-testable.
class PlayerBody {
public:
    static constexpr float Width = 0.6F;       // full width and depth
    static constexpr float Height = 1.8F;
    static constexpr float EyeHeight = 1.62F;
    static constexpr float Gravity = 28.0F;    // m/s^2
    static constexpr float JumpSpeed = 8.4F;   // m/s
    static constexpr float TerminalFall = 55.0F;

    explicit PlayerBody(Vec3 feetPosition) noexcept;

    // Advances the body by `dt` seconds. `wishVelocity` is the desired horizontal
    // velocity (m/s, y ignored); `jump` triggers a jump when grounded.
    void Step(const World& world, Vec3 wishVelocity, bool jump, float dt) noexcept;

    [[nodiscard]] Vec3 Position() const noexcept { return position_; }
    [[nodiscard]] Vec3 EyePosition() const noexcept;
    [[nodiscard]] bool OnGround() const noexcept { return onGround_; }
    [[nodiscard]] float VerticalVelocity() const noexcept { return velocityY_; }

    // Whether the body's box overlaps the unit cube at (bx, by, bz) -- used to
    // stop the player placing a block inside themselves.
    [[nodiscard]] bool Intersects(int bx, int by, int bz) const noexcept;

private:
    [[nodiscard]] bool Collides(const World& world) const noexcept;

    Vec3 position_;  // centre of the bottom face ("feet")
    float velocityY_ = 0.0F;
    bool onGround_ = false;
};

}  // namespace voxelgame
