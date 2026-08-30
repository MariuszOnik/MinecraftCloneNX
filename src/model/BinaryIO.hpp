#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <type_traits>

namespace voxelgame::vmodel::bin {

// Cap on any single length prefix we trust from a file, so a corrupt or hostile
// file cannot make us allocate gigabytes before we notice it is malformed.
inline constexpr std::uint32_t kMaxCount = 8u * 1024u * 1024u;

template <typename T>
void WritePod(std::ostream& out, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>, "WritePod needs a trivially copyable type");
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool ReadPod(std::istream& in, T& value) {
    static_assert(std::is_trivially_copyable_v<T>, "ReadPod needs a trivially copyable type");
    return static_cast<bool>(in.read(reinterpret_cast<char*>(&value), sizeof(T)));
}

inline void WriteString(std::ostream& out, const std::string& value) {
    WritePod(out, static_cast<std::uint32_t>(value.size()));
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
}

inline bool ReadString(std::istream& in, std::string& value) {
    std::uint32_t length = 0;
    if (!ReadPod(in, length) || length > kMaxCount) {
        return false;
    }
    value.resize(length);
    return length == 0 || static_cast<bool>(in.read(value.data(), length));
}

}  // namespace voxelgame::vmodel::bin
