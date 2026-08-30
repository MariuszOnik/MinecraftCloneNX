#pragma once

#include <array>
#include <cmath>

namespace voxelgame::vmodel {

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

// Column-major 4x4, laid out exactly like raylib's Matrix (translation in
// m[12..14]) so the renderer can memcpy the fields across. Multiplication mirrors
// raymath's MatrixMultiply, so `Parent * Child` composes transforms the same way.
struct Mat4 {
    std::array<float, 16> m{};

    static Mat4 Identity() noexcept {
        Mat4 r;
        r.m = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        return r;
    }

    static Mat4 Translate(const float x, const float y, const float z) noexcept {
        Mat4 r = Identity();
        r.m[12] = x;
        r.m[13] = y;
        r.m[14] = z;
        return r;
    }

    static Mat4 Scale(const float s) noexcept {
        Mat4 r = Identity();
        r.m[0] = s;
        r.m[5] = s;
        r.m[10] = s;
        return r;
    }

    // Intrinsic X, then Y, then Z rotation, angles in radians.
    static Mat4 RotateXYZ(const Vec3 radians) noexcept {
        const float cx = std::cos(radians.x);
        const float sx = std::sin(radians.x);
        const float cy = std::cos(radians.y);
        const float sy = std::sin(radians.y);
        const float cz = std::cos(radians.z);
        const float sz = std::sin(radians.z);

        Mat4 r = Identity();
        r.m[0] = cy * cz;
        r.m[1] = cy * sz;
        r.m[2] = -sy;
        r.m[4] = sx * sy * cz - cx * sz;
        r.m[5] = sx * sy * sz + cx * cz;
        r.m[6] = sx * cy;
        r.m[8] = cx * sy * cz + sx * sz;
        r.m[9] = cx * sy * sz - sx * cz;
        r.m[10] = cx * cy;
        return r;
    }

    Vec3 TransformPoint(const Vec3 p) const noexcept {
        return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
                m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
                m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
    }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) noexcept {
    Mat4 r;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0F;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[static_cast<std::size_t>(k * 4 + row)] *
                       b.m[static_cast<std::size_t>(col * 4 + k)];
            }
            r.m[static_cast<std::size_t>(col * 4 + row)] = sum;
        }
    }
    return r;
}

}  // namespace voxelgame::vmodel
