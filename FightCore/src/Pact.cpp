#include "fightcore/Pact.h"

#include "fightcore/Body.h"
#include "fightcore/Technique.h"

namespace fc {
namespace {

// Wie sehr tut dieses Opfer DIESEM Build weh? 0..10
int sensitivity(const PactChoice& c, const BodyDef& b, const TechniqueDef* t, const PlaystyleProfile& p) {
    const bool nullborn = b.has(PassiveNullborn);
    switch (c.category) {
        case PactCategory::Body:
            if (c.detail == 0) return b.hp * 2;
            if (c.detail == 1) return b.speed * 2;
            return p.blockRate / 10;
        case PactCategory::Resource:
            if (nullborn) return 0; // hat keine Essenz -> wertlos
            if (c.detail == 0) return b.ce * 2;
            if (c.detail == 1) return b.ce * 2 + (b.has(PassiveFastRegen) ? 2 : 0);
            return p.healUsage / 10;
        case PactCategory::Technique:
            if (!t) return 0;
            if (c.detail <= 2) return p.skillUsage[c.detail] / 10; // ungenutzter Skill = fast wertlos
            if (c.detail == 3) return p.realmUsage / 10;
            return p.apexUsage / 10;
        case PactCategory::Rule:
            if (c.detail == 0) return 2 + p.blockRate / 8;
            if (!t) return 0;
            return (p.skillUsage[0] + p.skillUsage[1] + p.skillUsage[2]) / 30;
        case PactCategory::Time:
            return 3;
        case PactCategory::Companion: {
            const bool assist = b.has(PassiveAssist);
            const bool techComps = t && !t->companions.empty();
            if (!assist && !techComps) return 0; // hat keine Begleiter -> wertlos
            if (c.detail == 0) return assist ? 4 : 3;
            if (c.detail == 1) return assist ? 8 : 6;
            return assist ? 10 : 5;
        }
    }
    return 0;
}

int16_t restrictionPct(PactSize s) {
    switch (s) {
        case PactSize::Small: return 90;
        case PactSize::Medium: return 80;
        case PactSize::Large: return 65;
    }
    return 100;
}

} // namespace

PactResult evaluatePact(const PactChoice& c, const BodyDef& body, const TechniqueDef* tech, const PlaystyleProfile& p) {
    PactResult r;
    if (!c.active) return r;
    PactModifiers& m = r.mods;

    // 1) Opfer anwenden (immer, egal wie es bewertet wird)
    const int16_t pct = restrictionPct(c.size);
    switch (c.category) {
        case PactCategory::Body:
            if (c.detail == 0) m.maxHealthPct = pct;
            else if (c.detail == 1) m.speedPct = pct;
            else m.blockPct = pct;
            break;
        case PactCategory::Resource:
            if (c.detail == 0) m.cePoolPct = pct;
            else if (c.detail == 1) m.ceRegenPct = pct;
            else m.healPct = pct;
            break;
        case PactCategory::Technique:
            if (c.detail <= 2) m.lockedSkills = static_cast<uint8_t>(1u << c.detail);
            else if (c.detail == 3) m.noRealm = true;
            else m.noApex = true;
            break;
        case PactCategory::Rule:
            if (c.detail == 0) m.noBlock = true;
            else m.lockedSkills = 0x7;
            break;
        case PactCategory::Time:
            m.durationFrames = 3600;
            break;
        case PactCategory::Companion:
            if (c.detail == 0) m.companionHpPct = pct;
            else if (c.detail == 1) m.lockedCompanionSlots = 1;
            else m.companionLost = true;
            break;
    }

    // 2) Versteckter Wert
    int value = static_cast<int>(c.size) * sensitivity(c, body, tech, p);
    if (body.has(PassiveNullborn) && value > 12) value = 12; // begrenzter Pakt-Stack
    if (c.category == PactCategory::Time) value = value * 3 / 2;
    r.hiddenValue = static_cast<int16_t>(value);

    // 3) Ergebnis-Stufe. Belohnung ist nie größer als das Opfer.
    if (value < 4) r.tier = PactTier::TooCheap;
    else if (value < 12) r.tier = PactTier::Fair;
    else if (value < 24) r.tier = PactTier::High;
    else r.tier = PactTier::Overpaid;

    const int bonus = value > 15 ? 15 : value;
    switch (r.tier) {
        case PactTier::TooCheap: // Backlash: "Der Pakt war nicht genug."
            m.ceLeak = 3;
            m.cooldownPct = 125;
            break;
        case PactTier::Overpaid:
            m.damagePct = static_cast<int16_t>(m.damagePct + 5);
            if (c.category == PactCategory::Companion) m.bonusCompanion = 2; // Wandler (adaptiv)
            [[fallthrough]];
        case PactTier::High:
            if (c.category == PactCategory::Companion) {
                if (!m.bonusCompanion) m.bonusCompanion = 1; // temporärer Zusatz-Begleiter
            } else if (tech) {
                m.openRealm = true;
            } else {
                m.extraZeroSpark = 1;
            }
            // Bodies ohne Begleiter bekommen einen nur über zeitlich begrenzte Pakte
            if (c.category == PactCategory::Time && !body.has(PassiveAssist) && !(tech && !tech->companions.empty()))
                m.bonusCompanion = 1;
            [[fallthrough]];
        case PactTier::Fair:
            m.damagePct = static_cast<int16_t>(m.damagePct + bonus);
            m.realmGainPct = static_cast<int16_t>(100 + bonus * 2);
            break;
        case PactTier::None:
            break;
    }
    return r;
}

} // namespace fc
