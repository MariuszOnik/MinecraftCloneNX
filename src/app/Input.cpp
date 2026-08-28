#include "app/Input.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace voxelgame {
namespace {

constexpr float kMouseSensitivity = 0.0028F;
constexpr float kStickLookSpeed = 2.6F;   // radians/second at full deflection
constexpr float kStickDeadzone = 0.22F;

// Radial deadzone with a rescale, then a hard snap so a resting stick that just
// creeps past the deadzone still reads as zero.
float ApplyDeadzone(const float value) noexcept {
    const float magnitude = std::fabs(value);
    if (magnitude < kStickDeadzone) {
        return 0.0F;
    }
    const float sign = value < 0.0F ? -1.0F : 1.0F;
    const float scaled = (magnitude - kStickDeadzone) / (1.0F - kStickDeadzone);
    return scaled < 0.06F ? 0.0F : sign * scaled;
}

float Axis(const int negative, const int positive) noexcept {
    return static_cast<float>(IsKeyDown(positive) ? 1 : 0) -
           static_cast<float>(IsKeyDown(negative) ? 1 : 0);
}

}  // namespace

FrameInput PollFrameInput(const bool mouseLook) {
    const float dt = GetFrameTime();

    FrameInput input;
    input.moveForward = Axis(KEY_S, KEY_W);
    input.moveStrafe = Axis(KEY_A, KEY_D);
    input.jump = IsKeyPressed(KEY_SPACE);
    input.sprint = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    input.toggleMouseLook = IsKeyPressed(KEY_TAB);

    if (mouseLook) {
        const Vector2 delta = GetMouseDelta();
        input.lookYaw = delta.x * kMouseSensitivity;
        input.lookPitch = -delta.y * kMouseSensitivity;
    }

    // A real controller reports a name; nameless "devices" are usually ghost
    // XInput slots whose axes rest slightly off-centre.
    const char* gamepadName = IsGamepadAvailable(0) ? GetGamepadName(0) : nullptr;
    if (gamepadName != nullptr && gamepadName[0] != '\0') {
        input.moveStrafe += ApplyDeadzone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X));
        input.moveForward += -ApplyDeadzone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y));
        input.lookYaw +=
            ApplyDeadzone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X)) * kStickLookSpeed * dt;
        input.lookPitch +=
            -ApplyDeadzone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y)) * kStickLookSpeed * dt;
        input.jump = input.jump || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        input.sprint = input.sprint || IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
        input.toggleMouseLook =
            input.toggleMouseLook || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT);
    }

    input.moveForward = std::clamp(input.moveForward, -1.0F, 1.0F);
    input.moveStrafe = std::clamp(input.moveStrafe, -1.0F, 1.0F);
    return input;
}

}  // namespace voxelgame
