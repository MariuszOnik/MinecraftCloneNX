#include "render/TilingShader.hpp"

namespace voxelgame {
namespace {

#if defined(__SWITCH__)

constexpr const char* kVertexShader = R"(#version 100
precision highp float;
attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec2 vertexTexCoord2;
attribute vec4 vertexColor;
uniform mat4 mvp;
varying vec2 fragTexCoord;
varying vec2 fragTileOrigin;
varying vec4 fragColor;
void main() {
    fragTexCoord = vertexTexCoord;
    fragTileOrigin = vertexTexCoord2;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
})";

constexpr const char* kFragmentShader = R"(#version 100
precision highp float;
varying vec2 fragTexCoord;
varying vec2 fragTileOrigin;
varying vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 tileExtent;
uniform float alphaCutoff;
void main() {
    vec2 local = fract(fragTexCoord);
    vec2 atlasUV = fragTileOrigin + local * tileExtent;
    vec4 texel = texture2D(texture0, atlasUV);
    if (texel.a < alphaCutoff) discard;
    gl_FragColor = texel * fragColor * colDiffuse;
})";

#else

constexpr const char* kVertexShader = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec2 vertexTexCoord2;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec2 fragTileOrigin;
out vec4 fragColor;
void main() {
    fragTexCoord = vertexTexCoord;
    fragTileOrigin = vertexTexCoord2;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
})";

constexpr const char* kFragmentShader = R"(#version 330
in vec2 fragTexCoord;
in vec2 fragTileOrigin;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 tileExtent;
uniform float alphaCutoff;
out vec4 finalColor;
void main() {
    vec2 local = fract(fragTexCoord);
    vec2 atlasUV = fragTileOrigin + local * tileExtent;
    vec4 texel = texture(texture0, atlasUV);
    if (texel.a < alphaCutoff) discard;
    finalColor = texel * fragColor * colDiffuse;
})";

#endif

}  // namespace

Shader LoadTilingShader() {
    return LoadShaderFromMemory(kVertexShader, kFragmentShader);
}

void SetTilingShaderExtent(const Shader shader, const float extentU, const float extentV) {
    const int location = GetShaderLocation(shader, "tileExtent");
    if (location < 0) {
        TraceLog(LOG_WARNING, "VOXEL: tiling shader has no 'tileExtent' uniform");
        return;
    }
    const float extent[2] = {extentU, extentV};
    SetShaderValue(shader, location, extent, SHADER_UNIFORM_VEC2);
}

void SetTilingShaderAlphaCutoff(const Shader shader, const float cutoff) {
    const int location = GetShaderLocation(shader, "alphaCutoff");
    if (location < 0) {
        return;
    }
    SetShaderValue(shader, location, &cutoff, SHADER_UNIFORM_FLOAT);
}

}  // namespace voxelgame
