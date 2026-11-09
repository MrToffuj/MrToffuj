#pragma once
#include <array>
#include <cstdint>

namespace fc {

// Same controls for every build.
// Face buttons: Light, Heavy, Grab, Jump
// Shoulders:    Guard (R1), Mod (L1), Dash (R2), Ult (L2)
//
// Mod + Light/Heavy/Grab  skill 1/2/3
// Ult                     Max Output
// Mod + Ult               Realm Expansion (full meter)
// Ult in an enemy realm   Simple Realm (hold)
// Guard + Grab            CE burst, works in hitstun
// Guard + Dash            dodge
// Mod + Guard (hold)      Reverse Flow
// Mod + Dash              toggle reinforcement
// Assist (R3)             call a companion
// Target (L3)             lock on / let go
// right stick flick       next target while locked
enum Button : uint16_t {
    BtnLight = 1 << 0,
    BtnHeavy = 1 << 1,
    BtnGrab  = 1 << 2,
    BtnGuard = 1 << 3,
    BtnJump  = 1 << 4,
    BtnDash  = 1 << 5,
    BtnMod   = 1 << 6,
    BtnUlt   = 1 << 7,
    BtnAssist = 1 << 8,
    BtnTarget = 1 << 9,
    BtnTargetNext = 1 << 10,
};

// Numpad notation as seen from the camera behind the fighter:
// 7 8 9   8 = toward the target, 2 = away
// 4 5 6   4/6 = circle around it
// 1 2 3
// Players without lock-on move relative to their camera, so the client sends a world space
// direction (moveX/moveZ, -127..127). The simulation turns it into dir itself. Bots and locked
// players can send dir directly and leave move at zero.
struct FrameInput {
    uint8_t dir = 5;
    uint16_t buttons = 0;
    uint16_t consumed = 0;
    int8_t moveX = 0;
    int8_t moveZ = 0;

    bool hasWorldMove() const { return moveX != 0 || moveZ != 0; }
};

class InputBuffer {
public:
    static constexpr int kSize = 16;

    void push(FrameInput in) {
        in.consumed = 0;
        head_ = static_cast<uint8_t>((head_ + 1) % kSize);
        hist_[head_] = in;
    }
    const FrameInput& ago(int n) const { return hist_[(head_ + kSize - n) % kSize]; }
    const FrameInput& current() const { return ago(0); }
    bool held(uint16_t btn) const { return (current().buttons & btn) != 0; }

    // Frames since the newest unconsumed press of btn, or -1.
    int pressAge(uint16_t btn, int window) const {
        for (int i = 0; i < window && i < kSize - 1; ++i) {
            const FrameInput& now = ago(i);
            if ((now.buttons & btn) && !(ago(i + 1).buttons & btn) && !(now.consumed & btn)) return i;
        }
        return -1;
    }
    bool pressedWithin(uint16_t btn, int window) const { return pressAge(btn, window) >= 0; }
    void consume(uint16_t btn, int window) {
        int i = pressAge(btn, window);
        if (i >= 0) hist_[(head_ + kSize - i) % kSize].consumed = static_cast<uint16_t>(hist_[(head_ + kSize - i) % kSize].consumed | btn);
    }

    uint64_t hash(uint64_t h) const;

private:
    std::array<FrameInput, kSize> hist_{};
    uint8_t head_ = 0;
};

} // namespace fc
