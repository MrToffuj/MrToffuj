// ============================================================================
// GLUTKERN
//  Core Rule : Hitze-Stacks – Technik-Treffer setzen Stacks (max 5), der Gegner brennt.
//  Skills    : Glutfalter (Offense, Projektil), Magmasäule (Utility/Zone), Hitzemantel (Defense)
//  MaxOutput      : Sternfall (Burst, Meteor von oben)
//  Realm     : Schmelzkammer (Damage)
//  Schwäche  : Braucht Setup, langes Startup.
//  Welt      : Jeder Brand-Tick sendet Ignite-Events -> Holz in der Arena fängt Feuer.
// ============================================================================
#include "Techniques.h"

#include "fightcore/Simulation.h"

namespace fc::techniques {
namespace {

enum : int { kHeatStacks = 0, kHeatDecay = 1, kMantleTimer = 2, kBurnTarget = 3 };
constexpr int32_t kMaxStacks = 5;

class CalamityFlamesLogic final : public TechniqueLogic {
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
            case kFallingStar: {
                Vec3 p{op.pos.x, Fx::fromInt(12), op.pos.z};
                c.spawnProjectile(p, {Fx{}, Fx::ratio(-24, 100), Fx{}}, Fx::ratio(15, 10), 60, kFallingStar, 10);
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
        const int victim = me.tech[kBurnTarget];
        if (c.m.f[victim].out) { stacks = 0; return; }
        if (c.m.frame % 20 == 0) {
            c.damage(victim, static_cast<int16_t>(stacks));
            c.emit(ImpactKind::Ignite, c.m.f[victim].pos, {}, Fx::fromInt(stacks), ImpactClass::Light, 12);
        }
        if (++me.tech[kHeatDecay] >= 180) { me.tech[kHeatDecay] = 0; --stacks; }
    }

    void onHitLanded(const TechContext& c, int16_t move, bool, int victim) const override {
        if (move < 0) return; // nur Technik-Treffer
        int32_t& stacks = c.me().tech[kHeatStacks];
        if (c.me().tech[kBurnTarget] != victim) stacks = 0; // ein Opfer brennt zur Zeit
        c.me().tech[kBurnTarget] = victim;
        stacks += move == kFallingStar ? 3 : 1;
        if (stacks > kMaxStacks) stacks = kMaxStacks;
        c.me().tech[kHeatDecay] = 0;
    }

    int damageTakenPct(const TechContext& c) const override { return c.me().tech[kMantleTimer] > 0 ? 80 : 100; }
};

const CalamityFlamesLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.name = "Calamity Flames";
    t.coreRule = "Heat stacks: technique hits set the target on fire.";
    t.weakness = "Needs setup, long startup.";
    t.moves.resize(kCalamityFlamesMoveCount);
    {
        MoveDef& m = t.moves[kEmberMoths];
        m.name = "Ember Moths"; m.category = MoveCategory::Skill;
        m.startup = 18; m.active = 0; m.recovery = 20; m.eventFrame = 18;
        m.hit = {50, 18, 12, 6, Fx::ratio(15, 100), Fx{}, ImpactClass::Medium, false};
    }
    {
        MoveDef& m = t.moves[kMagmaPillar];
        m.name = "Magma Pillar"; m.category = MoveCategory::Skill;
        m.startup = 20; m.active = 0; m.recovery = 22; m.eventFrame = 20;
        m.hit = {90, 30, 16, 10, Fx::ratio(5, 100), Fx::ratio(35, 100), ImpactClass::Heavy, false};
    }
    {
        MoveDef& m = t.moves[kHeatMantle];
        m.name = "Heat Mantle"; m.category = MoveCategory::Skill;
        m.startup = 10; m.active = 0; m.recovery = 10; m.eventFrame = 10;
    }
    {
        MoveDef& m = t.moves[kFallingStar];
        m.name = "Falling Star"; m.category = MoveCategory::MaxOutput;
        m.startup = 50; m.active = 0; m.recovery = 30; m.eventFrame = 50;
        m.hit = {280, 45, 24, 18, Fx::ratio(20, 100), Fx::ratio(40, 100), ImpactClass::Crushing, false};
    }
    t.skills = {{{kEmberMoths, 70, 100}, {kMagmaPillar, 110, 200}, {kHeatMantle, 90, 360}}};
    t.maxOutputMove = kFallingStar;
    t.maxOutputType = MaxOutputType::Burst;
    t.realmClass = RealmClass::Damage;
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& calamityFlames() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
