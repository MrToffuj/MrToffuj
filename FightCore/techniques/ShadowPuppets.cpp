// ============================================================================
// SCHATTENMENAGERIE
//  Core Rule : Schattenpool – bis zu 2 Schattenbestien gleichzeitig (Begleiter-System).
//  Skills    : Schattenhunde (Offense), Nachtschwinge (Anti-Air), Schlundkröte (Grab-Pull)
//              -> Skill beschwört die Bestie; ist sie schon da, führt sie ihren Signatur-Angriff aus.
//  MaxOutput      : Flutkoloss (Zone)
//  Realm     : Schattengarten (BuffField) – Bestien-Cooldowns laufen doppelt
//  Schwäche  : Gefallene Bestien sind für die Runde weg.
// ============================================================================
#include "Techniques.h"

#include "fightcore/Simulation.h"

namespace fc::techniques {
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

const CompanionDef& hounds() {
    static const CompanionDef d = [] {
        CompanionDef c;
        c.name = "Shadow Hounds"; c.type = CompanionType::Striker;
        c.maxHp = 180; c.upkeep = 4; c.cooldown = 150;
        c.moves = {hit("Shadow Bite", 80, 18, Fx::ratio(12, 100), Fx{}, ImpactClass::Medium),
                   hit("Twin Leap", 60, 24, Fx::ratio(5, 100), Fx::ratio(25, 100), ImpactClass::Medium)};
        c.actions = {act(AssistKind::Rush, 12, 5, 10, 0, 35), act(AssistKind::Rush, 9, 6, 8, 0, 40),
                     act(AssistKind::Rise, 12, 4, 12, 1, 28), act(AssistKind::Shield, 8, 40, 8, 0, 30)};
        return c;
    }();
    return d;
}

const CompanionDef& nightWing() {
    static const CompanionDef d = [] {
        CompanionDef c;
        c.name = "Night Wing"; c.type = CompanionType::Zoner;
        c.maxHp = 150; c.upkeep = 4; c.cooldown = 180;
        c.moves = {hit("Shadow Feather", 55, 16, Fx::ratio(10, 100), Fx{}, ImpactClass::Light),
                   hit("Diving Wing", 70, 26, Fx::ratio(5, 100), Fx::ratio(34, 100), ImpactClass::Heavy)};
        c.actions = {act(AssistKind::Shoot, 14, 1, 12, 0, 32), act(AssistKind::Shoot, 11, 1, 10, 0, 40),
                     act(AssistKind::Rise, 8, 6, 12, 1, 35), act(AssistKind::Shield, 10, 40, 10, 0, 30)};
        return c;
    }();
    return d;
}

const CompanionDef& maw() {
    static const CompanionDef d = [] {
        CompanionDef c;
        c.name = "Maw Toad"; c.type = CompanionType::Support;
        c.maxHp = 230; c.upkeep = 3; c.cooldown = 220;
        c.moves = {hit("Maw Pull", 40, 32, Fx::ratio(32, 100), Fx{}, ImpactClass::Medium, true),
                   hit("Tongue Whip", 50, 22, Fx::ratio(5, 100), Fx::ratio(20, 100), ImpactClass::Medium)};
        c.actions = {act(AssistKind::Pull, 14, 4, 14, 0, 32), act(AssistKind::Pull, 11, 4, 12, 0, 36),
                     act(AssistKind::Rise, 14, 4, 12, 1, 25), act(AssistKind::Shield, 10, 60, 10, 1, 30)};
        return c;
    }();
    return d;
}

class ShadowLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        if (move <= kCallMaw) {
            const int beast = move; // Skill-Move-Index == Bestien-Index
            int slot = c.companionSlot(static_cast<int16_t>(beast));
            if (slot >= 0) {
                // Schon da -> Signatur-Angriff: Hunde greifen an, Schwinge Anti-Air, Kröte zieht
                const AssistRole sig = beast == 0 ? AssistRole::Attack : beast == 1 ? AssistRole::AntiAir : AssistRole::Pressure;
                c.command(slot, sig);
            } else {
                c.summon(static_cast<int16_t>(beast));
            }
        } else if (move == kFloodColossus) {
            Vec3 p = c.localToWorld({Fx{}, Fx::fromInt(1), Fx::ratio(35, 10)});
            c.spawnProjectile(p, {}, Fx::ratio(25, 10), 30, kFloodColossus, 15);
            c.emit(ImpactKind::TechniqueFx, p, {}, Fx::fromInt(1), ImpactClass::Crushing, 30);
        }
    }

    bool canUseSkill(const TechContext& c, int slot) const override {
        return !c.companionLost(static_cast<int16_t>(slot)); // gefallene Bestie = Skill weg
    }

    void onRealmOpen(const TechContext& c) const override {
        for (CompanionState& comp : c.me().comp) comp.cooldown = 0;
    }
    void onRealmTick(const TechContext& c) const override {
        for (CompanionState& comp : c.me().comp)
            if (comp.cooldown > 0) --comp.cooldown;
    }
};

const ShadowLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.name = "Shadow Puppets";
    t.coreRule = "Shadow pool: up to two shadow beasts fight alongside you.";
    t.weakness = "Fallen beasts are gone for the round.";
    t.moves.resize(kShadowMoveCount);
    const char* names[3] = {"Call: Shadow Hounds", "Call: Night Wing", "Call: Maw Toad"};
    for (int i = 0; i < 3; ++i) {
        MoveDef& m = t.moves[i];
        m.name = names[i]; m.category = MoveCategory::Skill;
        m.startup = 10; m.active = 0; m.recovery = 12; m.eventFrame = 10;
    }
    {
        MoveDef& m = t.moves[kFloodColossus];
        m.name = "Flood Colossus"; m.category = MoveCategory::MaxOutput;
        m.startup = 40; m.active = 0; m.recovery = 30; m.eventFrame = 40;
        m.hit = {220, 40, 24, 16, Fx::ratio(50, 100), Fx::ratio(25, 100), ImpactClass::Crushing, false};
    }
    t.skills = {{{kCallHounds, 60, 120}, {kCallWing, 60, 120}, {kCallMaw, 60, 120}}};
    t.maxOutputMove = kFloodColossus;
    t.maxOutputType = MaxOutputType::Zone;
    t.realmClass = RealmClass::BuffField;
    t.companions = {&hounds(), &nightWing(), &maw()};
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& shadowPuppets() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
