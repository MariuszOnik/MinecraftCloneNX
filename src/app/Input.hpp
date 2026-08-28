#pragma once

namespace voxelgame {

// Frame input as game actions rather than raw keys, so the same code drives a
// keyboard/mouse PC and Joy-Con on Switch (PLAN.md 5.1).
struct FrameInput {
    float moveForward = 0.0F;   // -1..1, +forward
    float moveStrafe = 0.0F;    // -1..1, +right
    float lookYaw = 0.0F;       // radians this frame, + turns left
    float lookPitch = 0.0F;     // radians this frame, + looks up
    bool jump = false;          // pressed this frame
    bool sprint = false;        // held
    bool toggleMouseLook = false;
};

// `mouseLook` gates reading the mouse delta (ignored while the cursor is free).
[[nodiscard]] FrameInput PollFrameInput(bool mouseLook);

}  // namespace voxelgame
