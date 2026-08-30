#include "app/Input.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace voxelgame {
namespace {

constexpr float kMouseSensitivity = 0.0028F;
constexpr float kStickLookSpeed = 2.6F;   // radians/second at full deflection
constexpr float kStickDeadzone = 0.18F;

#if defined(__SWITCH__)
// libnx analog sticks report "up" as +y; desktop backends report it as -y.
constexpr float kStickYSign = 1.0F;
#else
constexpr float kStickYSign = -1.0F;
#endif

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

float KeyAxis(const int negative, const int positive) noexcept {
    return static_cast<float>(IsKeyDown(positive) ? 1 : 0) -
           static_cast<float>(IsKeyDown(negative) ? 1 : 0);
}

float ButtonAxis(const int negative, const int positive) noexcept {
    return static_cast<float>(IsGamepadButtonDown(0, positive) ? 1 : 0) -
           static_cast<float>(IsGamepadButtonDown(0, negative) ? 1 : 0);
}

}  // namespace

FrameInput PollFrameInput(const bool mouseLook) {
    const float dt = GetFrameTime();

    FrameInput input;
    input.moveForward = KeyAxis(KEY_S, KEY_W);
    input.moveStrafe = KeyAxis(KEY_A, KEY_D);
    input.fly = KeyAxis(KEY_LEFT_CONTROL, KEY_SPACE);
    input.jump = IsKeyPressed(KEY_SPACE);
    input.sprint = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    input.toggleMouseLook = IsKeyPressed(KEY_TAB);
    input.breakBlock = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.placeBlock = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    input.saveRequested = IsKeyPressed(KEY_F5);
    input.cycleView = IsKeyPressed(KEY_C);
    input.zoom = GetMouseWheelMove();
    input.cycleBlock = 0;
    if (IsKeyPressed(KEY_Q)) {
        --input.cycleBlock;
    }
    if (IsKeyPressed(KEY_E)) {
        ++input.cycleBlock;
    }

    if (mouseLook) {
        const Vector2 delta = GetMouseDelta();
        input.lookYaw = delta.x * kMouseSensitivity;
        input.lookPitch = -delta.y * kMouseSensitivity;
    }

    // A real controller reports a name; nameless "devices" are usually ghost
    // XInput slots whose axes rest slightly off-centre.
    const char* gamepadName = IsGamepadAvailable(0) ? GetGamepadName(0) : nullptr;
    if (gamepadName != nullptr && gamepadName[0] != '\0') {
        // Left stick or D-pad to move.
        input.moveStrafe += ApplyDeadzone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X)) +
                            ButtonAxis(GAMEPAD_BUTTON_LEFT_FACE_LEFT, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        input.moveForward +=
            kStickYSign * ApplyDeadzone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y)) +
            ButtonAxis(GAMEPAD_BUTTON_LEFT_FACE_DOWN, GAMEPAD_BUTTON_LEFT_FACE_UP);

        // Right stick to look.
        input.lookYaw +=
            ApplyDeadzone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X)) * kStickLookSpeed * dt;
        input.lookPitch += kStickYSign *
                           ApplyDeadzone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y)) *
                           kStickLookSpeed * dt;

        // L1 + A is the save shortcut; swallow the jump/sprint it would also fire.
        const bool l1Held = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
        if (l1Held && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) {
            input.saveRequested = true;
        }

        input.jump = input.jump || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) ||
                     (!l1Held && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT));
        input.sprint = input.sprint || l1Held;
        // Free-cam vertical: R1 up, L1 down (L1 also boosts speed via sprint).
        input.fly += static_cast<float>(IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) -
                     static_cast<float>(l1Held);
        input.fly = std::clamp(input.fly, -1.0F, 1.0F);
        input.toggleMouseLook =
            input.toggleMouseLook || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT);
        input.cycleView =
            input.cycleView || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_LEFT);
        input.breakBlock =
            input.breakBlock || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
        input.placeBlock =
            input.placeBlock || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_TRIGGER_2);
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_UP)) {
            ++input.cycleBlock;
        }
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) {
            --input.cycleBlock;
        }
    }

    input.moveForward = std::clamp(input.moveForward, -1.0F, 1.0F);
    input.moveStrafe = std::clamp(input.moveStrafe, -1.0F, 1.0F);
    return input;
}

}  // namespace voxelgame
