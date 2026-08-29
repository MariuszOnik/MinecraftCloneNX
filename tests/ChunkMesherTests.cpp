#include "world/AtlasDescriptor.hpp"
#include "world/Block.hpp"
#include "world/BlockAtlasBinding.hpp"
#include "world/BlockAtlasLayout.hpp"
#include "world/ChunkMesher.hpp"
#include "world/ChunkSection.hpp"
#include "world/PlayerBody.hpp"
#include "world/Raycast.hpp"
#include "world/TerrainGenerator.hpp"
#include "world/World.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void Expect(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

const voxelgame::MeshData& Opaque(const voxelgame::SectionMesh& mesh) {
    return mesh.Layer(voxelgame::RenderLayer::Opaque);
}

void ExpectMesh(const voxelgame::MeshData& mesh, const std::size_t expectedQuads) {
    Expect(mesh.quadCount == expectedQuads, "unexpected quad count");
    Expect(mesh.VertexCount() == expectedQuads * 4, "each quad must have four vertices");
    Expect(mesh.TriangleCount() == expectedQuads * 2, "each quad must have two triangles");
    Expect(mesh.normals.size() == mesh.vertices.size(), "every vertex must have a normal");
    Expect(mesh.uvs.size() == mesh.VertexCount() * 2, "every vertex must have a UV pair");
    Expect(mesh.tileOrigins.size() == mesh.VertexCount() * 2, "every vertex must have a tile origin");
    Expect(mesh.colors.size() == mesh.VertexCount() * 4, "every vertex must have RGBA color");
}

std::array<float, 3> QuadNormal(const voxelgame::MeshData& mesh, const std::size_t quad) {
    const std::size_t base = quad * 12;  // 4 vertices * 3
    return {mesh.normals[base], mesh.normals[base + 1], mesh.normals[base + 2]};
}

std::array<float, 2> QuadTileOrigin(const voxelgame::MeshData& mesh, const std::size_t quad) {
    const std::size_t base = quad * 8;  // 4 vertices * 2
    return {mesh.tileOrigins[base], mesh.tileOrigins[base + 1]};
}

bool Near(const float a, const float b) {
    return std::fabs(a - b) < 1.0e-4F;
}

// Finds the quad whose face normal is (nx, ny, nz) and checks its tile origin
// matches the given atlas tile.
void ExpectFaceTile(const voxelgame::MeshData& mesh, const float nx, const float ny,
                    const float nz, const voxelgame::atlas::TileRect& expected,
                    const std::string_view what) {
    for (std::size_t q = 0; q < mesh.quadCount; ++q) {
        const auto n = QuadNormal(mesh, q);
        if (Near(n[0], nx) && Near(n[1], ny) && Near(n[2], nz)) {
            const auto origin = QuadTileOrigin(mesh, q);
            Expect(Near(origin[0], expected.u0) && Near(origin[1], expected.v0), what);
            return;
        }
    }
    Expect(false, what);
}

}  // namespace

int main() {
    using namespace voxelgame;

    const BlockAtlasBinding defaultAtlas;

    Expect(ChunkSection::Index(0, 0, 0) == 0, "origin index");
    Expect(ChunkSection::Index(15, 0, 0) == 15, "x index");
    Expect(ChunkSection::Index(0, 0, 1) == 16, "z index");
    Expect(ChunkSection::Index(0, 1, 0) == 256, "y index");
    Expect(ChunkSection::Index(15, 15, 15) == ChunkSection::Volume - 1, "last index");

    Expect(!IsSolidBlock(blocks::Air), "air must not be solid");
    Expect(IsOccludingBlock(blocks::Grass), "grass must occlude opaque neighbors");
    Expect(GetBlockDefinition(blocks::Stone).name == "stone", "stone registry entry");
    Expect(!IsKnownBlock(999), "invalid block id must be detectable");

    ChunkMesher mesher;
    ChunkSection section;
    Expect(section.Get(-1, 0, 0) == blocks::Air, "outside lookup returns air");
    Expect(!section.Set(-1, 0, 0, blocks::Stone), "outside write is rejected");
    Expect(section.IsMeshDirty(), "new section requires initial mesh");
    section.MarkMeshClean();
    Expect(!section.IsMeshDirty(), "mesh can be marked clean after upload");
    Expect(section.Set(1, 2, 3, blocks::Stone), "setting a new value changes the section");
    Expect(section.IsDataDirty() && section.IsMeshDirty(), "block change sets dirty flags");
    Expect(!section.Set(1, 2, 3, blocks::Stone), "setting the same value is a no-op");
    Expect(section.NonAirBlockCount() == 1, "non-air count after one block");
    ExpectMesh(Opaque(mesher.Build(section, defaultAtlas)), 6);

    // A lone grass block: six faces, none mergeable. Each face samples the tile
    // the registry assigns to it, regardless of emission order.
    {
        ChunkSection lone;
        lone.Set(5, 5, 5, blocks::Grass);
        const MeshData grass = Opaque(mesher.Build(lone, defaultAtlas));
        ExpectMesh(grass, 6);
        ExpectFaceTile(grass, 0, 1, 0, atlas::TileRectOf(atlas::Tile::GrassTop),
                       "grass top face samples the grass-top tile");
        ExpectFaceTile(grass, 0, -1, 0, atlas::TileRectOf(atlas::Tile::Dirt),
                       "grass bottom face samples the dirt tile");
        ExpectFaceTile(grass, 1, 0, 0, atlas::TileRectOf(atlas::Tile::GrassSide),
                       "grass +X face samples the grass-side tile");
        ExpectFaceTile(grass, 0, 0, -1, atlas::TileRectOf(atlas::Tile::GrassSide),
                       "grass -Z face samples the grass-side tile");
        for (std::size_t k = 0; k + 1 < grass.uvs.size(); k += 2) {
            Expect(grass.uvs[k] >= 0.0F && grass.uvs[k] <= 1.0F + 1.0e-3F,
                   "1x1 face UV stays within one tile span");
        }
    }

    // Greedy merge: a stone + dirt pair keeps 10 quads (different tiles never
    // merge), but two dirt blocks collapse their coplanar faces.
    Expect(section.Set(2, 2, 3, blocks::Dirt), "second adjacent block");
    ExpectMesh(Opaque(mesher.Build(section, defaultAtlas)), 10);
    {
        ChunkSection pair;
        pair.Set(1, 2, 3, blocks::Dirt);
        pair.Set(2, 2, 3, blocks::Dirt);
        ExpectMesh(Opaque(mesher.Build(pair, defaultAtlas)), 6);  // 2 ends + merged faces
    }

    // The headline greedy result: a solid section is six full-face quads.
    section.Fill(blocks::Stone);
    Expect(section.NonAirBlockCount() == ChunkSection::Volume, "filled section block count");
    {
        const MeshData solid = Opaque(mesher.Build(section, defaultAtlas));
        ExpectMesh(solid, 6);
        // Every quad spans the whole 16x16 face.
        for (std::size_t q = 0; q < solid.quadCount; ++q) {
            float maxU = 0.0F;
            for (std::size_t c = 0; c < 4; ++c) {
                maxU = std::fmax(maxU, solid.uvs[q * 8 + c * 2]);
            }
            Expect(maxU > 15.0F, "merged section face tiles ~16 times across");
        }
    }

    // Atlas descriptor: a 2x2 / 32px grid overrides the grid and face mapping.
    {
        const std::string json = R"({
            "texture": "pack.png",
            "atlasSize": [32, 32],
            "tileSize": 16,
            "tiles": { "a": [0, 0], "b": [1, 0], "c": [0, 1], "d": [1, 1] },
            "blocks": {
                "stone": "d",
                "grass": { "top": "a", "bottom": "c", "sides": "b" }
            }
        })";
        std::string error;
        const std::optional<AtlasDescriptor> descriptor = ParseAtlasDescriptor(json, error);
        Expect(descriptor.has_value(), "valid descriptor parses");
        if (descriptor) {
            Expect(descriptor->texture == "pack.png", "descriptor texture name");
            Expect(descriptor->columns == 2 && descriptor->rows == 2, "descriptor grid size");

            BlockAtlasBinding binding;
            binding.Apply(*descriptor);
            Expect(binding.Columns() == 2 && binding.Rows() == 2, "binding adopts descriptor grid");
            Expect(binding.FaceTile(blocks::Stone, 2) == 3, "stone -> tile d (index 3)");
            Expect(binding.FaceTile(blocks::Grass, 2) == 0, "grass top -> tile a (index 0)");
            Expect(binding.FaceTile(blocks::Grass, 3) == 2, "grass bottom -> tile c (index 2)");
            Expect(binding.FaceTile(blocks::Grass, 0) == 1, "grass side -> tile b (index 1)");

            ChunkSection one;
            one.Set(0, 0, 0, blocks::Grass);
            const MeshData mesh = Opaque(mesher.Build(one, binding));
            ExpectFaceTile(mesh, 0, 1, 0, binding.FaceRect(blocks::Grass, 2),
                           "descriptor: grass top face origin");
            ExpectFaceTile(mesh, 0, -1, 0, binding.FaceRect(blocks::Grass, 3),
                           "descriptor: grass bottom face origin");
        }

        std::string badError;
        Expect(!ParseAtlasDescriptor("{ not json", badError).has_value(),
               "malformed descriptor is rejected");
        Expect(!ParseAtlasDescriptor(
                    R"({"texture":"x","atlasSize":[16,16],"tileSize":5,"tiles":{},"blocks":{}})",
                    badError)
                    .has_value(),
               "tileSize that does not divide the atlas is rejected");
        Expect(ParseAtlasDescriptor(
                   R"({"texture":"x","atlasSize":[36,18],"tileSize":16,"padding":1,
                       "tiles":{},"blocks":{}})",
                   badError)
                   .has_value(),
               "padded atlas size is accepted");
    }

    // World: streamed columns, negative coordinates, boundary dirty propagation.
    {
        World world(1);
        Expect(world.BlocksY() == 16, "vertical extent");
        Expect(world.GetBlock(3, 3, 3) == blocks::Air, "unloaded column reads as air");
        Expect(!world.SetBlock(3, 3, 3, blocks::Stone), "write to an unloaded column is rejected");

        Expect(world.EnsureColumn(0, 0), "column loads");
        Expect(!world.EnsureColumn(0, 0), "loading a loaded column is a no-op");
        world.EnsureColumn(1, 0);
        world.EnsureColumn(-1, -1);

        Expect(world.SetBlock(5, 5, 5, blocks::Stone), "in-column write");
        Expect(world.GetBlock(5, 5, 5) == blocks::Stone, "write is visible via GetBlock");
        Expect(world.SetBlock(-1, 3, -5, blocks::Dirt), "negative-coordinate write");
        Expect(world.GetBlock(-1, 3, -5) == blocks::Dirt, "negative-coordinate read");

        world.MarkSectionMeshClean(0, 0, 0);
        world.MarkSectionMeshClean(1, 0, 0);
        Expect(world.SetBlock(15, 8, 8, blocks::Stone), "edit on the column-0/1 boundary");
        Expect(world.SectionMeshDirty(0, 0, 0) && world.SectionMeshDirty(1, 0, 0),
               "a column-boundary edit dirties the neighbour column too");

        world.MarkSectionMeshClean(1, 0, 0);
        Expect(world.EnsureColumn(2, 0), "another column loads");
        Expect(world.SectionMeshDirty(1, 0, 0),
               "loading a column re-dirties its already-loaded neighbours");

        const std::size_t before = world.LoadedColumnCount();
        world.UnloadColumn(0, 0);
        Expect(!world.IsColumnLoaded(0, 0), "column unloaded");
        Expect(world.LoadedColumnCount() == before - 1, "loaded count drops on unload");
        Expect(world.GetBlock(5, 5, 5) == blocks::Air, "unloaded blocks read as air");
    }

    // Cross-column meshing: a shared face between two solid columns is culled.
    {
        World solid(1);
        solid.EnsureColumn(0, 0);
        solid.EnsureColumn(1, 0);
        for (int y = 0; y < ChunkSection::Size; ++y) {
            for (int z = 0; z < ChunkSection::Size; ++z) {
                for (int x = 0; x < 2 * ChunkSection::Size; ++x) {
                    solid.SetBlock(x, y, z, blocks::Stone);
                }
            }
        }
        const MeshData left = Opaque(mesher.Build(solid, 0, 0, 0, defaultAtlas));
        ExpectMesh(left, 5);  // +X face is culled by the neighbouring column
        for (std::size_t q = 0; q < left.quadCount; ++q) {
            Expect(!Near(QuadNormal(left, q)[0], 1.0F),
                   "no +X face where a solid neighbour column abuts");
        }
        ChunkSection lone;
        lone.Fill(blocks::Stone);
        ExpectMesh(Opaque(mesher.Build(lone, defaultAtlas)), 6);  // isolation keeps six faces
    }

    // Render layers: leaves -> cutout, glass -> transparent, water -> liquid,
    // and identical non-opaque neighbours hide the face between them.
    {
        ChunkSection s;
        s.Set(4, 4, 4, blocks::Leaves);
        s.Set(6, 4, 4, blocks::Glass);
        s.Set(8, 4, 4, blocks::Water);
        s.Set(10, 4, 4, blocks::GlassRed);
        const SectionMesh m = mesher.Build(s, defaultAtlas);
        Expect(m.Layer(RenderLayer::Opaque).Empty(), "no opaque geometry here");
        Expect(m.Layer(RenderLayer::Cutout).quadCount == 6, "leaves -> 6 cutout faces");
        Expect(m.Layer(RenderLayer::Liquid).quadCount == 6, "water -> 6 liquid faces");
        Expect(m.Layer(RenderLayer::Transparent).quadCount == 12,
               "glass + stained glass -> 12 transparent faces");

        // Two glass blocks: the shared face is culled and the four coplanar
        // faces merge -> 6 quads, exactly like an opaque pair.
        ChunkSection glassPair;
        glassPair.Set(3, 3, 3, blocks::Glass);
        glassPair.Set(4, 3, 3, blocks::Glass);
        Expect(mesher.Build(glassPair, defaultAtlas).Layer(RenderLayer::Transparent).quadCount == 6,
               "glass-glass hides the shared face");

        // A glass block next to leaves (different non-opaque block) keeps its face.
        ChunkSection glassLeaves;
        glassLeaves.Set(3, 3, 3, blocks::Glass);
        glassLeaves.Set(4, 3, 3, blocks::Leaves);
        Expect(mesher.Build(glassLeaves, defaultAtlas).Layer(RenderLayer::Transparent).quadCount == 6,
               "glass keeps its face against a different non-opaque neighbour");

        // A thin pane skips the cube greedy pass and emits a 4-quad cross.
        ChunkSection pane;
        pane.Set(8, 8, 8, blocks::GlassPane);
        const SectionMesh pm = mesher.Build(pane, defaultAtlas);
        Expect(pm.Layer(RenderLayer::Opaque).Empty() && pm.Layer(RenderLayer::Cutout).Empty(),
               "a pane has no cube geometry");
        Expect(pm.Layer(RenderLayer::Transparent).quadCount == 4, "pane cross is 4 quads");
        // A cube next to a pane still shows its face (the pane does not occlude).
        ChunkSection cubePane;
        cubePane.Set(3, 3, 3, blocks::Stone);
        cubePane.Set(4, 3, 3, blocks::GlassPane);
        Expect(mesher.Build(cubePane, defaultAtlas).Layer(RenderLayer::Opaque).quadCount == 6,
               "stone keeps all six faces next to a pane");
    }

    // PlayerBody: gravity/landing, walls, jumping.
    {
        World ground(1);
        ground.EnsureColumn(0, 0);
        for (int z = 0; z < ChunkSection::Size; ++z) {
            for (int x = 0; x < ChunkSection::Size; ++x) {
                ground.SetBlock(x, 0, z, blocks::Stone);
            }
        }

        PlayerBody body(Vec3{8.0F, 6.0F, 8.0F});
        for (int step = 0; step < 240; ++step) {
            body.Step(ground, Vec3{0.0F, 0.0F, 0.0F}, false, 1.0F / 60.0F);
        }
        Expect(body.OnGround(), "player lands on the floor");
        Expect(std::fabs(body.Position().y - 1.0F) < 0.05F,
               "player rests on top of the y=0 block");

        // Walk east into a wall at x = 11.
        World walled(1);
        walled.EnsureColumn(0, 0);
        for (int y = 0; y < ChunkSection::Size; ++y) {
            for (int z = 0; z < ChunkSection::Size; ++z) {
                for (int x = 0; x < ChunkSection::Size; ++x) {
                    if (y == 0 || x == 11) {
                        walled.SetBlock(x, y, z, blocks::Stone);
                    }
                }
            }
        }
        PlayerBody walker(Vec3{6.0F, 1.0F, 8.0F});
        for (int step = 0; step < 240; ++step) {
            walker.Step(walled, Vec3{6.0F, 0.0F, 0.0F}, false, 1.0F / 60.0F);
        }
        Expect(walker.Position().x < 11.0F - PlayerBody::Width * 0.5F + 0.02F,
               "player stops at the wall instead of passing through");

        // Water: the player sinks slowly (not at fall speed), and a jump at the
        // surface leaps them clear so they can climb onto land.
        World lake(1);
        lake.EnsureColumn(0, 0);
        for (int y = 0; y < 6; ++y) {
            for (int z = 0; z < ChunkSection::Size; ++z) {
                for (int x = 0; x < ChunkSection::Size; ++x) {
                    lake.SetBlock(x, y, z, y == 0 ? blocks::Stone : blocks::Water);
                }
            }
        }
        PlayerBody swimmer(Vec3{8.0F, 5.0F, 8.0F});
        swimmer.Step(lake, Vec3{}, false, 1.0F / 60.0F);
        Expect(swimmer.InWater(), "player detects being in water");
        for (int step = 0; step < 60; ++step) {
            swimmer.Step(lake, Vec3{}, false, 1.0F / 60.0F);
        }
        Expect(swimmer.VerticalVelocity() > -3.5F, "the swimmer sinks slowly, not at fall speed");
        Expect(swimmer.Position().y > 1.0F, "the swimmer has not dropped like a stone");
        for (int step = 0; step < 45; ++step) {
            swimmer.Step(lake, Vec3{}, true, 1.0F / 60.0F);
        }
        Expect(swimmer.Position().y > 5.2F && !swimmer.InWater(),
               "jumping at the surface clears the water");

        // Jump only works from the ground.
        PlayerBody jumper(Vec3{8.0F, 1.0F, 8.0F});
        jumper.Step(walled, Vec3{}, false, 1.0F / 60.0F);
        Expect(jumper.OnGround(), "jumper is grounded before jumping");
        jumper.Step(walled, Vec3{}, true, 1.0F / 60.0F);
        Expect(jumper.VerticalVelocity() > 0.0F && !jumper.OnGround(), "jump lifts the player");
        const float airborne = jumper.VerticalVelocity();
        jumper.Step(walled, Vec3{}, true, 1.0F / 60.0F);
        Expect(jumper.VerticalVelocity() < airborne, "no mid-air second jump");
    }

    // Raycast: DDA against solid blocks with entry-face normals and reach limit.
    {
        World scene(1);
        scene.EnsureColumn(0, 0);
        for (int z = 0; z < ChunkSection::Size; ++z) {
            for (int x = 0; x < ChunkSection::Size; ++x) {
                scene.SetBlock(x, 0, z, blocks::Stone);          // floor
            }
        }
        for (int y = 0; y < ChunkSection::Size; ++y) {
            for (int z = 0; z < ChunkSection::Size; ++z) {
                scene.SetBlock(11, y, z, blocks::Stone);          // wall at x = 11
            }
        }

        const RaycastHit down = Raycast(scene, Vec3{8.5F, 5.0F, 8.5F}, Vec3{0.0F, -1.0F, 0.0F}, 10.0F);
        Expect(down.hit && down.blockX == 8 && down.blockY == 0 && down.blockZ == 8,
               "ray down hits the floor block");
        Expect(down.normalX == 0 && down.normalY == 1 && down.normalZ == 0,
               "floor hit normal points up");

        const RaycastHit up = Raycast(scene, Vec3{8.5F, 5.0F, 8.5F}, Vec3{0.0F, 1.0F, 0.0F}, 10.0F);
        Expect(!up.hit, "ray into open air misses");

        const RaycastHit wall =
            Raycast(scene, Vec3{8.5F, 5.5F, 8.5F}, Vec3{1.0F, 0.0F, 0.0F}, 10.0F);
        Expect(wall.hit && wall.blockX == 11 && wall.normalX == -1,
               "ray east hits the wall on its -X face");

        const RaycastHit tooFar =
            Raycast(scene, Vec3{8.5F, 5.5F, 8.5F}, Vec3{1.0F, 0.0F, 0.0F}, 2.0F);
        Expect(!tooFar.hit, "reach limit stops the ray short of the wall");
    }

    // TerrainGenerator: determinism, seed sensitivity, layered columns.
    {
        constexpr int span = 2 * ChunkSection::Size;  // a 2x2 chunk area
        const auto fill = [](std::uint32_t seed) {
            World w(2, [seed](World& ww, int cx, int cz) {
                TerrainGenerator(seed).FillColumn(ww, cx, cz);
            });
            for (int cz = 0; cz < 2; ++cz) {
                for (int cx = 0; cx < 2; ++cx) {
                    w.EnsureColumn(cx, cz);
                }
            }
            return w;
        };

        World a = fill(42);
        World b = fill(42);
        World c = fill(43);
        bool identical = true;
        bool differsFromOtherSeed = false;
        for (int y = 0; y < a.BlocksY() && identical; ++y) {
            for (int z = 0; z < span; ++z) {
                for (int x = 0; x < span; ++x) {
                    if (a.GetBlock(x, y, z) != b.GetBlock(x, y, z)) {
                        identical = false;
                    }
                    if (a.GetBlock(x, y, z) != c.GetBlock(x, y, z)) {
                        differsFromOtherSeed = true;
                    }
                }
            }
        }
        Expect(identical, "same seed generates an identical world");
        Expect(differsFromOtherSeed, "a different seed generates different terrain");

        const TerrainGenerator gen(42);
        int checkedColumns = 0;
        for (int z = 1; z < span; z += 5) {
            for (int x = 1; x < span; x += 5) {
                const int surface = gen.SurfaceHeight(x, z);
                Expect(surface >= 1 && surface < a.BlocksY(), "surface height stays in bounds");
                Expect(a.GetBlock(x, 0, z) == blocks::Bedrock, "column floor is bedrock");
                const BlockId top = a.GetBlock(x, surface, z);
                Expect(top == blocks::Grass || top == blocks::Sand,
                       "column surface is grass or sand");
                const BlockId above = a.GetBlock(x, surface + 1, z);
                Expect(above == blocks::Air || above == blocks::Wood || above == blocks::Water,
                       "above the surface is air, a trunk, or water");
                Expect(IsCollidableBlock(a.GetBlock(x, surface - 1, z)),
                       "solid ground directly below the surface");

                if (surface < TerrainGenerator::SeaLevel) {
                    Expect(a.GetBlock(x, TerrainGenerator::SeaLevel, z) == blocks::Water,
                           "a submerged column has water up to sea level");
                    Expect(top == blocks::Sand, "a submerged column's bed is sand");
                }
                ++checkedColumns;
            }
        }
        Expect(checkedColumns > 0, "sampled at least one column");
    }

    if (failures != 0) {
        std::cerr << failures << " voxel test(s) failed\n";
        return 1;
    }
    std::cout << "Chunk section, world, player, raycast, terrain and mesher tests passed\n";
    return 0;
}
