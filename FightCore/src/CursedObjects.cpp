#include <cstring>

#include "fightcore/CursedObject.h"

namespace fc::cursed {
namespace {

using E = Effect;

const CursedObjectDef kItems[] = {
    {"withered_finger", "Withered Finger", "Raw power with a price. +15% damage, +10% damage taken.",
     {{E::DamagePct, 15}, {E::DamageTakenPct, 10}}},
    {"womb_painting", "Womb Painting", "Hits feed you. 10% lifesteal, -10% max HP.", {{E::LifestealPct, 10}, {E::MaxHealthPct, -10}}},
    {"stitched_doll", "Stitched Doll", "Companions hit 25% harder.", {{E::CompanionDamagePct, 25}}},
    {"unraveling_rope", "Unraveling Rope", "Guaranteed hits from realms do half damage. -10% CE.",
     {{E::GuaranteedHitTakenPct, -50}, {E::CeMaxPct, -10}}},
    {"slaughter_edge", "Slaughter Edge", "+20% damage with plain attacks, -15% CE regen.",
     {{E::NormalDamagePct, 20}, {E::CeRegenPct, -15}}},
    {"soul_splitter", "Soul Splitter", "Black Spark window +2 frames.", {{E::BlackSparkWindow, 2}}},
    {"thunder_charm", "Thunder Charm", "+15% technique damage, -5% max HP.", {{E::SkillDamagePct, 15}, {E::MaxHealthPct, -5}}},
    {"silk_blindfold", "Silk Blindfold", "Parry window +3 frames.", {{E::ParryWindow, 3}}},
    {"sealed_cube", "Sealed Cube", "Start every life with half a realm meter, -15% realm gain.",
     {{E::StartRealmMeterPct, 50}, {E::RealmGainPct, -15}}},
    {"promise_ring", "Promise Ring", "+30% realm gain, -10% damage.", {{E::RealmGainPct, 30}, {E::DamagePct, -10}}},
    {"patchwork_charm", "Patchwork Charm", "Heal 120 HP whenever you get a KO.", {{E::HealOnKo, 120}}},
    {"overtime_watch", "Overtime Watch", "After 60 s alive you deal 25% more damage.", {{E::OvertimeDamagePct, 25}}},
    {"lucky_ball", "Lucky Ball", "6% chance to deal double damage.", {{E::CritChancePct, 6}}},
    {"storage_worm", "Storage Worm", "Skills come back 15% faster, -10% max HP.", {{E::CooldownPct, -15}, {E::MaxHealthPct, -10}}},
    {"thorn_talisman", "Thorn Talisman", "Melee attackers take 25% of their damage back. -5% speed.",
     {{E::ThornsPct, 25}, {E::SpeedPct, -5}}},
};

} // namespace

const std::vector<const CursedObjectDef*>& builtins() {
    static const std::vector<const CursedObjectDef*> list = [] {
        std::vector<const CursedObjectDef*> v;
        for (const CursedObjectDef& item : kItems) v.push_back(&item);
        return v;
    }();
    return list;
}

const CursedObjectDef* find(const char* id) {
    for (const CursedObjectDef* item : builtins())
        if (std::strcmp(item->id, id) == 0) return item;
    return nullptr;
}

} // namespace fc::cursed
