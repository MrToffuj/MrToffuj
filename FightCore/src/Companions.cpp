// Summoner assists and oath companions.
// Damage is listed at skill level, the simulation scales companion hits down to 60%.
#include "fightcore/Companion.h"

namespace fc::companions {
namespace {

MoveDef hit(const char* name, int16_t dmg, int16_t hitstun, Fx kb, Fx launch, ImpactClass impact, bool unblockable = false) {
    MoveDef m;
    m.name = name;
    m.category = MoveCategory::Skill;
    m.hit = {dmg, hitstun, static_cast<int16_t>(hitstun * 2 / 3), 6, kb, launch, impact, unblockable};
    return m;
}

AssistAction act(AssistKind k, int16_t st, int16_t ac, int16_t rec, int16_t move, int speedPct) {
    return {k, st, ac, rec, move, Fx::ratio(speedPct, 100)};
}

} // namespace

const CompanionDef& bladeWolf() {
    static const CompanionDef d = [] {
        CompanionDef c;
        c.name = "Blade Wolf"; c.type = CompanionType::Striker;
        c.maxHp = 220; c.upkeep = 4; c.cooldown = 180;
        c.moves = {hit("Bite", 80, 18, Fx::ratio(15, 100), Fx{}, ImpactClass::Medium),
                   hit("Leaping Bite", 60, 24, Fx::ratio(5, 100), Fx::ratio(25, 100), ImpactClass::Medium)};
        c.actions = {act(AssistKind::Rush, 14, 4, 10, 0, 30), act(AssistKind::Rush, 10, 6, 8, 0, 35),
                     act(AssistKind::Rise, 12, 4, 12, 1, 25), act(AssistKind::Shield, 8, 45, 8, 0, 30)};
        return c;
    }();
    return d;
}

const CompanionDef& sparkOwl() {
    static const CompanionDef d = [] {
        CompanionDef c;
        c.name = "Spark Owl"; c.type = CompanionType::Zoner;
        c.maxHp = 160; c.upkeep = 4; c.cooldown = 200;
        c.moves = {hit("Spark Feather", 60, 16, Fx::ratio(12, 100), Fx{}, ImpactClass::Light),
                   hit("Dive", 60, 24, Fx::ratio(5, 100), Fx::ratio(30, 100), ImpactClass::Medium)};
        c.actions = {act(AssistKind::Shoot, 16, 1, 14, 0, 30), act(AssistKind::Shoot, 12, 1, 10, 0, 40),
                     act(AssistKind::Rise, 10, 5, 12, 1, 30), act(AssistKind::Shield, 10, 40, 10, 0, 30)};
        return c;
    }();
    return d;
}

const CompanionDef& vineToad() {
    static const CompanionDef d = [] {
        CompanionDef c;
        c.name = "Vine Toad"; c.type = CompanionType::Support;
        c.maxHp = 240; c.upkeep = 3; c.cooldown = 240;
        c.moves = {hit("Vine Pull", 40, 30, Fx::ratio(30, 100), Fx{}, ImpactClass::Medium, true),
                   hit("Tongue Lash", 50, 24, Fx::ratio(5, 100), Fx::ratio(20, 100), ImpactClass::Medium)};
        c.actions = {act(AssistKind::Pull, 16, 4, 14, 0, 30), act(AssistKind::Pull, 12, 4, 12, 0, 35),
                     act(AssistKind::Rise, 14, 4, 12, 1, 25), act(AssistKind::Shield, 10, 60, 10, 1, 30)};
        return c;
    }();
    return d;
}

const CompanionDef& stoneWarden() {
    static const CompanionDef d = [] {
        CompanionDef c;
        c.name = "Stone Warden"; c.type = CompanionType::Guard;
        c.maxHp = 350; c.upkeep = 5; c.cooldown = 240;
        c.moves = {hit("Stone Strike", 70, 20, Fx::ratio(30, 100), Fx{}, ImpactClass::Heavy),
                   hit("Stone Pillar", 60, 26, Fx::ratio(5, 100), Fx::ratio(30, 100), ImpactClass::Heavy)};
        c.actions = {act(AssistKind::Rush, 20, 4, 16, 0, 20), act(AssistKind::Rush, 18, 4, 14, 0, 22),
                     act(AssistKind::Rise, 16, 4, 14, 1, 20), act(AssistKind::Shield, 6, 120, 10, 0, 35)};
        return c;
    }();
    return d;
}

const CompanionDef& oathSpirit() {
    static const CompanionDef d = [] {
        CompanionDef c = bladeWolf();
        c.name = "Oath Spirit";
        c.maxHp = 180; c.upkeep = 0; c.cooldown = 150;  // no upkeep, it's a gift
        return c;
    }();
    return d;
}

const CompanionDef& adaptor() {
    static const CompanionDef d = [] {
        CompanionDef c;
        c.name = "The Adaptor"; c.type = CompanionType::Guard;
        c.maxHp = 400; c.upkeep = 0; c.cooldown = 200; c.adaptive = true;
        c.moves = {hit("Adaptive Strike", 100, 22, Fx::ratio(35, 100), Fx{}, ImpactClass::Heavy),
                   hit("Adaptive Uppercut", 80, 28, Fx::ratio(5, 100), Fx::ratio(32, 100), ImpactClass::Heavy)};
        c.actions = {act(AssistKind::Rush, 16, 5, 14, 0, 28), act(AssistKind::Rush, 14, 5, 12, 0, 30),
                     act(AssistKind::Rise, 14, 5, 14, 1, 25), act(AssistKind::Shield, 6, 120, 10, 0, 35)};
        return c;
    }();
    return d;
}

} // namespace fc::companions
