#include "render/Frustum.hpp"

#include <raymath.h>

#include <cmath>

namespace voxelgame {
namespace {

FrustumPlane PlaneThrough(const Vector3 point, const Vector3 normal) {
    const Vector3 n = Vector3Normalize(normal);
    return {n, -Vector3DotProduct(n, point)};
}

FrustumPlane PlaneFrom3(const Vector3 a, const Vector3 b, const Vector3 c) {
    const Vector3 n =
        Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
    return {n, -Vector3DotProduct(n, a)};
}

FrustumPlane FaceInward(FrustumPlane plane, const Vector3 interior) {
    if (Vector3DotProduct(plane.normal, interior) + plane.d < 0.0F) {
        plane.normal = Vector3Negate(plane.normal);
        plane.d = -plane.d;
    }
    return plane;
}

}  // namespace

Frustum MakeFrustum(const Camera3D& camera, const float aspect) {
    constexpr float kNear = 0.05F;
    constexpr float kFar = 1000.0F;

    const Vector3 forward =
        Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3CrossProduct(right, forward);

    const float tangent = std::tan(camera.fovy * 0.5F * DEG2RAD);
    const float nearHalfH = kNear * tangent;
    const float nearHalfW = nearHalfH * aspect;

    const Vector3 nearCentre = Vector3Add(camera.position, Vector3Scale(forward, kNear));
    const Vector3 farCentre = Vector3Add(camera.position, Vector3Scale(forward, kFar));
    const Vector3 interior = Vector3Add(camera.position, Vector3Scale(forward, kFar * 0.5F));

    const Vector3 nearTopLeft = Vector3Subtract(
        Vector3Add(nearCentre, Vector3Scale(up, nearHalfH)), Vector3Scale(right, nearHalfW));
    const Vector3 nearTopRight = Vector3Add(
        Vector3Add(nearCentre, Vector3Scale(up, nearHalfH)), Vector3Scale(right, nearHalfW));
    const Vector3 nearBotLeft = Vector3Subtract(
        Vector3Subtract(nearCentre, Vector3Scale(up, nearHalfH)), Vector3Scale(right, nearHalfW));
    const Vector3 nearBotRight = Vector3Add(
        Vector3Subtract(nearCentre, Vector3Scale(up, nearHalfH)), Vector3Scale(right, nearHalfW));

    Frustum frustum{};
    frustum.planes[0] = FaceInward(PlaneThrough(nearCentre, forward), interior);
    frustum.planes[1] = FaceInward(PlaneThrough(farCentre, Vector3Negate(forward)), interior);
    frustum.planes[2] = FaceInward(PlaneFrom3(camera.position, nearBotLeft, nearTopLeft), interior);
    frustum.planes[3] = FaceInward(PlaneFrom3(camera.position, nearTopRight, nearBotRight), interior);
    frustum.planes[4] = FaceInward(PlaneFrom3(camera.position, nearBotRight, nearBotLeft), interior);
    frustum.planes[5] = FaceInward(PlaneFrom3(camera.position, nearTopLeft, nearTopRight), interior);
    return frustum;
}

bool AabbInFrustum(const Frustum& frustum, const Vector3 min, const Vector3 max) {
    for (const FrustumPlane& plane : frustum.planes) {
        const Vector3 positive{
            plane.normal.x >= 0.0F ? max.x : min.x,
            plane.normal.y >= 0.0F ? max.y : min.y,
            plane.normal.z >= 0.0F ? max.z : min.z,
        };
        if (Vector3DotProduct(plane.normal, positive) + plane.d < 0.0F) {
            return false;
        }
    }
    return true;
}

}  // namespace voxelgame
