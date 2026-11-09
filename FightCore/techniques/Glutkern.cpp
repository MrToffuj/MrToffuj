// ============================================================================
// GLUTKERN
//  Core Rule : Hitze-Stacks – Technik-Treffer setzen Stacks (max 5), der Gegner brennt.
//  Skills    : Glutfalter (Offense, Projektil), Magmasäule (Utility/Zone), Hitzemantel (Defense)
//  Apex      : Sternfall (Burst, Meteor von oben)
//  Realm     : Schmelzkammer (Damage)
//  Schwäche  : Braucht Setup, langes Startup.
//  Welt      : Jeder Brand-Tick sendet Ignite-Events -> Holz in der Arena fängt Feuer.
// ============================================================================
#include "Techniques.h"

#include "fightcore/Simulation.h"

namespace fc::techniques {
namespace {

enum : int { kHeatStacks = 0, kHeatDecay = 1, kMantleTimer = 2 };
constexpr int32_t kMaxStacks = 5;

class GlutkernLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        FighterState& me = c.me();
        const FighterState& op = c.opp();
        switch (move) {
            case kEmberMoths: {
                Vec3 p = c.localToWorld({Fx{}, Fx::ratio(13, 10), Fx::fromInt(1)});
                c.spawnProjectile(p, me.facing * Fx::ratio(22, 100), Fx::ratio(5, 10), 90, kEmberMoths);
                break;
            }
            case kMagmaPillar: {
                // Säule bricht nach 20 Frames am aktuellen Gegner-Standort aus dem Boden
                Vec3 p{op.pos.x, Fx::fromInt(1), op.pos.z};
                c.spawnProjectile(p, {}, Fx::ratio(9, 10), 12, kMagmaPillar, 20);
                c.emit(ImpactKind::TechniqueFx, p, {}, Fx{}, ImpactClass::Medium, 10); // Warn-Glühen
                break;
            }
            case kHeatMantle:
                me.tech[kMantleTimer] = 180;
                c.emit(ImpactKind::Ignite, me.pos, {}, Fx::fromInt(1), ImpactClass::Medium, 11);
                break;
            case kStarfall: {
                Vec3 p{op.pos.x, Fx::fromInt(12), op.pos.z};
                c.spawnProjectile(p, {Fx{}, Fx::ratio(-24, 100), Fx{}}, Fx::ratio(15, 10), 60, kStarfall, 10);
                break;
            }
            default: break;
        }
    }

    void onTick(const TechContext& c) const override {
        FighterState& me = c.me();
        if (me.tech[kMantleTimer] > 0) --me.tech[kMantleTimer];
        int32_t& stacks = me.tech[kHeatStacks];
        if (stacks <= 0) return;
        // Brennen: 1 Schaden pro Stack alle 20 Frames
        if (c.m.frame % 20 == 0) {
            c.damage(1 - c.self, static_cast<int16_t>(stacks));
            c.emit(ImpactKind::Ignite, c.opp().pos, {}, Fx::fromInt(stacks), ImpactClass::Light, 12);
        }
        if (++me.tech[kHeatDecay] >= 180) { me.tech[kHeatDecay] = 0; --stacks; }
    }

    void onHitLanded(const TechContext& c, int16_t move, bool) const override {
        if (move < 0) return; // nur Technik-Treffer
        int32_t& stacks = c.me().tech[kHeatStacks];
        stacks += move == kStarfall ? 3 : 1;
        if (stacks > kMaxStacks) stacks = kMaxStacks;
        c.me().tech[kHeatDecay] = 0;
    }

    int damageTakenPct(const TechContext& c) const override { return c.me().tech[kMantleTimer] > 0 ? 80 : 100; }
};

const GlutkernLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.name = "Glutkern";
    t.coreRule = "Hitze-Stacks: Technik-Treffer lassen den Gegner brennen.";
    t.weakness = "Braucht Setup, langes Startup.";
    t.moves.resize(kGlutkernMoveCount);
    {
        MoveDef& m = t.moves[kEmberMoths];
        m.name = "Glutfalter"; m.category = MoveCategory::Skill;
        m.startup = 18; m.active = 0; m.recovery = 20; m.eventFrame = 18;
        m.hit = {50, 18, 12, 6, Fx::ratio(15, 100), Fx{}, ImpactClass::Medium, false};
    }
    {
        MoveDef& m = t.moves[kMagmaPillar];
        m.name = "Magmasaeule"; m.category = MoveCategory::Skill;
        m.startup = 20; m.active = 0; m.recovery = 22; m.eventFrame = 20;
        m.hit = {90, 30, 16, 10, Fx::ratio(5, 100), Fx::ratio(35, 100), ImpactClass::Heavy, false};
    }
    {
        MoveDef& m = t.moves[kHeatMantle];
        m.name = "Hitzemantel"; m.category = MoveCategory::Skill;
        m.startup = 10; m.active = 0; m.recovery = 10; m.eventFrame = 10;
    }
    {
        MoveDef& m = t.moves[kStarfall];
        m.name = "Sternfall"; m.category = MoveCategory::Apex;
        m.startup = 50; m.active = 0; m.recovery = 30; m.eventFrame = 50;
        m.hit = {280, 45, 24, 18, Fx::ratio(20, 100), Fx::ratio(40, 100), ImpactClass::Crushing, false};
    }
    t.skills = {{{kEmberMoths, 70, 100}, {kMagmaPillar, 110, 200}, {kHeatMantle, 90, 360}}};
    t.apexMove = kStarfall;
    t.apexType = ApexType::Burst;
    t.realmClass = RealmClass::Damage;
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& glutkern() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
