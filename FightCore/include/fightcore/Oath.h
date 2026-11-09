#pragma once
// Binding Oaths. The player picks what to give up and how much; the game decides
// what it's worth for their build and hands out a reward or a backlash. Nothing is shown.
#include <cstdint>

namespace fc {

struct BodyDef;
struct TechniqueDef;

enum class OathCategory : uint8_t { Body, Resource, Technique, Rule, Time, Companion };
enum class OathSize : uint8_t { Small = 1, Medium = 2, Large = 4 };
enum class OathTier : uint8_t { None, TooCheap, Fair, High, Overpaid };

// detail per category:
// Body:      0 max HP, 1 speed, 2 block strength
// Resource:  0 CE pool, 1 CE regen, 2 Reverse Flow healing
// Technique: 0..2 lock a skill, 3 no realm, 4 no Max Output
// Rule:      0 never block, 1 melee only (all skills locked)
// Time:      only lasts 60 s but counts for more
// Companion: 0 companion HP, 1 lock a slot, 2 lose a companion for good
struct OathChoice {
    bool active = false;
    OathCategory category = OathCategory::Body;
    OathSize size = OathSize::Small;
    uint8_t detail = 0;
};

// Usage stats from recent matches (0..100). Peers exchange these before the match.
struct PlaystyleProfile {
    uint8_t skillUsage[3] = {33, 33, 33};
    uint8_t blockRate = 50;
    uint8_t realmUsage = 50;
    uint8_t maxOutputUsage = 50;
    uint8_t healUsage = 30;
};

struct OathModifiers {
    // what was given up
    int16_t maxHealthPct = 100, speedPct = 100, blockPct = 100;
    int16_t cePoolPct = 100, ceRegenPct = 100, healPct = 100;
    uint8_t lockedSkills = 0;  // bitmask
    bool noRealm = false, noMaxOutput = false, noBlock = false;
    int16_t companionHpPct = 100;
    uint8_t lockedCompanionSlots = 0;
    bool companionLost = false;
    // rewards / backlash
    int16_t damagePct = 100, realmGainPct = 100, cooldownPct = 100;
    int32_t ceLeak = 0;  // per frame, 1/100 CE
    int16_t extraBlackSpark = 0;
    bool openRealm = false;
    bool maxOutputNoStartup = false;
    uint8_t bonusCompanion = 0;  // 0 none, 1 Oath Spirit, 2 The Adaptor
    int32_t durationFrames = -1;  // -1 = whole match
};

struct OathResult {
    OathTier tier = OathTier::None;
    int16_t hiddenValue = 0;  // debug only
    OathModifiers mods;
};

OathResult evaluateOath(const OathChoice& choice, const BodyDef& body, const TechniqueDef* tech,
                        const PlaystyleProfile& profile);

} // namespace fc
