// Blossom Draw
// Core rule: a counter zone around you. While guarding, melee attacks from close range get cut
// down automatically (costs CE). Skills: Draw Cut, Falling Petal (iai counter stance), Step In.
// Max Output: Absolute Counter, a long stance that counters everything but slows you down.
// Realm: Petal Field (buff field). Weakness: purely reactive, useless without a read.
#include "Common.h"
#include "Techniques.h"

namespace fc::techniques {
namespace {

enum : int { kStance, kAbsolute };
constexpr int32_t kZoneCost = 4000; // 40 CE

class BlossomLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        if (move == kFallingPetal) c.me().tech[kStance] = 40;
        if (move == kAbsoluteCounter) {
            c.me().tech[kAbsolute] = 600;
            c.emit(ImpactKind::TechniqueFx, c.me().pos, {}, Fx::fromInt(1), ImpactClass::Heavy, 6);
        }
    }

    void onTick(const TechContext& c) const override {
        int32_t* t = c.me().tech.data();
        if (t[kStance] > 0) --t[kStance];
        if (t[kAbsolute] > 0) --t[kAbsolute];
    }

    bool nullifiesIncoming(const TechContext& c, const MoveDef&, bool projectile, int attacker) const override {
        FighterState& me = c.me();
        if (me.tech[kAbsolute] > 0) {
            if (!projectile) c.strike(attacker, kIai);
            return true;
        }
        if (projectile) return false;
        if (me.tech[kStance] > 0) {
            me.tech[kStance] = 0;
            c.strike(attacker, kIai);
            return true;
        }
        const bool close = flatDistance(me.pos, c.m.f[attacker].pos) < tenths(22);
        if (me.state == FState::Guard && close && me.ce >= kZoneCost) {
            me.ce -= kZoneCost;
            c.strike(attacker, kZoneCut);
            return true;
        }
        return false;
    }

    int speedPct(const TechContext& c) const override { return c.me().tech[kAbsolute] > 0 ? 60 : 100; }
};

const BlossomLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.name = "Blossom Draw";
    t.coreRule = "Counter zone: guarding cuts down close melee attackers automatically.";
    t.weakness = "Reactive. Without a read it does nothing.";
    t.realmName = "Petal Field";
    t.moves.resize(kBlossomMoveCount);
    t.moves[kDrawCut] = melee("Draw Cut", MoveCategory::Skill, 6, 3, 14, {Fx{}, tenths(12), tenths(18)}, tenths(6),
                              hit(85, 22, pct(25), Fx{}, ImpactClass::Heavy));
    t.moves[kFallingPetal] = move("Falling Petal", MoveCategory::Skill, 3, 0, 40, 3);
    t.moves[kStepIn] = move("Step In", MoveCategory::Skill, 2, 0, 8, 2);
    t.moves[kStepIn].forwardSpeed = pct(40);
    t.moves[kStepIn].moveFirst = 1;
    t.moves[kStepIn].moveLast = 8;
    t.moves[kStepIn].invulnFirst = 1;
    t.moves[kStepIn].invulnLast = 8;
    t.moves[kAbsoluteCounter] = move("Absolute Counter", MoveCategory::MaxOutput, 20, 0, 10, 20);
    t.moves[kIai] = move("Iai", MoveCategory::Skill, 1, 0, 1);
    t.moves[kIai].hit = hit(140, 34, pct(40), Fx{}, ImpactClass::Crushing, true);
    t.moves[kIai].isCounter = true;
    t.moves[kZoneCut] = move("Zone Cut", MoveCategory::Skill, 1, 0, 1);
    t.moves[kZoneCut].hit = hit(60, 20, pct(25), Fx{}, ImpactClass::Medium, true);
    t.moves[kZoneCut].isCounter = true;
    t.skills = {{{kDrawCut, 50, 90}, {kFallingPetal, 60, 180}, {kStepIn, 30, 90}}};
    t.maxOutputMove = kAbsoluteCounter;
    t.maxOutputType = MaxOutputType::Transform;
    t.realmClass = RealmClass::BuffField;
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& blossomDraw() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
