#include "render/BlockAtlas.hpp"

#include "world/BlockAtlasLayout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace voxelgame {
namespace {

std::uint32_t HashCoords(const int x, const int y, const std::uint32_t salt) noexcept {
    std::uint32_t h = salt + 0x9E3779B9U;
    h ^= static_cast<std::uint32_t>(x) * 0x85EBCA6BU;
    h = (h << 13) | (h >> 19);
    h ^= static_cast<std::uint32_t>(y) * 0xC2B2AE35U;
    h = (h << 17) | (h >> 15);
    h ^= h >> 16;
    h *= 0x7FEB352DU;
    h ^= h >> 15;
    return h;
}

// Deterministic value noise in [0, 1).
float NoiseAt(const int x, const int y, const std::uint32_t salt) noexcept {
    return static_cast<float>(HashCoords(x, y, salt) & 0xFFFFFFU) /
           static_cast<float>(0x1000000U);
}

std::uint8_t LerpByte(const std::uint8_t a, const std::uint8_t b, const float t) noexcept {
    const float clamped = std::clamp(t, 0.0F, 1.0F);
    return static_cast<std::uint8_t>(
        std::lround(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * clamped));
}

Color Mix(const Color a, const Color b, const float t) noexcept {
    return Color{LerpByte(a.r, b.r, t), LerpByte(a.g, b.g, t), LerpByte(a.b, b.b, t), 255};
}

Color GrassTopPixel(const int px, const int py) noexcept {
    const Color dark{74, 118, 46, 255};
    const Color light{122, 170, 74, 255};
    const float fine = NoiseAt(px, py, 101U);
    const float blocky = NoiseAt(px / 2, py / 2, 102U);
    Color c = Mix(dark, light, 0.35F * fine + 0.5F * blocky);
    if (NoiseAt(px, py, 103U) > 0.93F) {
        c = Mix(c, Color{150, 200, 110, 255}, 0.8F);
    }
    return c;
}

Color DirtPixel(const int px, const int py) noexcept {
    const Color dark{90, 60, 38, 255};
    const Color light{124, 86, 54, 255};
    const float fine = NoiseAt(px, py, 201U);
    const float blocky = NoiseAt(px / 2, py / 3, 202U);
    Color c = Mix(dark, light, 0.4F * fine + 0.45F * blocky);
    if (NoiseAt(px, py, 203U) > 0.9F) {
        c = Mix(c, Color{58, 38, 24, 255}, 0.7F);
    }
    return c;
}

Color GrassSidePixel(const int px, const int py) noexcept {
    int cap = 3;
    if (NoiseAt(px, 0, 301U) > 0.6F) {
        ++cap;
    }
    if (NoiseAt(px, 0, 302U) > 0.85F) {
        ++cap;
    }
    if (py < cap) {
        return GrassTopPixel(px, py);
    }
    Color c = DirtPixel(px, py);
    if (py == cap) {
        c = Mix(c, Color{0, 0, 0, 255}, 0.12F);
    }
    return c;
}

Color StonePixel(const int px, const int py) noexcept {
    const Color dark{104, 104, 110, 255};
    const Color light{140, 140, 146, 255};
    const float fine = NoiseAt(px, py, 401U);
    const float blocky = NoiseAt(px / 3, py / 3, 402U);
    Color c = Mix(dark, light, 0.5F * fine + 0.4F * blocky);
    if (NoiseAt(px, py, 403U) > 0.95F) {
        c = Mix(c, Color{70, 70, 76, 255}, 0.8F);
    }
    return c;
}

Color PixelFor(const int tile, const int px, const int py) noexcept {
    switch (tile) {
        case atlas::Tile::GrassTop:
            return GrassTopPixel(px, py);
        case atlas::Tile::GrassSide:
            return GrassSidePixel(px, py);
        case atlas::Tile::Dirt:
            return DirtPixel(px, py);
        default:
            return StonePixel(px, py);
    }
}

}  // namespace

Image GenerateBlockAtlasImage() {
    Image image = GenImageColor(atlas::Width, atlas::Height, BLANK);
    for (int tile = 0; tile < atlas::TileCount; ++tile) {
        const int originX = (tile % atlas::Columns) * atlas::TileSize;
        const int originY = (tile / atlas::Columns) * atlas::TileSize;
        for (int py = 0; py < atlas::TileSize; ++py) {
            for (int px = 0; px < atlas::TileSize; ++px) {
                ImageDrawPixel(&image, originX + px, originY + py, PixelFor(tile, px, py));
            }
        }
    }
    return image;
}

}  // namespace voxelgame
