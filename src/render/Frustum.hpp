#pragma once

#include <raylib.h>

namespace voxelgame {

struct FrustumPlane {
    Vector3 normal;  // points into the frustum
    float d;         // plane: dot(normal, p) + d >= 0 inside
};

struct Frustum {
    FrustumPlane planes[6];  // near, far, left, right, bottom, top
};

// Builds the six view-frustum planes for a perspective camera. `aspect` is
// width / height of the render target.
[[nodiscard]] Frustum MakeFrustum(const Camera3D& camera, float aspect);

// True when the axis-aligned box [min, max] is at least partly inside the frustum.
[[nodiscard]] bool AabbInFrustum(const Frustum& frustum, Vector3 min, Vector3 max);

}  // namespace voxelgame
