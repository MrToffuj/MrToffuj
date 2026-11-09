#pragma once
// Small helpers shared by the technique files.
#include "fightcore/Simulation.h"

namespace fc::techniques {

inline Fx pct(int p) { return Fx::ratio(p, 100); }
inline Fx tenths(int v) { return Fx::ratio(v, 10); }

inline MoveDef move(const char* name, MoveCategory cat, int16_t startup, int16_t active, int16_t recovery, int16_t eventFrame = -1) {
    MoveDef m;
    m.name = name;
    m.category = cat;
    m.startup = startup;
    m.active = active;
    m.recovery = recovery;
    m.eventFrame = eventFrame;
    return m;
}

inline HitProps hit(int16_t damage, int16_t hitstun, Fx knockback, Fx launch, ImpactClass impact, bool unblockable = false) {
    const int16_t hitstop = static_cast<int16_t>(6 + 2 * static_cast<int>(impact));
    return {damage, hitstun, static_cast<int16_t>(hitstun * 2 / 3), hitstop, knockback, launch, impact, unblockable};
}

inline MoveDef melee(const char* name, MoveCategory cat, int16_t startup, int16_t active, int16_t recovery, Vec3 offset, Fx radius,
                     HitProps props) {
    MoveDef m = move(name, cat, startup, active, recovery);
    m.hitboxes = {{offset, radius}};
    m.hit = props;
    return m;
}

inline AssistAction act(AssistKind k, int16_t startup, int16_t active, int16_t recovery, int16_t move, int speedPct) {
    return {k, startup, active, recovery, move, pct(speedPct)};
}

inline void lockFighter(FighterState& f, int16_t frames) {
    if (f.out) return;
    f.state = FState::Locked;
    f.lock = frames;
    f.moveId = -1;
    f.vel = {};
}

inline void pullToward(FighterState& f, Vec3 to, Fx range, Fx strength) {
    const Vec3 d{to.x - f.pos.x, Fx{}, to.z - f.pos.z};
    const Fx dist = length(d);
    if (f.out || dist > range || dist < Fx::ratio(1, 2)) return;
    f.pos += normalizeXZ(d, {}) * strength;
}

inline Fx flatDistance(Vec3 a, Vec3 b) { return length(Vec3{a.x - b.x, Fx{}, a.z - b.z}); }

inline Vec3 rightOf(Vec3 facing) { return {facing.z, Fx{}, -facing.x}; }

// Recoil that hurts but never kills you on its own
inline void recoil(const TechContext& c, int16_t amount) {
    const int16_t hp = c.me().health;
    c.damage(c.self, hp - amount < 1 ? static_cast<int16_t>(hp - 1) : amount);
}

inline int clampStacks(int32_t v, int32_t max) { return static_cast<int>(v < 0 ? 0 : (v > max ? max : v)); }

} // namespace fc::techniques
