// Granite Cannon
// Core rule: every landed hit adds an output stack (max 5), the beams spend them.
// Skills: Granite Beam, Shockwave (anti-air), Charge Guard. Max Output: Full Output (burst).
// Realm: Granite Furnace (damage). Weakness: the charge is easy to read and the body is slow.
#include "Common.h"
#include "Techniques.h"

namespace fc::techniques {
namespace {

enum : int { kStacks, kGuard, kSpent };
constexpr int kMaxStacks = 5;

class GraniteLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        FighterState& me = c.me();
        switch (move) {
            case kGraniteBeam:
                me.tech[kSpent] = me.tech[kStacks];
                me.tech[kStacks] = 0;
                c.spawnBeam(c.localToWorld({Fx{}, tenths(12), tenths(8)}), me.facing, Fx::fromInt(12), tenths(5), 6, kGraniteBeam);
                break;
            case kChargeGuard:
                me.tech[kGuard] = 90;
                break;
            case kFullOutput:
                me.tech[kSpent] = me.tech[kStacks] + 3;
                me.tech[kStacks] = 0;
                c.spawnBeam(c.localToWorld({Fx{}, tenths(12), tenths(8)}), me.facing, Fx::fromInt(20), Fx::fromInt(1), 8, kFullOutput);
                c.emit(ImpactKind::TechniqueFx, me.pos, me.facing, Fx::fromInt(me.tech[kSpent]), ImpactClass::Crushing, 1);
                break;
            default: break;
        }
    }

    void onTick(const TechContext& c) const override {
        int32_t* t = c.me().tech.data();
        if (t[kGuard] > 0 && --t[kGuard] % 20 == 0) t[kStacks] = clampStacks(t[kStacks] + 1, kMaxStacks);
    }

    void onHitLanded(const TechContext& c, int16_t move, bool, int) const override {
        if (move == kGraniteBeam || move == kFullOutput) return;
        c.me().tech[kStacks] = clampStacks(c.me().tech[kStacks] + 1, kMaxStacks);
    }

    int damageDealtPct(const TechContext& c, int16_t move) const override {
        if (move == kGraniteBeam || move == kFullOutput) return 100 + 25 * c.me().tech[kSpent];
        return 100;
    }
    int damageTakenPct(const TechContext& c) const override { return c.me().tech[kGuard] > 0 ? 60 : 100; }
    int speedPct(const TechContext& c) const override { return c.me().tech[kGuard] > 0 ? 30 : 85; }
};

const GraniteLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.id = "granite_cannon";
    t.name = "Granite Cannon";
    t.coreRule = "Landed hits charge output stacks, beams spend them.";
    t.weakness = "Easy to read charge, poor mobility.";
    t.realmName = "Granite Furnace";
    t.moves.resize(kGraniteMoveCount);
    t.moves[kGraniteBeam] = move("Granite Beam", MoveCategory::Skill, 18, 0, 26, 18);
    t.moves[kGraniteBeam].hit = hit(70, 24, pct(35), Fx{}, ImpactClass::Heavy);
    t.moves[kShockwave] = melee("Shockwave", MoveCategory::Skill, 9, 4, 18, {Fx{}, Fx::fromInt(2), tenths(6)}, tenths(12),
                                hit(60, 28, pct(5), pct(30), ImpactClass::Heavy));
    t.moves[kChargeGuard] = move("Charge Guard", MoveCategory::Skill, 4, 0, 6, 4);
    t.moves[kFullOutput] = move("Full Output", MoveCategory::MaxOutput, 50, 0, 36, 50);
    t.moves[kFullOutput].hit = hit(200, 45, pct(70), pct(20), ImpactClass::Crushing);
    t.skills = {{{kGraniteBeam, 90, 150}, {kShockwave, 60, 120}, {kChargeGuard, 40, 240}}};
    t.maxOutputMove = kFullOutput;
    t.maxOutputType = MaxOutputType::Burst;
    t.realmClass = RealmClass::Damage;
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& graniteCannon() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
