#pragma once
// Body types decide how a fighter moves and defends.
// Every body spends exactly 10 points on HP / speed / CE / range.
#include <cstdint>

#include "fightcore/Fixed.h"

namespace fc {

enum BodyPassive : uint16_t {
    PassiveSuperArmor      = 1 << 0,
    PassiveStrongGrab      = 1 << 1,
    PassiveDashCancel      = 1 << 2,
    PassiveFlankCrit       = 1 << 3,
    PassiveFastRegen       = 1 << 4,
    PassiveQuickSkills     = 1 << 5,
    PassiveWideBlackSpark   = 1 << 6,
    PassiveCheapCounter    = 1 << 7,
    PassiveAssist          = 1 << 8,
    PassiveHeavensBind        = 1 << 9,
};

struct BodyDef {
    const char* name = "";
    uint8_t hp = 0, speed = 0, ce = 0, range = 0;
    uint16_t passives = 0;

    bool has(BodyPassive p) const { return (passives & p) != 0; }
    bool valid() const { return hp + speed + ce + range == 10; }
};

struct BodyStats {
    int16_t maxHealth = 0;
    Fx walkSpeed, backSpeed, sideSpeed, dashSpeed;
    int16_t dashFrames = 12;
    Fx jumpVelocity, gravity, pushRadius;
    Fx reach;  // scales base move hitbox reach
    int32_t ceMax = 0;  // 1/100 CE
    int32_t ceRegen = 0;  // per frame, 1/100 CE
    int16_t blackSparkWindow = 1;  // +- frames
    int16_t parryWindow = 5;
    int32_t simpleRealmDrain = 0;  // per frame, 1/100 CE
    int16_t cooldownPct = 100;
    int16_t damagePct = 100;
    int16_t realmGainPct = 100;
    bool guaranteedHitImmune = false;
};

BodyStats deriveStats(const BodyDef& b);

namespace bodies {
const BodyDef& heavy();
const BodyDef& speedster();
const BodyDef& caster();
const BodyDef& brawler();
const BodyDef& technician();
const BodyDef& summoner();
const BodyDef& heavensBind();  // speedster without a technique
} // namespace bodies

} // namespace fc
