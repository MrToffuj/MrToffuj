// Calamity Flames
// Core rule: technique hits stack heat (max 5) and the target burns. Burn ticks emit Ignite
// events so wooden scenery catches fire too.
// Skills: Ember Moths, Magma Pillar, Heat Mantle. Max Output: Falling Star. Realm: Iron Coffin.
// Weakness: needs setup, long startups.
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
                // erupts under the target after 20 frames
                Vec3 p{op.pos.x, Fx::fromInt(1), op.pos.z};
                c.spawnProjectile(p, {}, Fx::ratio(9, 10), 12, kMagmaPillar, 20);
                c.emit(ImpactKind::TechniqueFx, p, {}, Fx{}, ImpactClass::Medium, 10);
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
        // 1 damage per stack every 20 frames
        const int victim = me.tech[kBurnTarget];
        if (c.m.f[victim].out) { stacks = 0; return; }
        if (c.m.frame % 20 == 0) {
            c.damage(victim, static_cast<int16_t>(stacks));
            c.emit(ImpactKind::Ignite, c.m.f[victim].pos, {}, Fx::fromInt(stacks), ImpactClass::Light, 12);
        }
        if (++me.tech[kHeatDecay] >= 180) { me.tech[kHeatDecay] = 0; --stacks; }
    }

    void onHitLanded(const TechContext& c, int16_t move, bool, int victim) const override {
        if (move < 0) return;
        int32_t& stacks = c.me().tech[kHeatStacks];
        if (c.me().tech[kBurnTarget] != victim) stacks = 0;  // only one victim burns at a time
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
