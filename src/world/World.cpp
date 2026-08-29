#include "world/World.hpp"

#include <algorithm>
#include <utility>

namespace voxelgame {

static_assert(ChunkSection::Size == 16, "World coordinate math assumes 16-wide sections");

World::World(const int sectionsY, ColumnFiller filler)
    : sectionsY_(std::max(sectionsY, 1)), filler_(std::move(filler)) {}

const World::Column* World::Find(const int chunkX, const int chunkZ) const noexcept {
    const auto it = columns_.find(Key(chunkX, chunkZ));
    return it == columns_.end() ? nullptr : &it->second;
}

World::Column* World::Find(const int chunkX, const int chunkZ) noexcept {
    const auto it = columns_.find(Key(chunkX, chunkZ));
    return it == columns_.end() ? nullptr : &it->second;
}

bool World::IsColumnLoaded(const int chunkX, const int chunkZ) const noexcept {
    return columns_.count(Key(chunkX, chunkZ)) != 0;
}

bool World::EnsureColumn(const int chunkX, const int chunkZ) {
    const std::int64_t key = Key(chunkX, chunkZ);
    if (columns_.count(key) != 0) {
        return false;
    }
    Column& column = columns_[key];
    column.chunkX = chunkX;
    column.chunkZ = chunkZ;
    column.sections.resize(static_cast<std::size_t>(sectionsY_));
    if (filler_) {
        journalling_ = false;
        filler_(*this, chunkX, chunkZ);
        journalling_ = true;
    }
    ApplyEditsInColumn(chunkX, chunkZ);
    // A new column exposes faces on its already-loaded neighbours.
    for (int sy = 0; sy < sectionsY_; ++sy) {
        if (Column* n = Find(chunkX - 1, chunkZ)) n->sections[sy].MarkMeshDirty();
        if (Column* n = Find(chunkX + 1, chunkZ)) n->sections[sy].MarkMeshDirty();
        if (Column* n = Find(chunkX, chunkZ - 1)) n->sections[sy].MarkMeshDirty();
        if (Column* n = Find(chunkX, chunkZ + 1)) n->sections[sy].MarkMeshDirty();
    }
    return true;
}

void World::UnloadColumn(const int chunkX, const int chunkZ) {
    if (columns_.erase(Key(chunkX, chunkZ)) != 0) {
        for (int sy = 0; sy < sectionsY_; ++sy) {
            if (Column* n = Find(chunkX - 1, chunkZ)) n->sections[sy].MarkMeshDirty();
            if (Column* n = Find(chunkX + 1, chunkZ)) n->sections[sy].MarkMeshDirty();
            if (Column* n = Find(chunkX, chunkZ - 1)) n->sections[sy].MarkMeshDirty();
            if (Column* n = Find(chunkX, chunkZ + 1)) n->sections[sy].MarkMeshDirty();
        }
    }
}

BlockId World::GetBlock(const int x, const int y, const int z) const noexcept {
    if (y < 0 || y >= BlocksY()) {
        return blocks::Air;
    }
    const Column* column = Find(ToChunk(x), ToChunk(z));
    if (column == nullptr) {
        return blocks::Air;
    }
    return column->sections[static_cast<std::size_t>(y >> 4)].Get(ToLocal(x), y & 15, ToLocal(z));
}

bool World::SetBlock(const int x, const int y, const int z, const BlockId block) noexcept {
    if (y < 0 || y >= BlocksY()) {
        return false;
    }
    const int cx = ToChunk(x);
    const int cz = ToChunk(z);
    Column* column = Find(cx, cz);
    if (column == nullptr) {
        return false;
    }
    const int lx = ToLocal(x);
    const int lz = ToLocal(z);
    const int sy = y >> 4;
    const int ly = y & 15;
    if (!column->sections[static_cast<std::size_t>(sy)].Set(lx, ly, lz, block)) {
        return false;
    }
    if (journalling_) {
        edits_[BlockKey(x, y, z)] = block;
    }

    const auto dirty = [&](const int ncx, const int nsy, const int ncz) {
        if (nsy < 0 || nsy >= sectionsY_) {
            return;
        }
        if (Column* n = Find(ncx, ncz)) {
            n->sections[static_cast<std::size_t>(nsy)].MarkMeshDirty();
        }
    };
    if (lx == 0) dirty(cx - 1, sy, cz);
    if (lx == 15) dirty(cx + 1, sy, cz);
    if (lz == 0) dirty(cx, sy, cz - 1);
    if (lz == 15) dirty(cx, sy, cz + 1);
    if (ly == 0) dirty(cx, sy - 1, cz);
    if (ly == 15) dirty(cx, sy + 1, cz);
    return true;
}

void World::ForEachLoadedColumn(const ColumnVisitor& fn) const {
    for (const auto& [key, column] : columns_) {
        (void)key;
        fn(column.chunkX, column.chunkZ);
    }
}

void World::ForEachLoadedSection(const SectionVisitor& fn) const {
    for (const auto& [key, column] : columns_) {
        (void)key;
        for (int sy = 0; sy < sectionsY_; ++sy) {
            fn(column.chunkX, sy, column.chunkZ);
        }
    }
}

bool World::SectionMeshDirty(const int chunkX, const int sectionY, const int chunkZ) const noexcept {
    const Column* column = Find(chunkX, chunkZ);
    if (column == nullptr || sectionY < 0 || sectionY >= sectionsY_) {
        return false;
    }
    return column->sections[static_cast<std::size_t>(sectionY)].IsMeshDirty();
}

void World::MarkSectionMeshClean(const int chunkX, const int sectionY, const int chunkZ) noexcept {
    if (Column* column = Find(chunkX, chunkZ)) {
        if (sectionY >= 0 && sectionY < sectionsY_) {
            column->sections[static_cast<std::size_t>(sectionY)].MarkMeshClean();
        }
    }
}

std::int64_t World::BlockKey(const int x, const int y, const int z) noexcept {
    // 21 bits X, 21 bits Z (both biased), 12 bits Y (0..4095) -> one 64-bit key.
    const std::int64_t bx = (static_cast<std::int64_t>(x) + (1 << 20)) & 0x1FFFFF;
    const std::int64_t bz = (static_cast<std::int64_t>(z) + (1 << 20)) & 0x1FFFFF;
    const std::int64_t by = static_cast<std::int64_t>(y) & 0xFFF;
    return (bx << 33) | (bz << 12) | by;
}

void World::DecodeKey(const std::int64_t key, int& x, int& y, int& z) noexcept {
    x = static_cast<int>((key >> 33) & 0x1FFFFF) - (1 << 20);
    z = static_cast<int>((key >> 12) & 0x1FFFFF) - (1 << 20);
    y = static_cast<int>(key & 0xFFF);
}

void World::AddEdit(const int x, const int y, const int z, const BlockId block) {
    edits_[BlockKey(x, y, z)] = block;
    const int cx = ToChunk(x);
    const int cz = ToChunk(z);
    if (IsColumnLoaded(cx, cz)) {
        journalling_ = false;
        SetBlock(x, y, z, block);
        journalling_ = true;
    }
}

void World::ApplyEditsInColumn(const int chunkX, const int chunkZ) {
    if (edits_.empty()) {
        return;
    }
    journalling_ = false;
    for (const auto& [key, block] : edits_) {
        const int x = static_cast<int>((key >> 33) & 0x1FFFFF) - (1 << 20);
        const int z = static_cast<int>((key >> 12) & 0x1FFFFF) - (1 << 20);
        if (ToChunk(x) == chunkX && ToChunk(z) == chunkZ) {
            SetBlock(x, static_cast<int>(key & 0xFFF), z, block);
        }
    }
    journalling_ = true;
}

std::size_t World::NonAirBlockCount() const noexcept {
    std::size_t total = 0;
    for (const auto& [key, column] : columns_) {
        (void)key;
        for (const ChunkSection& section : column.sections) {
            total += section.NonAirBlockCount();
        }
    }
    return total;
}

}  // namespace voxelgame
