#include "world/PlayerBody.hpp"

#include "world/Block.hpp"
#include "world/World.hpp"

#include <algorithm>
#include <cmath>

namespace voxelgame {
namespace {

int FloorToInt(const float value) noexcept {
    return static_cast<int>(std::floor(value));
}

}  // namespace

PlayerBody::PlayerBody(const Vec3 feetPosition) noexcept : position_(feetPosition) {}

Vec3 PlayerBody::EyePosition() const noexcept {
    return {position_.x, position_.y + EyeHeight, position_.z};
}

bool PlayerBody::Intersects(const int bx, const int by, const int bz) const noexcept {
    const float half = Width * 0.5F;
    return static_cast<float>(bx) < position_.x + half &&
           static_cast<float>(bx + 1) > position_.x - half &&
           static_cast<float>(by) < position_.y + Height && static_cast<float>(by + 1) > position_.y &&
           static_cast<float>(bz) < position_.z + half &&
           static_cast<float>(bz + 1) > position_.z - half;
}

bool PlayerBody::Collides(const World& world) const noexcept {
    const float half = Width * 0.5F;
    // Shrink by a whisker so touching a flush surface is not a collision.
    constexpr float skin = 1.0E-3F;
    const int minX = FloorToInt(position_.x - half + skin);
    const int maxX = FloorToInt(position_.x + half - skin);
    const int minY = FloorToInt(position_.y + skin);
    const int maxY = FloorToInt(position_.y + Height - skin);
    const int minZ = FloorToInt(position_.z - half + skin);
    const int maxZ = FloorToInt(position_.z + half - skin);

    for (int y = minY; y <= maxY; ++y) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                if (IsCollidableBlock(world.GetBlock(x, y, z))) {
                    return true;
                }
            }
        }
    }
    return false;
}

void PlayerBody::Step(const World& world, Vec3 wishVelocity, const bool jump,
                      const float dt) noexcept {
    if (dt <= 0.0F) {
        return;
    }

    inWater_ = world.GetBlock(FloorToInt(position_.x), FloorToInt(position_.y + 0.6F),
                              FloorToInt(position_.z)) == blocks::Water;

    if (inWater_) {
        // Buoyant, draggy, and jump swims you upward.
        velocityY_ -= Gravity * 0.28F * dt;
        velocityY_ = std::clamp(velocityY_ - velocityY_ * 4.0F * dt, -4.0F, 6.0F);
        if (jump) {
            velocityY_ = 4.2F;
        }
        wishVelocity.x *= 0.6F;
        wishVelocity.z *= 0.6F;
    } else {
        velocityY_ -= Gravity * dt;
        velocityY_ = std::clamp(velocityY_, -TerminalFall, TerminalFall);
        if (jump && onGround_) {
            velocityY_ = JumpSpeed;
            onGround_ = false;
        }
    }

    const Vec3 delta{wishVelocity.x * dt, velocityY_ * dt, wishVelocity.z * dt};

    position_.x += delta.x;
    if (Collides(world)) {
        position_.x -= delta.x;
    }

    position_.z += delta.z;
    if (Collides(world)) {
        position_.z -= delta.z;
    }

    position_.y += delta.y;
    if (Collides(world)) {
        position_.y -= delta.y;
        onGround_ = delta.y < 0.0F;
        velocityY_ = 0.0F;
    } else {
        onGround_ = false;
    }
}

}  // namespace voxelgame
