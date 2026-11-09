#pragma once
// Cursed objects: passive items worn into a match. Which modes allow them and how many is part
// of the mode rules (custom lobbies go up to five). Mods can add their own.
#include <array>
#include <cstdint>
#include <vector>

namespace fc {

constexpr int kMaxCursedObjects = 5;

enum class Effect : uint8_t {
    DamagePct,            // all damage dealt
    DamageTakenPct,
    NormalDamagePct,      // light/heavy/grab only
    SkillDamagePct,       // technique moves only
    CompanionDamagePct,
    SpeedPct,
    CeRegenPct,
    CeMaxPct,
    MaxHealthPct,
    CooldownPct,          // negative = faster
    RealmGainPct,
    StartRealmMeterPct,   // realm meter filled at spawn
    BlackSparkWindow,     // extra frames
    ParryWindow,          // extra frames
    LifestealPct,
    HealOnKo,             // HP
    GuaranteedHitTakenPct,
    OvertimeDamagePct,    // bonus after staying alive for 60 s
    CritChancePct,        // chance for double damage
    ThornsPct,            // share of melee damage sent back
    Count
};

struct EffectDef {
    Effect type;
    int16_t value;
};

struct CursedObjectDef {
    const char* id = "";
    const char* name = "";
    const char* description = "";
    std::vector<EffectDef> effects;
};

// Summed effects of everything a fighter wears. Values are deltas, so 0 means "no change".
struct ItemModifiers {
    std::array<int16_t, static_cast<size_t>(Effect::Count)> value{};

    int16_t operator[](Effect e) const { return value[static_cast<size_t>(e)]; }
    int pct(Effect e) const { return 100 + value[static_cast<size_t>(e)]; }
    void add(const CursedObjectDef& item) {
        for (const EffectDef& e : item.effects) value[static_cast<size_t>(e.type)] = static_cast<int16_t>(value[static_cast<size_t>(e.type)] + e.value);
    }
};

namespace cursed {
const std::vector<const CursedObjectDef*>& builtins();
const CursedObjectDef* find(const char* id);
} // namespace cursed

} // namespace fc
