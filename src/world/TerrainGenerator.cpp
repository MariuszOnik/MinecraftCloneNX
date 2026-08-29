#include "world/TerrainGenerator.hpp"

#include "world/Block.hpp"
#include "world/World.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace voxelgame {
namespace {

constexpr int kSeaLevel = 9;
constexpr int kBaseHeight = 11;
constexpr float kAmplitude = 9.0F;
constexpr float kFrequency = 0.045F;

std::uint32_t Hash(std::uint32_t x) noexcept {
    x ^= x >> 16;
    x *= 0x7FEB352DU;
    x ^= x >> 15;
    x *= 0x846CA68BU;
    x ^= x >> 16;
    return x;
}

std::uint32_t HashCoords(int x, int z, std::uint32_t salt) noexcept {
    return Hash(static_cast<std::uint32_t>(x) * 0x9E3779B1U ^
                static_cast<std::uint32_t>(z) * 0x85EBCA77U ^ salt);
}

// Deterministic value at an integer lattice point in [-1, 1].
float LatticeValue(int x, int z, std::uint32_t salt) noexcept {
    return static_cast<float>(HashCoords(x, z, salt) & 0xFFFFFFU) /
               static_cast<float>(0x7FFFFFU) -
           1.0F;
}

float SmoothStep(float t) noexcept {
    return t * t * (3.0F - 2.0F * t);
}

// Bilinearly interpolated value noise.
float ValueNoise(float x, float z, std::uint32_t salt) noexcept {
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const float tx = SmoothStep(x - static_cast<float>(x0));
    const float tz = SmoothStep(z - static_cast<float>(z0));

    const float v00 = LatticeValue(x0, z0, salt);
    const float v10 = LatticeValue(x0 + 1, z0, salt);
    const float v01 = LatticeValue(x0, z0 + 1, salt);
    const float v11 = LatticeValue(x0 + 1, z0 + 1, salt);

    const float a = v00 + (v10 - v00) * tx;
    const float b = v01 + (v11 - v01) * tx;
    return a + (b - a) * tz;
}

// Fractal Brownian motion: a few octaves of value noise.
float Fbm(float x, float z, std::uint32_t seed) noexcept {
    float sum = 0.0F;
    float amplitude = 1.0F;
    float frequency = 1.0F;
    float norm = 0.0F;
    for (int octave = 0; octave < 4; ++octave) {
        sum += amplitude * ValueNoise(x * frequency, z * frequency,
                                      seed + static_cast<std::uint32_t>(octave) * 0x1000193U);
        norm += amplitude;
        amplitude *= 0.5F;
        frequency *= 2.0F;
    }
    return sum / norm;
}

bool ShouldPlantTree(int x, int z, std::uint32_t seed) noexcept {
    // Sparse and spaced: the column must be a local maximum of a tree-density
    // field within a small window, and clear the density threshold.
    const auto density = [&](int cx, int cz) {
        return (HashCoords(cx, cz, seed ^ 0x7EE7U) & 0xFFFFU) / 65535.0F;
    };
    const float here = density(x, z);
    if (here < 0.86F) {
        return false;
    }
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            if ((dx != 0 || dz != 0) && density(x + dx, z + dz) >= here) {
                return false;
            }
        }
    }
    return true;
}

void PlantTree(World& world, int x, int surfaceY, int z, std::uint32_t seed) {
    const int trunk = 4 + static_cast<int>(HashCoords(x, z, seed ^ 0xA11CEU) % 3);
    for (int i = 1; i <= trunk; ++i) {
        world.SetBlock(x, surfaceY + i, z, blocks::Wood);
    }
    const int crownBase = surfaceY + trunk - 1;
    for (int dy = 0; dy <= 3; ++dy) {
        const int radius = (dy == 0 || dy == 3) ? 1 : 2;
        for (int dz = -radius; dz <= radius; ++dz) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (std::abs(dx) == radius && std::abs(dz) == radius && radius == 2) {
                    continue;  // clip the corners
                }
                if (dx == 0 && dz == 0 && dy < 3) {
                    continue;  // leave the trunk
                }
                world.SetBlock(x + dx, crownBase + dy, z + dz, blocks::Leaves);
            }
        }
    }
}

}  // namespace

TerrainGenerator::TerrainGenerator(const std::uint32_t seed) noexcept : seed_(seed) {}

int TerrainGenerator::SurfaceHeight(const int worldX, const int worldZ) const noexcept {
    const float n =
        Fbm(static_cast<float>(worldX) * kFrequency, static_cast<float>(worldZ) * kFrequency, seed_);
    return kBaseHeight + static_cast<int>(std::lround(n * kAmplitude));
}

void TerrainGenerator::FillColumn(World& world, const int chunkX, const int chunkZ) const {
    const int maxY = world.BlocksY() - 1;
    const int baseX = chunkX * 16;
    const int baseZ = chunkZ * 16;

    for (int lz = 0; lz < 16; ++lz) {
        for (int lx = 0; lx < 16; ++lx) {
            const int x = baseX + lx;
            const int z = baseZ + lz;
            const int surface = std::clamp(SurfaceHeight(x, z), 1, maxY);
            const bool beach = surface <= kSeaLevel + 1;

            for (int y = 0; y <= surface; ++y) {
                BlockId block = blocks::Stone;
                if (y == 0) {
                    block = blocks::Bedrock;
                } else if (y == surface) {
                    block = beach ? blocks::Sand : blocks::Grass;
                } else if (y + 3 >= surface) {
                    block = beach ? blocks::Sand : blocks::Dirt;
                }
                world.SetBlock(x, y, z, block);
            }

            if (!beach && surface + 8 < world.BlocksY() && ShouldPlantTree(x, z, seed_)) {
                PlantTree(world, x, surface, z, seed_);
            }
        }
    }
}

}  // namespace voxelgame
