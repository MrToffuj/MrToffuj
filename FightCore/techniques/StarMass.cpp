// Star Mass
// Core rule: plain hits pile up virtual mass (max 5), every attack hits harder with it.
// Skills: Mass Slam (spends the mass), Weightless, Star Wing (a striker companion).
// Max Output: Black Hole. Realm: Gravity Well (damage). Weakness: lots of setup, hungry for CE.
#include "Common.h"
#include "Techniques.h"

namespace fc::techniques {
namespace {

enum : int { kMass, kFloat, kSpent, kHoleX, kHoleZ, kHoleTimer };

const CompanionDef& starWing() {
    static const CompanionDef d = [] {
        CompanionDef c;
        c.name = "Star Wing";
        c.type = CompanionType::Striker;
        c.maxHp = 200;
        c.upkeep = 5;
        c.cooldown = 160;
        MoveDef talon = move("Talon", MoveCategory::Skill, 1, 0, 1);
        talon.hit = hit(90, 22, pct(30), pct(10), ImpactClass::Heavy);
        MoveDef dive = move("Dive", MoveCategory::Skill, 1, 0, 1);
        dive.hit = hit(70, 26, pct(5), pct(30), ImpactClass::Heavy);
        c.moves = {talon, dive};
        c.actions = {act(AssistKind::Rush, 12, 5, 12, 0, 38), act(AssistKind::Rush, 9, 5, 10, 0, 42),
                     act(AssistKind::Rise, 10, 5, 12, 1, 32), act(AssistKind::Shield, 8, 40, 8, 0, 30)};
        return c;
    }();
    return d;
}

class StarLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        FighterState& me = c.me();
        switch (move) {
            case kMassSlam:
                me.tech[kSpent] = me.tech[kMass];
                me.tech[kMass] = 0;
                break;
            case kWeightless:
                me.tech[kFloat] = 120;
                me.tech[kMass] = clampStacks(me.tech[kMass] + 2, 5);
                break;
            case kStarWingCall: {
                const int slot = c.companionSlot(0);
                if (slot >= 0) c.command(slot, AssistRole::Attack);
                else c.summon(0);
                break;
            }
            case kBlackHole: {
                const Vec3 at = c.opp().pos;
                me.tech[kHoleX] = at.x.raw;
                me.tech[kHoleZ] = at.z.raw;
                me.tech[kHoleTimer] = 45;
                c.spawnProjectile(at + Vec3{Fx{}, Fx::fromInt(1), Fx{}}, {}, tenths(25), 20, kBlackHole, 30);
                break;
            }
            default: break;
        }
    }

    void onTick(const TechContext& c) const override {
        int32_t* t = c.me().tech.data();
        if (t[kFloat] > 0) --t[kFloat];
        if (t[kHoleTimer] > 0) {
            --t[kHoleTimer];
            const Vec3 hole{Fx::fromRaw(t[kHoleX]), Fx{}, Fx::fromRaw(t[kHoleZ])};
            for (int j = 0; j < c.m.count; ++j)
                if (c.isEnemy(j)) pullToward(c.m.f[j], hole, Fx::fromInt(8), pct(12));
        }
    }

    void onHitLanded(const TechContext& c, int16_t move, bool, int) const override {
        if (move < 0) c.me().tech[kMass] = clampStacks(c.me().tech[kMass] + 1, 5);
    }

    bool canUseSkill(const TechContext& c, int slot) const override { return slot != 2 || !c.companionLost(0); }

    int damageDealtPct(const TechContext& c, int16_t move) const override {
        if (move == kMassSlam) return 100 + 30 * c.me().tech[kSpent];
        return 100 + 8 * c.me().tech[kMass];
    }
    int speedPct(const TechContext& c) const override { return c.me().tech[kFloat] > 0 ? 150 : 100; }
};

const StarLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.name = "Star Mass";
    t.coreRule = "Plain hits pile up virtual mass, which makes everything hit harder.";
    t.weakness = "Takes setup and eats CE.";
    t.realmName = "Gravity Well";
    t.moves.resize(kStarMoveCount);
    t.moves[kMassSlam] = melee("Mass Slam", MoveCategory::Skill, 14, 4, 22, {Fx{}, Fx::fromInt(1), tenths(12)}, tenths(8),
                               hit(100, 30, pct(40), Fx{}, ImpactClass::Heavy));
    t.moves[kMassSlam].eventFrame = 1;
    t.moves[kWeightless] = move("Weightless", MoveCategory::Skill, 2, 0, 8, 2);
    t.moves[kStarWingCall] = move("Star Wing", MoveCategory::Skill, 8, 0, 10, 8);
    t.moves[kBlackHole] = move("Black Hole", MoveCategory::MaxOutput, 45, 0, 30, 45);
    t.moves[kBlackHole].hit = hit(280, 45, pct(20), pct(35), ImpactClass::Crushing);
    t.skills = {{{kMassSlam, 110, 150}, {kWeightless, 90, 240}, {kStarWingCall, 100, 200}}};
    t.maxOutputMove = kBlackHole;
    t.maxOutputType = MaxOutputType::Burst;
    t.realmClass = RealmClass::Damage;
    t.companions = {&starWing()};
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& starMass() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
