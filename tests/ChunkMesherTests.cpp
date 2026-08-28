#include "world/AtlasDescriptor.hpp"
#include "world/Block.hpp"
#include "world/BlockAtlasBinding.hpp"
#include "world/BlockAtlasLayout.hpp"
#include "world/ChunkMesher.hpp"
#include "world/ChunkSection.hpp"

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

    if (failures != 0) {
        std::cerr << failures << " voxel test(s) failed\n";
        return 1;
    }
    std::cout << "Chunk section, atlas descriptor and greedy mesher tests passed\n";
    return 0;
}
