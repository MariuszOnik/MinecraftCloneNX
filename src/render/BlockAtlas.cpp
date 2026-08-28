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

Color Speckle(const int px, const int py, const Color base, const Color grain, const Color rare,
              const std::uint32_t salt, const float rareThreshold) noexcept {
    Color c = Mix(base, grain, 0.4F * NoiseAt(px, py, salt) + 0.4F * NoiseAt(px / 2, py / 2, salt + 1U));
    if (NoiseAt(px, py, salt + 2U) > rareThreshold) {
        c = Mix(c, rare, 0.75F);
    }
    return c;
}

Color CobblestonePixel(const int px, const int py) noexcept {
    // Rounded cobbles: cell grid with dark mortar between the cells.
    const int cell = 6;
    const int lx = px % cell;
    const int ly = (py + (px / cell) * 3) % cell;
    const bool mortar = lx == 0 || ly == 0;
    const Color stone = Speckle(px, py, Color{128, 128, 132, 255}, Color{150, 150, 154, 255},
                                Color{96, 96, 100, 255}, 610U, 0.9F);
    return mortar ? Mix(stone, Color{58, 58, 62, 255}, 0.75F) : stone;
}

Color PlanksPixel(const int px, const int py) noexcept {
    const Color light{168, 134, 86, 255};
    const Color dark{132, 100, 60, 255};
    Color c = Mix(light, dark, 0.3F * NoiseAt(px, py / 4, 620U) + 0.2F * NoiseAt(px / 3, py, 621U));
    if (py % 5 == 0) {  // seam between boards
        c = Mix(c, Color{70, 50, 30, 255}, 0.65F);
    }
    return c;
}

Color WoodSidePixel(const int px, const int py) noexcept {
    const Color light{112, 86, 54, 255};
    const Color dark{78, 58, 36, 255};
    // Vertical bark streaks.
    Color c = Mix(light, dark, 0.5F * NoiseAt(px, py / 6, 630U) + 0.35F * NoiseAt(px, py, 631U));
    if (NoiseAt(px, py / 3, 632U) > 0.82F) {
        c = Mix(c, Color{54, 40, 26, 255}, 0.6F);
    }
    return c;
}

Color WoodTopPixel(const int px, const int py) noexcept {
    const float dx = static_cast<float>(px) - 7.5F;
    const float dy = static_cast<float>(py) - 7.5F;
    const float ring = std::sqrt(dx * dx + dy * dy) + NoiseAt(px, py, 640U) * 1.5F;
    const bool darkRing = (static_cast<int>(ring) % 2) == 0;
    const Color light{150, 120, 78, 255};
    const Color dark{110, 84, 52, 255};
    return darkRing ? dark : light;
}

Color SandPixel(const int px, const int py) noexcept {
    return Speckle(px, py, Color{219, 205, 152, 255}, Color{201, 184, 130, 255},
                   Color{188, 170, 116, 255}, 650U, 0.92F);
}

Color GravelPixel(const int px, const int py) noexcept {
    return Speckle(px, py, Color{124, 118, 114, 255}, Color{96, 92, 90, 255},
                   Color{150, 146, 142, 255}, 660U, 0.7F);
}

Color BedrockPixel(const int px, const int py) noexcept {
    const float n = NoiseAt(px, py, 670U) * 0.6F + NoiseAt(px / 2, py / 2, 671U) * 0.4F;
    return Mix(Color{44, 44, 48, 255}, Color{92, 92, 98, 255}, n);
}

Color LeavesPixel(const int px, const int py) noexcept {
    const Color dark{46, 92, 34, 255};
    const Color light{88, 148, 60, 255};
    Color c = Mix(dark, light, 0.5F * NoiseAt(px, py, 680U) + 0.4F * NoiseAt(px / 2, py / 2, 681U));
    if (NoiseAt(px, py, 682U) > 0.8F) {
        c = Mix(c, Color{30, 62, 22, 255}, 0.8F);  // gaps read as deep shadow for now
    }
    return c;
}

Color GlassPixel(const int px, const int py) noexcept {
    const bool border = px == 0 || py == 0 || px == atlas::TileSize - 1 || py == atlas::TileSize - 1;
    if (border) {
        return Color{176, 206, 218, 255};
    }
    Color c = Mix(Color{206, 230, 240, 255}, Color{226, 244, 250, 255}, NoiseAt(px, py, 690U));
    if (px + py == 12 || px + py == 13) {  // faint highlight streak
        c = Color{240, 250, 253, 255};
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
        case atlas::Tile::Stone:
            return StonePixel(px, py);
        case atlas::Tile::Cobblestone:
            return CobblestonePixel(px, py);
        case atlas::Tile::Planks:
            return PlanksPixel(px, py);
        case atlas::Tile::WoodSide:
            return WoodSidePixel(px, py);
        case atlas::Tile::WoodTop:
            return WoodTopPixel(px, py);
        case atlas::Tile::Sand:
            return SandPixel(px, py);
        case atlas::Tile::Gravel:
            return GravelPixel(px, py);
        case atlas::Tile::Bedrock:
            return BedrockPixel(px, py);
        case atlas::Tile::Leaves:
            return LeavesPixel(px, py);
        case atlas::Tile::Glass:
            return GlassPixel(px, py);
        default:
            return Color{255, 0, 255, 255};
    }
}

}  // namespace

Image GenerateBlockAtlasImage() {
    Image image = GenImageColor(atlas::Width, atlas::Height, BLANK);
    for (int tile = 0; tile < atlas::TileCount; ++tile) {
        const int cellX = (tile % atlas::Columns) * atlas::CellStride;
        const int cellY = (tile / atlas::Columns) * atlas::CellStride;
        // Fill the whole cell; pixels in the gutter clamp to the nearest content
        // pixel so the tile's edge is extruded outward.
        for (int cy = 0; cy < atlas::CellStride; ++cy) {
            for (int cx = 0; cx < atlas::CellStride; ++cx) {
                const int sx = std::clamp(cx - atlas::Padding, 0, atlas::TileSize - 1);
                const int sy = std::clamp(cy - atlas::Padding, 0, atlas::TileSize - 1);
                ImageDrawPixel(&image, cellX + cx, cellY + cy, PixelFor(tile, sx, sy));
            }
        }
    }
    return image;
}

}  // namespace voxelgame
