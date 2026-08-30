#pragma once

#include "model/ModelMath.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace voxelgame::vmodel {

// A dense voxel volume for one model part. `voxels[Index(x,y,z)]` is a palette
// index; 0 means empty. Layout matches ChunkSection: x fastest, then z, then y.
struct VoxelGrid {
    int sizeX = 0;
    int sizeY = 0;
    int sizeZ = 0;
    std::vector<std::uint8_t> voxels;

    [[nodiscard]] std::size_t Index(const int x, const int y, const int z) const noexcept {
        return static_cast<std::size_t>(x) +
               static_cast<std::size_t>(sizeX) *
                   (static_cast<std::size_t>(z) + static_cast<std::size_t>(sizeZ) * y);
    }
    [[nodiscard]] bool InBounds(const int x, const int y, const int z) const noexcept {
        return x >= 0 && y >= 0 && z >= 0 && x < sizeX && y < sizeY && z < sizeZ;
    }
    [[nodiscard]] std::uint8_t At(const int x, const int y, const int z) const noexcept {
        return InBounds(x, y, z) ? voxels[Index(x, y, z)] : std::uint8_t{0};
    }
    [[nodiscard]] std::size_t Volume() const noexcept {
        return static_cast<std::size_t>(sizeX) * static_cast<std::size_t>(sizeY) *
               static_cast<std::size_t>(sizeZ);
    }
};

struct ModelMaterial {
    std::string name;
    std::uint8_t red = 255;
    std::uint8_t green = 255;
    std::uint8_t blue = 255;
    std::uint8_t alpha = 255;
};

// A rigid part: its own voxel grid plus where it sits and turns relative to its
// parent. All lengths are in voxel units. Rotation is applied about `pivot`
// (part-local voxel coords); animation only ever changes `rotationDegrees`.
struct VoxelModelPart {
    std::string name;
    int parent = -1;  // index into VoxelModel::parts, -1 for a root part
    Vec3 position;    // offset of this part's origin from the parent's origin
    Vec3 pivot;       // rotation centre, in this part's local voxel space
    Vec3 rotationDegrees;
    VoxelGrid grid;
};

// A hierarchy of rigid voxel parts sharing one material palette. `parts` is
// ordered so every part comes after its parent (a valid load guarantees this).
struct VoxelModel {
    std::string name;
    float voxelSize = 1.0F / 16.0F;  // world units per voxel (16 vox == 1 block)
    std::vector<ModelMaterial> palette;  // indexed 1..N; index 0 is "empty"
    std::vector<VoxelModelPart> parts;

    [[nodiscard]] const ModelMaterial* Material(std::uint8_t index) const noexcept;
};

// World matrix of every part, in `parts` order: parentWorld * T(position) *
// T(pivot) * R(rotation) * T(-pivot). `root` is prepended to every part (use it
// to place and scale the whole model in the world).
[[nodiscard]] std::vector<Mat4> ResolvePartMatrices(const VoxelModel& model, const Mat4& root);

}  // namespace voxelgame::vmodel
