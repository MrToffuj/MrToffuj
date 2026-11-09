// Thunder Vessel
// Core rule: hits store charge (max 5), Bolt releases it.
// Skills: Bolt (beam), Jolt Dash, Arc Counter. Max Output: Amber Form, a short transformation that
// burns you out afterwards and leaves you open. Realm: Storm Hall (buff field).
#include "Common.h"
#include "Techniques.h"

namespace fc::techniques {
namespace {

enum : int { kCharge, kAmber, kCounter, kSpent, kExposed };

class ThunderLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        FighterState& me = c.me();
        switch (move) {
            case kBolt:
                me.tech[kSpent] = me.tech[kCharge];
                me.tech[kCharge] = 0;
                c.spawnBeam(c.localToWorld({Fx{}, tenths(12), tenths(6)}), me.facing, Fx::fromInt(10), tenths(4), 4, kBolt);
                break;
            case kJoltDash:
                me.tech[kCharge] = clampStacks(me.tech[kCharge] + 1, 5);
                break;
            case kArcCounter:
                me.tech[kCounter] = 30;
                break;
            case kAmberForm:
                me.tech[kAmber] = 480;
                me.tech[kCharge] = 5;
                c.emit(ImpactKind::TechniqueFx, me.pos, {}, Fx::fromInt(1), ImpactClass::Crushing, 3);
                break;
            default: break;
        }
    }

    void onTick(const TechContext& c) const override {
        int32_t* t = c.me().tech.data();
        if (t[kCounter] > 0) --t[kCounter];
        if (t[kExposed] > 0) --t[kExposed];
        if (t[kAmber] > 0) {
            t[kCharge] = 5;
            if (--t[kAmber] == 0) {
                c.burnOut(300); // the body gives out
                t[kExposed] = 300;
            }
        }
    }

    void onHitLanded(const TechContext& c, int16_t move, bool, int) const override {
        if (move != kBolt) c.me().tech[kCharge] = clampStacks(c.me().tech[kCharge] + 1, 5);
    }

    bool nullifiesIncoming(const TechContext& c, const MoveDef&, bool projectile, int attacker) const override {
        if (projectile || c.me().tech[kCounter] <= 0) return false;
        c.me().tech[kCounter] = 0;
        c.strike(attacker, kArcStrike);
        return true;
    }

    int damageDealtPct(const TechContext& c, int16_t move) const override {
        int p = c.me().tech[kAmber] > 0 ? 140 : 100;
        if (move == kBolt) p = p * (100 + 30 * c.me().tech[kSpent]) / 100;
        return p;
    }
    int damageTakenPct(const TechContext& c) const override { return c.me().tech[kExposed] > 0 ? 125 : 100; }
    int speedPct(const TechContext& c) const override { return c.me().tech[kAmber] > 0 ? 140 : 100; }
};

const ThunderLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.name = "Thunder Vessel";
    t.coreRule = "Hits store electric charge, Bolt releases all of it.";
    t.weakness = "Amber Form runs out, then you're burned out and wide open.";
    t.realmName = "Storm Hall";
    t.moves.resize(kThunderMoveCount);
    t.moves[kBolt] = move("Bolt", MoveCategory::Skill, 12, 0, 18, 12);
    t.moves[kBolt].hit = hit(55, 20, pct(25), Fx{}, ImpactClass::Medium);
    t.moves[kJoltDash] = move("Jolt Dash", MoveCategory::Skill, 1, 0, 12, 1);
    t.moves[kJoltDash].forwardSpeed = pct(45);
    t.moves[kJoltDash].moveFirst = 1;
    t.moves[kJoltDash].moveLast = 10;
    t.moves[kJoltDash].invulnFirst = 1;
    t.moves[kJoltDash].invulnLast = 10;
    t.moves[kArcCounter] = move("Arc Counter", MoveCategory::Skill, 2, 0, 28, 2);
    t.moves[kAmberForm] = move("Amber Form", MoveCategory::MaxOutput, 30, 0, 20, 30);
    t.moves[kAmberForm].invulnFirst = 0;
    t.moves[kAmberForm].invulnLast = 30;
    t.moves[kArcStrike] = move("Arc Strike", MoveCategory::Skill, 1, 0, 1);
    t.moves[kArcStrike].hit = hit(80, 30, pct(30), Fx{}, ImpactClass::Heavy);
    t.moves[kArcStrike].isCounter = true;
    t.skills = {{{kBolt, 70, 120}, {kJoltDash, 40, 90}, {kArcCounter, 50, 240}}};
    t.maxOutputMove = kAmberForm;
    t.maxOutputType = MaxOutputType::Transform;
    t.realmClass = RealmClass::BuffField;
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& thunderVessel() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
