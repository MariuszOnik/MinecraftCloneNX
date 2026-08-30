#include "model/VoxelModelBinary.hpp"

#include "model/BinaryIO.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <utility>

namespace voxelgame::vmodel {
namespace {

using namespace bin;

void WriteVec3(std::ostream& out, const Vec3 v) {
    WritePod(out, v.x);
    WritePod(out, v.y);
    WritePod(out, v.z);
}

bool ReadVec3(std::istream& in, Vec3& v) {
    return ReadPod(in, v.x) && ReadPod(in, v.y) && ReadPod(in, v.z);
}

}  // namespace

bool WriteVoxelModelBinary(std::ostream& out, const VoxelModel& model) {
    out.write(kVoxelModelMagic, sizeof(kVoxelModelMagic));
    WritePod(out, kVoxelModelVersion);
    WriteString(out, model.name);
    WritePod(out, model.voxelSize);

    WritePod(out, static_cast<std::uint32_t>(model.palette.size()));
    for (const ModelMaterial& material : model.palette) {
        WriteString(out, material.name);
        WritePod(out, material.red);
        WritePod(out, material.green);
        WritePod(out, material.blue);
        WritePod(out, material.alpha);
    }

    WritePod(out, static_cast<std::uint32_t>(model.parts.size()));
    for (const VoxelModelPart& part : model.parts) {
        WriteString(out, part.name);
        WritePod(out, static_cast<std::int32_t>(part.parent));
        WriteVec3(out, part.position);
        WriteVec3(out, part.pivot);
        WriteVec3(out, part.rotationDegrees);
        WritePod(out, static_cast<std::int32_t>(part.grid.sizeX));
        WritePod(out, static_cast<std::int32_t>(part.grid.sizeY));
        WritePod(out, static_cast<std::int32_t>(part.grid.sizeZ));
        WritePod(out, static_cast<std::uint32_t>(part.grid.voxels.size()));
        out.write(reinterpret_cast<const char*>(part.grid.voxels.data()),
                  static_cast<std::streamsize>(part.grid.voxels.size()));
    }
    return out.good();
}

bool WriteVoxelModelBinaryFile(const std::string& path, const VoxelModel& model) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    return out && WriteVoxelModelBinary(out, model);
}

std::optional<VoxelModel> ReadVoxelModelBinary(std::istream& in) {
    char magic[4] = {};
    std::uint32_t version = 0;
    if (!in.read(magic, sizeof(magic)) ||
        std::memcmp(magic, kVoxelModelMagic, sizeof(magic)) != 0) {
        return std::nullopt;
    }
    if (!ReadPod(in, version) || version != kVoxelModelVersion) {
        return std::nullopt;
    }

    VoxelModel model;
    std::uint32_t paletteCount = 0;
    if (!ReadString(in, model.name) || !ReadPod(in, model.voxelSize) || model.voxelSize <= 0.0F ||
        !ReadPod(in, paletteCount) || paletteCount == 0 || paletteCount > bin::kMaxCount) {
        return std::nullopt;
    }
    model.palette.reserve(paletteCount);
    for (std::uint32_t i = 0; i < paletteCount; ++i) {
        ModelMaterial material;
        if (!ReadString(in, material.name) || !ReadPod(in, material.red) ||
            !ReadPod(in, material.green) || !ReadPod(in, material.blue) ||
            !ReadPod(in, material.alpha)) {
            return std::nullopt;
        }
        model.palette.push_back(std::move(material));
    }

    std::uint32_t partCount = 0;
    if (!ReadPod(in, partCount) || partCount == 0 || partCount > bin::kMaxCount) {
        return std::nullopt;
    }
    model.parts.reserve(partCount);
    for (std::uint32_t i = 0; i < partCount; ++i) {
        VoxelModelPart part;
        std::int32_t parent = 0;
        std::int32_t sx = 0;
        std::int32_t sy = 0;
        std::int32_t sz = 0;
        std::uint32_t voxelCount = 0;
        if (!ReadString(in, part.name) || !ReadPod(in, parent) || !ReadVec3(in, part.position) ||
            !ReadVec3(in, part.pivot) || !ReadVec3(in, part.rotationDegrees) || !ReadPod(in, sx) ||
            !ReadPod(in, sy) || !ReadPod(in, sz) || !ReadPod(in, voxelCount)) {
            return std::nullopt;
        }
        if (sx <= 0 || sy <= 0 || sz <= 0 || parent >= static_cast<std::int32_t>(i)) {
            return std::nullopt;  // parts must reference an earlier parent
        }
        const std::uint64_t expected = static_cast<std::uint64_t>(sx) *
                                       static_cast<std::uint64_t>(sy) *
                                       static_cast<std::uint64_t>(sz);
        if (voxelCount != expected || voxelCount > bin::kMaxCount) {
            return std::nullopt;
        }
        part.parent = parent;
        part.grid.sizeX = sx;
        part.grid.sizeY = sy;
        part.grid.sizeZ = sz;
        part.grid.voxels.resize(voxelCount);
        if (voxelCount != 0 &&
            !in.read(reinterpret_cast<char*>(part.grid.voxels.data()), voxelCount)) {
            return std::nullopt;
        }
        model.parts.push_back(std::move(part));
    }
    return model;
}

std::optional<VoxelModel> ReadVoxelModelBinaryFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return ReadVoxelModelBinary(in);
}

}  // namespace voxelgame::vmodel
