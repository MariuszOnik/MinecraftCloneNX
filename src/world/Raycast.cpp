#include "world/Raycast.hpp"

#include "world/Block.hpp"
#include "world/World.hpp"

#include <cmath>

namespace voxelgame {
namespace {

int FloorToInt(const float value) noexcept {
    return static_cast<int>(std::floor(value));
}

}  // namespace

RaycastHit Raycast(const World& world, const Vec3 origin, Vec3 direction,
                   const float maxDistance) {
    const float length =
        std::sqrt(direction.x * direction.x + direction.y * direction.y +
                  direction.z * direction.z);
    if (length < 1.0e-6F || maxDistance <= 0.0F) {
        return {};
    }
    direction = {direction.x / length, direction.y / length, direction.z / length};

    int x = FloorToInt(origin.x);
    int y = FloorToInt(origin.y);
    int z = FloorToInt(origin.z);

    const int stepX = direction.x > 0.0F ? 1 : (direction.x < 0.0F ? -1 : 0);
    const int stepY = direction.y > 0.0F ? 1 : (direction.y < 0.0F ? -1 : 0);
    const int stepZ = direction.z > 0.0F ? 1 : (direction.z < 0.0F ? -1 : 0);

    constexpr float kInf = 1.0e30F;
    const float tDeltaX = stepX != 0 ? std::fabs(1.0F / direction.x) : kInf;
    const float tDeltaY = stepY != 0 ? std::fabs(1.0F / direction.y) : kInf;
    const float tDeltaZ = stepZ != 0 ? std::fabs(1.0F / direction.z) : kInf;

    const auto firstBoundary = [](const float pos, const int step, const float delta) {
        if (step > 0) {
            return (std::floor(pos) + 1.0F - pos) * delta;
        }
        if (step < 0) {
            return (pos - std::floor(pos)) * delta;
        }
        return kInf;
    };
    float tMaxX = firstBoundary(origin.x, stepX, tDeltaX);
    float tMaxY = firstBoundary(origin.y, stepY, tDeltaY);
    float tMaxZ = firstBoundary(origin.z, stepZ, tDeltaZ);

    int normalX = 0;
    int normalY = 0;
    int normalZ = 0;
    float travelled = 0.0F;

    while (travelled <= maxDistance) {
        if (IsCollidableBlock(world.GetBlock(x, y, z))) {
            return {true, x, y, z, normalX, normalY, normalZ};
        }

        if (tMaxX <= tMaxY && tMaxX <= tMaxZ) {
            x += stepX;
            travelled = tMaxX;
            tMaxX += tDeltaX;
            normalX = -stepX;
            normalY = 0;
            normalZ = 0;
        } else if (tMaxY <= tMaxZ) {
            y += stepY;
            travelled = tMaxY;
            tMaxY += tDeltaY;
            normalX = 0;
            normalY = -stepY;
            normalZ = 0;
        } else {
            z += stepZ;
            travelled = tMaxZ;
            tMaxZ += tDeltaZ;
            normalX = 0;
            normalY = 0;
            normalZ = -stepZ;
        }
    }

    return {};
}

}  // namespace voxelgame
