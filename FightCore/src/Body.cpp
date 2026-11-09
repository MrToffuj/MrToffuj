#include "fightcore/Body.h"

namespace fc {

BodyStats deriveStats(const BodyDef& b) {
    BodyStats s;
    s.maxHealth = static_cast<int16_t>(600 + b.hp * 120);
    s.walkSpeed = Fx::ratio(35 + b.speed * 8, 1000);
    s.backSpeed = s.walkSpeed * Fx::ratio(8, 10);
    s.sideSpeed = s.walkSpeed * Fx::ratio(85, 100);
    s.dashSpeed = Fx::ratio(160 + b.speed * 30, 1000);
    s.dashFrames = 12;
    s.jumpVelocity = Fx::ratio(30, 100);
    s.gravity = Fx::ratio(15, 1000);
    s.pushRadius = Fx::ratio(45, 100);
    s.reach = Fx::ratio(850 + b.range * 75, 1000);
    s.ceMax = (400 + b.ce * 150) * 100;
    s.ceRegen = 4 + b.ce * 3;
    s.realmGainPct = static_cast<int16_t>(80 + b.ce * 10); // Caster lädt schneller, Heavy langsamer
    s.simpleRealmDrain = 150;

    if (b.has(PassiveFastRegen)) s.ceRegen = s.ceRegen * 3 / 2;
    if (b.has(PassiveQuickSkills)) s.cooldownPct = 75;
    if (b.has(PassiveWideBlackSpark)) s.blackSparkWindow = 3;
    if (b.has(PassiveCheapCounter)) { s.parryWindow = 8; s.simpleRealmDrain = 75; }
    if (b.has(PassiveHeavensBind)) {
        s.walkSpeed = s.walkSpeed * Fx::ratio(12, 10);
        s.dashSpeed = s.dashSpeed * Fx::ratio(12, 10);
        s.damagePct = 115;
        s.guaranteedHitImmune = true;
        s.ceMax = 0; // keine Essenz: kein Burst, keine Verstärkung, keine Heilung
        s.ceRegen = 0;
    }
    return s;
}

namespace bodies {
//                                  name          HP Spd CE Rng  Passive
const BodyDef& heavy()      { static const BodyDef b{"Heavy",      5, 1, 2, 2, PassiveSuperArmor | PassiveStrongGrab}; return b; }
const BodyDef& speedster()  { static const BodyDef b{"Speedster",  1, 5, 2, 2, PassiveDashCancel | PassiveFlankCrit}; return b; }
const BodyDef& caster()     { static const BodyDef b{"Caster",     1, 2, 4, 3, PassiveFastRegen | PassiveQuickSkills}; return b; }
const BodyDef& brawler()    { static const BodyDef b{"Brawler",    3, 3, 2, 2, PassiveWideBlackSpark}; return b; }
const BodyDef& technician() { static const BodyDef b{"Technician", 2, 3, 3, 2, PassiveCheapCounter}; return b; }
const BodyDef& summoner()   { static const BodyDef b{"Summoner",   2, 2, 3, 3, PassiveAssist}; return b; }
const BodyDef& heavensBind()   { static const BodyDef b{"Heaven's Bind",   1, 5, 2, 2, PassiveDashCancel | PassiveFlankCrit | PassiveHeavensBind}; return b; }
} // namespace bodies

} // namespace fc
