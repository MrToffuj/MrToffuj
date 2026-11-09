// Bot-KI. Läuft INNERHALB der Simulation und hängt nur vom Spielzustand ab -> deterministisch,
// funktioniert dadurch automatisch mit Rollback und online, ohne dass Bot-Inputs übertragen werden.
#include "Internal.h"

namespace fc {
namespace {

uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

struct Level {
    int blockPct;    // Chance, einen erkannten Angriff zu blocken
    int reaction;    // Frames, bevor ein gegnerischer Angriff erkannt wird
    int aggression;  // Chance, im Nahkampf anzugreifen statt zu umkreisen
    int burstPct;
};
constexpr Level kLevels[3] = {
    {20, 14, 35, 2},
    {45, 9, 55, 5},
    {75, 5, 75, 10},
};

} // namespace

FrameInput botInput(const MatchState& m, const MatchSetup& s, int i) {
    using namespace detail;
    const FighterState& f = m.f[i];
    FrameInput in;
    if (f.out || f.target < 0) return in;

    const FighterState& t = m.f[f.target];
    const Loadout& L = s.loadout[i];
    const Level& lv = kLevels[L.botLevel > 2 ? 2 : L.botLevel];
    // Entscheidungen bleiben 8 Frames stabil, sonst zappelt der Bot
    const uint32_t seed = hash32(static_cast<uint32_t>(m.frame / 8) * 7919u + static_cast<uint32_t>(i) * 104729u);
    auto roll = [&](uint32_t salt) { return static_cast<int>(hash32(seed + salt) % 100); };
    const uint16_t prev = f.input.current().buttons;
    auto press = [&](uint16_t b) { if (!(prev & b)) in.buttons = static_cast<uint16_t>(in.buttons | b); };
    const Fx dist = distXZ(f.pos, t.pos);
    const int32_t ceMax = s.stats[i].ceMax;

    // In einem gegnerischen Realm: Schutzkreis halten
    if (enemyRealmAround(m, i) >= 0 && f.ce > 0 && roll(1) < lv.blockPct + 20) {
        in.buttons = BtnUlt;
        return in;
    }

    // Unter Druck: Burst
    if ((f.state == FState::Hitstun || f.state == FState::Blockstun) && ceMax > 0 && f.ce >= ceMax * 3 / 10 &&
        roll(2) < lv.burstPct) {
        in.buttons = BtnGuard;
        press(BtnGrab);
        return in;
    }

    // Verteidigung: Angriff des Ziels erkannt
    const bool threatened = t.state == FState::Attack && dist < Fx::fromInt(4) && t.moveFrame >= lv.reaction / 3;
    if (threatened && roll(3) < lv.blockPct) {
        in.buttons = BtnGuard;
        return in;
    }

    if (f.state == FState::Air) {
        if (dist < Fx::fromInt(2)) press(BtnLight);
        return in;
    }

    // Ultimates
    if (L.technique && f.realmMeter >= kRealmMeterMax && f.realm.timer == 0 && dist < Fx::fromInt(10)) {
        in.buttons = BtnMod;
        press(BtnUlt);
        return in;
    }
    if (L.technique && !f.apexUsed && ceMax > 0 && f.ce >= ceMax / 2 && dist < Fx::fromInt(6) && roll(4) < 6) {
        press(BtnUlt);
        return in;
    }

    bool hasCompanion = false;
    for (const CompanionState& c : f.comp) hasCompanion |= c.onField();
    if (hasCompanion && roll(5) < 6) press(BtnAssist);

    if (dist > Fx::ratio(22, 10)) {
        // Annähern, manchmal schräg, auf Distanz dashen oder Fernkampf-Skill
        in.dir = roll(6) < 25 ? (roll(7) < 50 ? 7 : 9) : 8;
        if (dist > Fx::fromInt(7) && roll(8) < 25) press(BtnDash);
        if (L.technique && f.cooldown[0] == 0 && f.burnout == 0 && roll(9) < 6) {
            in.buttons = static_cast<uint16_t>(in.buttons | BtnMod);
            press(BtnLight);
        }
        return in;
    }

    if (roll(10) >= lv.aggression) {
        in.dir = roll(11) < 50 ? 4 : 6; // umkreisen
        return in;
    }
    const int pick = roll(12);
    if (t.state == FState::Guard && pick < 60) press(BtnGrab);
    else if (pick < 50) press(BtnLight);
    else if (pick < 72) press(BtnHeavy);
    else if (pick < 80) { in.dir = 2; press(BtnHeavy); } // Launcher
    else if (pick < 88) press(BtnGrab);
    else if (L.technique && f.burnout == 0) {
        in.buttons = static_cast<uint16_t>(in.buttons | BtnMod);
        press(pick < 94 ? BtnLight : BtnHeavy);
    } else {
        press(BtnLight);
    }
    return in;
}

} // namespace fc
