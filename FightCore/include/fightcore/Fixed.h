#pragma once
// Q16.16 fixed point. The simulation never touches float so rollback
// stays bit-identical across PC, consoles and handhelds.
#include <compare>
#include <cstdint>

namespace fc {

struct Fx {
    int32_t raw = 0;

    static constexpr int kShift = 16;
    static constexpr int32_t kOne = 1 << kShift;

    static constexpr Fx fromRaw(int32_t r) { Fx f; f.raw = r; return f; }
    static constexpr Fx fromInt(int32_t v) { return fromRaw(v * kOne); }
    // Fx::ratio(3, 2) == 1.5
    static constexpr Fx ratio(int32_t num, int32_t den) {
        return fromRaw(static_cast<int32_t>((static_cast<int64_t>(num) * kOne) / den));
    }

    constexpr int32_t toInt() const { return raw >> kShift; }
    // rendering only
    float toFloat() const { return static_cast<float>(raw) / static_cast<float>(kOne); }

    friend constexpr Fx operator+(Fx a, Fx b) { return fromRaw(a.raw + b.raw); }
    friend constexpr Fx operator-(Fx a, Fx b) { return fromRaw(a.raw - b.raw); }
    friend constexpr Fx operator-(Fx a) { return fromRaw(-a.raw); }
    friend constexpr Fx operator*(Fx a, Fx b) {
        return fromRaw(static_cast<int32_t>((static_cast<int64_t>(a.raw) * b.raw) >> kShift));
    }
    friend constexpr Fx operator/(Fx a, Fx b) {
        return fromRaw(static_cast<int32_t>((static_cast<int64_t>(a.raw) << kShift) / b.raw));
    }
    constexpr Fx& operator+=(Fx o) { raw += o.raw; return *this; }
    constexpr Fx& operator-=(Fx o) { raw -= o.raw; return *this; }
    friend constexpr auto operator<=>(Fx, Fx) = default;
};

constexpr Fx abs(Fx a) { return a.raw < 0 ? -a : a; }

constexpr uint64_t isqrt64(uint64_t n) {
    uint64_t res = 0;
    uint64_t bit = 1ull << 62;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= res + bit) { n -= res + bit; res = (res >> 1) + bit; }
        else { res >>= 1; }
        bit >>= 2;
    }
    return res;
}

// x right, y up, z forward
struct Vec3 {
    Fx x, y, z;

    friend constexpr Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
    friend constexpr Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    friend constexpr Vec3 operator*(Vec3 a, Fx s) { return {a.x * s, a.y * s, a.z * s}; }
    constexpr Vec3& operator+=(Vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    friend constexpr bool operator==(Vec3, Vec3) = default;
};

constexpr int64_t lengthSqRaw(Vec3 v) {
    return static_cast<int64_t>(v.x.raw) * v.x.raw + static_cast<int64_t>(v.y.raw) * v.y.raw +
           static_cast<int64_t>(v.z.raw) * v.z.raw;
}
constexpr Fx length(Vec3 v) { return Fx::fromRaw(static_cast<int32_t>(isqrt64(static_cast<uint64_t>(lengthSqRaw(v))))); }
constexpr Fx dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Normalized on the ground plane. Returns fallback for (near) zero vectors.
constexpr Vec3 normalizeXZ(Vec3 v, Vec3 fallback) {
    Vec3 flat{v.x, Fx{}, v.z};
    Fx len = length(flat);
    if (len.raw < 16) return fallback;
    return {v.x / len, Fx{}, v.z / len};
}

} // namespace fc
