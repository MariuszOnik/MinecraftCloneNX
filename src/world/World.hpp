#pragma once

#include "world/Block.hpp"
#include "world/ChunkSection.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace voxelgame {

// An X/Z-unbounded voxel world of 16^3 sections, `SectionsY` sections tall.
// Columns at (chunkX, chunkZ) are streamed: loaded (and filled by the generator
// callback) on demand, and unloaded when far from the player.
class World {
public:
    // Fills a freshly loaded column: called with the column's (chunkX, chunkZ);
    // the callback writes blocks through world.SetBlock within that column.
    using ColumnFiller = std::function<void(World& world, int chunkX, int chunkZ)>;
    using SectionVisitor = std::function<void(int chunkX, int sectionY, int chunkZ)>;
    using ColumnVisitor = std::function<void(int chunkX, int chunkZ)>;

    explicit World(int sectionsY, ColumnFiller filler = {});

    [[nodiscard]] int SectionsY() const noexcept { return sectionsY_; }
    [[nodiscard]] int BlocksY() const noexcept { return sectionsY_ * ChunkSection::Size; }

    [[nodiscard]] static constexpr int ToChunk(int block) noexcept { return block >> 4; }
    [[nodiscard]] static constexpr int ToLocal(int block) noexcept { return block & 15; }

    // World-block access. Reads outside the vertical range or in an unloaded
    // column return Air; writes there are rejected.
    [[nodiscard]] BlockId GetBlock(int x, int y, int z) const noexcept;
    bool SetBlock(int x, int y, int z, BlockId block) noexcept;

    [[nodiscard]] bool IsColumnLoaded(int chunkX, int chunkZ) const noexcept;
    // Loads and fills the column if absent; returns true when newly loaded.
    bool EnsureColumn(int chunkX, int chunkZ);
    void UnloadColumn(int chunkX, int chunkZ);

    [[nodiscard]] std::size_t LoadedColumnCount() const noexcept { return columns_.size(); }
    [[nodiscard]] std::size_t LoadedSectionCount() const noexcept {
        return columns_.size() * static_cast<std::size_t>(sectionsY_);
    }

    void ForEachLoadedColumn(const ColumnVisitor& fn) const;
    void ForEachLoadedSection(const SectionVisitor& fn) const;

    [[nodiscard]] bool SectionMeshDirty(int chunkX, int sectionY, int chunkZ) const noexcept;
    void MarkSectionMeshClean(int chunkX, int sectionY, int chunkZ) noexcept;

    [[nodiscard]] std::size_t NonAirBlockCount() const noexcept;

private:
    struct Column {
        std::vector<ChunkSection> sections;
        int chunkX = 0;
        int chunkZ = 0;
    };

    [[nodiscard]] static std::int64_t Key(int chunkX, int chunkZ) noexcept {
        return (static_cast<std::int64_t>(chunkX) << 32) |
               static_cast<std::int64_t>(static_cast<std::uint32_t>(chunkZ));
    }
    [[nodiscard]] const Column* Find(int chunkX, int chunkZ) const noexcept;
    [[nodiscard]] Column* Find(int chunkX, int chunkZ) noexcept;

    int sectionsY_;
    ColumnFiller filler_;
    std::unordered_map<std::int64_t, Column> columns_;
};

}  // namespace voxelgame
