#pragma once
#include <array>
#include <cstdint>

namespace fc {

// Gleiche Steuerung für ALLE Builds (GDD-Pillar 1).
//  Face:      Light, Heavy, Grab, Jump
//  Schultern: Guard (R1), Mod (L1), Dash (R2), Ult (L2)
//
//  Mod + Light/Heavy/Grab  -> Skill 1/2/3 (Offense / Utility / Defense-Movement)
//  Ult                     -> Apex-Technik (Maximum)
//  Mod + Ult               -> Realm öffnen (100 % Realm-Meter)
//  Ult während gegn. Realm -> Schutzkreis (halten)
//  Guard + Grab            -> Essenz-Burst (Notfall-Pushback, auch aus Hitstun)
//  Guard + Dash            -> Dodge
//  Mod + Guard (halten)    -> Rückfluss (Heilung, steht still)
//  Mod + Dash              -> Verstärkung an/aus
//  Assist (R3)             -> Begleiter rufen: neutral Angriff, vor Druck, hoch Anti-Air, runter Schutz
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
};

// Richtung im Numpad-Format relativ zum Gegner: 6 vor, 4 zurück, 8/2 Sidestep, 5 neutral.
struct FrameInput {
    uint8_t dir = 5;
    uint16_t buttons = 0;
    uint16_t consumed = 0;
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

    // Alter (in Frames) des jüngsten unverbrauchten Drucks von btn, oder -1.
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
