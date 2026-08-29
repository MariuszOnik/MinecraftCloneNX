#pragma once

#include <cstdint>

namespace voxelgame {

class World;

// Procedural terrain: a deterministic fractal-noise heightmap with bedrock /
// stone / dirt / grass (sand on low ground) layers and scattered trees. The same
// seed always produces the same world -- no raylib, so it is unit-testable.
class TerrainGenerator {
public:
    explicit TerrainGenerator(std::uint32_t seed) noexcept;

    // Fills every column of `world` with generated terrain and trees.
    void Generate(World& world) const;

    // World Y of the topmost solid block in the column (x, z).
    [[nodiscard]] int SurfaceHeight(int worldX, int worldZ) const noexcept;

    [[nodiscard]] std::uint32_t Seed() const noexcept { return seed_; }

private:
    std::uint32_t seed_;
};

}  // namespace voxelgame
