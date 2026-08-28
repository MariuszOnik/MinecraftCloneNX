#pragma once

#include "world/PlayerBody.hpp"  // for Vec3

namespace voxelgame {

class World;

struct RaycastHit {
    bool hit = false;
    int blockX = 0;
    int blockY = 0;
    int blockZ = 0;
    // Unit face normal of the side the ray entered through (0 on a miss).
    int normalX = 0;
    int normalY = 0;
    int normalZ = 0;
};

// Amanatides & Woo voxel DDA: the first solid block hit by the ray
// origin + t*direction for t in [0, maxDistance]. `direction` need not be
// normalised; `maxDistance` is measured in units of `direction` length.
[[nodiscard]] RaycastHit Raycast(const World& world, Vec3 origin, Vec3 direction,
                                 float maxDistance);

}  // namespace voxelgame
