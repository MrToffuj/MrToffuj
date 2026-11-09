#pragma once
// Body Types: bestimmen WIE ein Char sich bewegt und verteidigt.
// Jeder Body verteilt genau 10 Punkte auf HP / Speed / CE / Range (Balance-Anker).
#include <cstdint>

#include "fightcore/Fixed.h"

namespace fc {

enum BodyPassive : uint16_t {
    PassiveSuperArmor      = 1 << 0, // Heavy: Armor auf Heavies
    PassiveStrongGrab      = 1 << 1, // Heavy: stärkere Grabs
    PassiveDashCancel      = 1 << 2, // Speedster
    PassiveFlankCrit       = 1 << 3, // Speedster: Crit von Rücken/Seite
    PassiveFastRegen       = 1 << 4, // Caster
    PassiveQuickSkills     = 1 << 5, // Caster: kürzere Cooldowns
    PassiveWideBlackSpark   = 1 << 6, // Brawler: größeres Nullfunke-Fenster
    PassiveCheapCounter    = 1 << 7, // Technician: größeres Parry-Fenster, billiger Schutzkreis
    PassiveAssist          = 1 << 8, // Summoner: Assist-Slot (noch nicht implementiert)
    PassiveHeavensBind        = 1 << 9, // Sonder-Build ohne Technik
};

struct BodyDef {
    const char* name = "";
    uint8_t hp = 0, speed = 0, ce = 0, range = 0;
    uint16_t passives = 0;

    bool has(BodyPassive p) const { return (passives & p) != 0; }
    bool valid() const { return hp + speed + ce + range == 10; }
};

// Aus den Punkten abgeleitete, konkrete Werte.
struct BodyStats {
    int16_t maxHealth = 0;
    Fx walkSpeed, backSpeed, sideSpeed, dashSpeed;
    int16_t dashFrames = 12;
    Fx jumpVelocity, gravity, pushRadius;
    Fx reach;                 // Multiplikator für Basis-Hitbox-Reichweite
    int32_t ceMax = 0;        // in 1/100 Essenz
    int32_t ceRegen = 0;      // pro Frame, in 1/100 Essenz
    int16_t blackSparkWindow = 1; // +- Frames
    int16_t parryWindow = 5;
    int32_t simpleRealmDrain = 0;    // Schutzkreis-Kosten pro Frame (1/100)
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
const BodyDef& heavensBind(); // Sonder-Build: Speedster ohne Technik
} // namespace bodies

} // namespace fc
