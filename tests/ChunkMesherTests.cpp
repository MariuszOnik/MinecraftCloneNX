#include "world/AtlasDescriptor.hpp"
#include "world/Block.hpp"
#include "world/BlockAtlasBinding.hpp"
#include "world/BlockAtlasLayout.hpp"
#include "world/ChunkMesher.hpp"
#include "world/ChunkSection.hpp"
#include "world/PlayerBody.hpp"
#include "world/Raycast.hpp"
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
    ExpectMesh(mesher.Build(section, defaultAtlas), 6);

    // A lone grass block: six faces, none mergeable. Each face samples the tile
    // the registry assigns to it, regardless of emission order.
    {
        ChunkSection lone;
        lone.Set(5, 5, 5, blocks::Grass);
        const MeshData grass = mesher.Build(lone, defaultAtlas);
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
    ExpectMesh(mesher.Build(section, defaultAtlas), 10);
    {
        ChunkSection pair;
        pair.Set(1, 2, 3, blocks::Dirt);
        pair.Set(2, 2, 3, blocks::Dirt);
        ExpectMesh(mesher.Build(pair, defaultAtlas), 6);  // 2 ends + merged top/bottom/front/back
    }

    // The headline greedy result: a solid section is six full-face quads.
    section.Fill(blocks::Stone);
    Expect(section.NonAirBlockCount() == ChunkSection::Volume, "filled section block count");
    {
        const MeshData solid = mesher.Build(section, defaultAtlas);
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
            const MeshData mesh = mesher.Build(one, binding);
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

    // World: block access, boundary dirty propagation, cross-section culling.
    {
        World world(2, 1, 1);  // 32 x 16 x 16 blocks
        Expect(world.BlocksX() == 32 && world.BlocksY() == 16 && world.BlocksZ() == 16,
               "world dimensions");
        Expect(world.GetBlock(-1, 0, 0) == blocks::Air, "out-of-world read is air");
        Expect(!world.SetBlock(40, 0, 0, blocks::Stone), "out-of-world write is rejected");

        Expect(world.SetBlock(20, 5, 5, blocks::Stone), "in-world write");
        Expect(world.GetBlock(20, 5, 5) == blocks::Stone, "write is visible via GetBlock");
        Expect(world.SectionMeshDirty(1, 0, 0), "written section is dirty");

        world.MarkSectionMeshClean(0, 0, 0);
        world.MarkSectionMeshClean(1, 0, 0);
        // Local x == 0 of section 1 (world x == 16) touches section 0's boundary.
        Expect(world.SetBlock(16, 8, 8, blocks::Stone), "boundary write");
        Expect(world.SectionMeshDirty(0, 0, 0) && world.SectionMeshDirty(1, 0, 0),
               "a boundary edit dirties both sections");

        World solid(2, 1, 1);
        for (int y = 0; y < ChunkSection::Size; ++y) {
            for (int z = 0; z < ChunkSection::Size; ++z) {
                for (int x = 0; x < solid.BlocksX(); ++x) {
                    solid.SetBlock(x, y, z, blocks::Stone);
                }
            }
        }
        const MeshData left = mesher.Build(solid, 0, 0, 0, defaultAtlas);
        ExpectMesh(left, 5);  // +X face is culled by the neighbouring section
        for (std::size_t q = 0; q < left.quadCount; ++q) {
            const auto n = QuadNormal(left, q);
            Expect(!Near(n[0], 1.0F), "no +X face where a solid neighbour section abuts");
        }
        // In isolation the same section keeps all six faces.
        ExpectMesh(mesher.Build(solid.SectionAt(0, 0, 0), defaultAtlas), 6);
    }

    // PlayerBody: gravity/landing, walls, jumping.
    {
        World ground(1, 1, 1);
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

        // Walk east into a wall column at x = 11.
        World walled(1, 1, 1);
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
        World scene(1, 1, 1);
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

    if (failures != 0) {
        std::cerr << failures << " voxel test(s) failed\n";
        return 1;
    }
    std::cout << "Chunk section, world, player, raycast and greedy mesher tests passed\n";
    return 0;
}
