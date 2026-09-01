#pragma once

#include <cstdint>
#include <vector>

namespace voxelgame {
class World;
}

namespace voxelgame::battle {

// Coarse terrain class of a tile's surface block, for movement cost and cover.
enum class Terrain : std::uint8_t {
    None,   // no floor
    Grass,
    Dirt,
    Stone,
    Sand,
    Wood,
    Water,
    Hazard,
};

struct Tile {
    int height = 0;            // world Y a unit's feet rest on (top solid + 1)
    bool hasFloor = false;     // a cube-shaped block to stand on / over
    bool walkable = false;     // hasFloor, solid top, not water/hazard, 2 air of headroom
    Terrain terrain = Terrain::None;
    int occupant = -1;         // unit handle index, -1 = empty (set by the battle)
};

// A rectangular grid of tiles over a bounded region of a voxel World. Tile
// coordinates are world block X/Z; each tile summarises that column's surface.
// No raylib dependency: derivation and queries are unit-testable.
class TileGrid {
public:
    TileGrid(int originX, int originZ, int sizeX, int sizeZ);

    // Rescans every column's surface from `world`. Call after the map is built
    // and whenever terrain changes.
    void Rebuild(const World& world);

    [[nodiscard]] int OriginX() const noexcept { return originX_; }
    [[nodiscard]] int OriginZ() const noexcept { return originZ_; }
    [[nodiscard]] int SizeX() const noexcept { return sizeX_; }
    [[nodiscard]] int SizeZ() const noexcept { return sizeZ_; }

    [[nodiscard]] bool InBounds(int tileX, int tileZ) const noexcept;
    [[nodiscard]] const Tile& At(int tileX, int tileZ) const noexcept;
    [[nodiscard]] Tile& At(int tileX, int tileZ) noexcept;

    [[nodiscard]] std::size_t WalkableCount() const noexcept;

private:
    [[nodiscard]] std::size_t Index(int tileX, int tileZ) const noexcept;

    int originX_;
    int originZ_;
    int sizeX_;
    int sizeZ_;
    std::vector<Tile> tiles_;
    Tile outside_{};  // returned for out-of-bounds queries
};

}  // namespace voxelgame::battle
