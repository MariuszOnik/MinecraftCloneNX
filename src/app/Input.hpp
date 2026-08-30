#pragma once

namespace voxelgame {

// Frame input as game actions rather than raw keys, so the same code drives a
// keyboard/mouse PC and Joy-Con on Switch (PLAN.md 5.1).
struct FrameInput {
    float moveForward = 0.0F;   // -1..1, +forward
    float moveStrafe = 0.0F;    // -1..1, +right
    float fly = 0.0F;           // -1..1, +up / -down (free-cam vertical)
    float lookYaw = 0.0F;       // radians this frame, + turns left
    float lookPitch = 0.0F;     // radians this frame, + looks up
    bool jump = false;          // pressed this frame
    bool sprint = false;        // held
    bool toggleMouseLook = false;
    bool breakBlock = false;    // pressed this frame
    bool placeBlock = false;    // pressed this frame
    bool saveRequested = false; // pressed this frame (F5, or L1 + A on the pad)
    bool cycleView = false;     // pressed this frame (C key, or Select/Minus)
    int cycleBlock = 0;         // -1 / 0 / +1, change the held block (Q/E, X/Y)
    float zoom = 0.0F;          // mouse-wheel delta (camera zoom, or block cycle)
};

// `mouseLook` gates reading the mouse delta (ignored while the cursor is free).
[[nodiscard]] FrameInput PollFrameInput(bool mouseLook);

}  // namespace voxelgame
