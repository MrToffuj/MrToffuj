#pragma once
#include <array>
#include <cstdint>

namespace fc {

// Button-Layout ist bewusst Controller-first und handheld-freundlich:
// keine Pflicht-Motion-Inputs, Specials = Richtung + Taste.
enum Button : uint8_t {
    BtnLight    = 1 << 0,
    BtnHeavy    = 1 << 1,
    BtnSpecial  = 1 << 2, // Technik-Taste
    BtnGuard    = 1 << 3,
    BtnJump     = 1 << 4,
    BtnDash     = 1 << 5,
    BtnUltimate = 1 << 6, // Realm / Ultimate
};

// Richtung im Numpad-Format, relativ zur Blickrichtung des Kämpfers:
// 6 = vorwärts, 4 = rückwärts, 8 = Sidestep links, 2 = Sidestep rechts, 5 = neutral.
struct FrameInput {
    uint8_t dir = 5;
    uint8_t buttons = 0;
    uint8_t consumed = 0; // bereits verbrauchte Tastendrücke (Buffer)
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
    bool held(uint8_t btn) const { return (current().buttons & btn) != 0; }

    // Wurde btn in den letzten `window` Frames frisch gedrückt (und noch nicht verbraucht)?
    bool pressedWithin(uint8_t btn, int window) const { return findPress(btn, window) >= 0; }

    void consume(uint8_t btn, int window) {
        int i = findPress(btn, window);
        if (i >= 0) hist_[(head_ + kSize - i) % kSize].consumed |= btn;
    }

    uint64_t hash(uint64_t h) const;

private:
    int findPress(uint8_t btn, int window) const {
        for (int i = 0; i < window && i < kSize - 1; ++i) {
            const FrameInput& now = ago(i);
            const FrameInput& before = ago(i + 1);
            if ((now.buttons & btn) && !(before.buttons & btn) && !(now.consumed & btn)) return i;
        }
        return -1;
    }

    std::array<FrameInput, kSize> hist_{};
    uint8_t head_ = 0;
};

} // namespace fc
