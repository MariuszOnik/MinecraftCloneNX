#include "battle/BattleMap.hpp"
#include "battle/TileGrid.hpp"
#include "battle/Unit.hpp"
#include "world/Block.hpp"
#include "world/World.hpp"

#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void Expect(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    using namespace voxelgame;
    using namespace voxelgame::battle;

    // A hand-built 5x5 patch: a flat stone floor at Y=2 (feet at Y=3), one raised
    // tile, one two-tall pillar, one water tile, and one empty column (a hole).
    {
        World world(1);
        world.EnsureColumn(0, 0);
        for (int z = 0; z < 5; ++z) {
            for (int x = 0; x < 5; ++x) {
                if (x == 4 && z == 4) {
                    continue;  // hole: no floor at all
                }
                world.SetBlock(x, 1, z, blocks::Stone);
                world.SetBlock(x, 2, z, (x == 3 && z == 3) ? blocks::Water : blocks::Stone);
            }
        }
        world.SetBlock(0, 2, 0, blocks::Grass);   // terrain classification
        world.SetBlock(1, 3, 1, blocks::Stone);   // raised one step
        world.SetBlock(2, 3, 2, blocks::Stone);   // pillar over (2,2)
        world.SetBlock(2, 4, 2, blocks::Stone);

        TileGrid grid(0, 0, 5, 5);
        grid.Rebuild(world);

        Expect(grid.InBounds(0, 0) && !grid.InBounds(5, 0), "bounds check");

        const Tile& flat = grid.At(1, 0);
        Expect(flat.hasFloor && flat.walkable, "flat tile is walkable");
        Expect(flat.height == 3, "feet rest one above the surface block");
        Expect(flat.terrain == Terrain::Stone, "stone surface -> Stone terrain");

        Expect(grid.At(0, 0).terrain == Terrain::Grass, "grass surface -> Grass terrain");

        const Tile& raised = grid.At(1, 1);
        Expect(raised.walkable && raised.height == 4, "the raised tile is a step higher");

        const Tile& pillar = grid.At(2, 2);
        Expect(pillar.walkable && pillar.height == 5, "a unit can stand on the pillar top");

        const Tile& water = grid.At(3, 3);
        Expect(water.hasFloor && !water.walkable && water.terrain == Terrain::Water,
               "water has a floor but is not walkable");
        Expect(water.height == 2, "water tile height is the surface");

        const Tile& hole = grid.At(4, 4);
        Expect(!hole.hasFloor && !hole.walkable && hole.terrain == Terrain::None,
               "an empty column is a hole");

        Expect(grid.WalkableCount() == 23,
               "23 of 25 tiles walkable (minus the water and the hole)");

        const Tile& outside = grid.At(99, 99);
        Expect(!outside.hasFloor && !outside.walkable, "out-of-bounds reads as empty");
    }

    // The built-in arena: it loads, has a sane walkable area, and a raised
    // platform higher than the base floor.
    {
        BattleMap map;
        Expect(map.SizeX() == 24 && map.SizeZ() == 24, "default arena is 24x24");

        const TileGrid& grid = map.Grid();
        const std::size_t walkable = grid.WalkableCount();
        Expect(walkable > 400 && walkable < 576, "most of the arena is walkable, but not all");

        const int baseHeight = grid.At(20, 20).height;
        Expect(baseHeight == 4, "arena floor puts feet at Y=4");
        Expect(grid.At(11, 11).height == baseHeight + 2, "centre platform is two blocks up");
        Expect(grid.At(2, 2).terrain == Terrain::Water, "the south strip is water");
        Expect(!grid.At(2, 2).walkable, "water is not walkable");
    }

    // UnitRegistry: generational handles survive removal / reuse.
    {
        UnitRegistry reg;
        Unit blue;
        blue.team = 0;
        blue.tileX = 3;
        blue.tileZ = 4;
        const UnitHandle a = reg.Spawn(blue);
        Unit red;
        red.team = 1;
        const UnitHandle b = reg.Spawn(red);

        Expect(reg.AliveCount() == 2, "two units spawned");
        Expect(reg.TeamCount(0) == 1 && reg.TeamCount(1) == 1, "one per team");
        Expect(reg.Get(a) != nullptr && reg.Get(a)->tileX == 3, "handle resolves to the unit");

        reg.Remove(a);
        Expect(!reg.Alive(a) && reg.Get(a) == nullptr, "a removed handle is dead");
        Expect(reg.Alive(b), "the other unit is untouched");
        Expect(reg.AliveCount() == 1, "alive count drops");

        const UnitHandle c = reg.Spawn(blue);  // reuses slot 0
        Expect(c.index == a.index && !(c == a), "the slot is reused with a new generation");
        Expect(reg.Get(a) == nullptr && reg.Get(c) != nullptr, "the stale handle stays invalid");

        int seen = 0;
        reg.ForEach([&](UnitHandle, const Unit&) { ++seen; });
        Expect(seen == 2, "ForEach visits only the alive units");
    }

    // Facing helpers.
    {
        Expect(FacingTowards(5, 5, 5, 9) == Facing::South, "toward +Z is South");
        Expect(FacingTowards(5, 5, 9, 5) == Facing::East, "toward +X is East");
        Expect(FacingTowards(5, 5, 5, 1) == Facing::North, "toward -Z is North");
        Expect(FacingYaw(Facing::North) == 0.0F, "North yaw is zero");
    }

    // The arena hands out spawn tiles, all walkable and clear of the water.
    {
        BattleMap map;
        Expect(map.Spawns(0).size() == 3 && map.Spawns(1).size() == 3, "3v3 spawn tiles");
        for (int team = 0; team < 2; ++team) {
            for (const auto& [tx, tz] : map.Spawns(team)) {
                Expect(map.Grid().At(tx, tz).walkable, "every spawn tile is walkable");
            }
        }
    }

    if (failures == 0) {
        std::cout << "battle tile grid tests passed\n";
        return 0;
    }
    std::cerr << failures << " battle tile grid test(s) failed\n";
    return 1;
}
