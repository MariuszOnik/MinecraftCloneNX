#include "world/Block.hpp"
#include "world/BlockAtlasLayout.hpp"
#include "world/ChunkMesher.hpp"
#include "world/ChunkSection.hpp"

#include <cstddef>
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

void ExpectMesh(const voxelgame::MeshData& mesh, const std::size_t expectedQuads) {
    Expect(mesh.quadCount == expectedQuads, "unexpected quad count");
    Expect(mesh.VertexCount() == expectedQuads * 4, "each quad must have four vertices");
    Expect(mesh.TriangleCount() == expectedQuads * 2, "each quad must have two triangles");
    Expect(mesh.normals.size() == mesh.vertices.size(), "every vertex must have a normal");
    Expect(mesh.uvs.size() == mesh.VertexCount() * 2, "every vertex must have a UV pair");
    Expect(mesh.colors.size() == mesh.VertexCount() * 4, "every vertex must have RGBA color");
}

}  // namespace

int main() {
    using namespace voxelgame;

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
    ExpectMesh(mesher.Build(section), 6);

    // A lone grass block emits its six faces in the mesher's face order
    // (+X, -X, +Y, -Y, +Z, -Z). Check that each face samples the atlas tile the
    // block registry assigns to it.
    {
        ChunkSection lone;
        lone.Set(5, 5, 5, blocks::Grass);
        const MeshData grassMesh = mesher.Build(lone);
        const auto tileOfQuad = [&](const std::size_t quad) {
            const float u = grassMesh.uvs[quad * 8];  // first vertex U of the quad
            return static_cast<int>(u * static_cast<float>(atlas::Columns));
        };
        Expect(tileOfQuad(0) == atlas::Tile::GrassSide, "grass +X face uses the side tile");
        Expect(tileOfQuad(2) == atlas::Tile::GrassTop, "grass +Y face uses the top tile");
        Expect(tileOfQuad(3) == atlas::Tile::Dirt, "grass -Y face uses the dirt tile");
        for (const float coord : grassMesh.uvs) {
            Expect(coord >= 0.0F && coord <= 1.0F, "every UV stays inside the atlas");
        }
    }

    Expect(section.Set(2, 2, 3, blocks::Dirt), "second adjacent block");
    ExpectMesh(mesher.Build(section), 10);

    section.Fill(blocks::Stone);
    Expect(section.NonAirBlockCount() == ChunkSection::Volume, "filled section block count");
    ExpectMesh(mesher.Build(section), 6 * ChunkSection::Size * ChunkSection::Size);

    if (failures != 0) {
        std::cerr << failures << " voxel test(s) failed\n";
        return 1;
    }
    std::cout << "Chunk section and face-culling mesher tests passed\n";
    return 0;
}
