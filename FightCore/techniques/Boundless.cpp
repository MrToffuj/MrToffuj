// Boundless
// Core rule: Infinite Gap, auto-blocks projectiles and lights while there's CE left.
// Skills: Repel, Attract, Fold Step. Max Output: Singularity. Realm: Endless Void (lock).
// Weakness: out of CE means no gap, and grabs always get through.
#include "Techniques.h"

#include "fightcore/Simulation.h"

namespace fc::techniques {
namespace {

enum : int { kKnotTimer = 0, kKnotX = 1, kKnotZ = 2 };
constexpr int32_t kFieldCost = 3000;  // 30 CE per blocked hit

class BoundlessLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        FighterState& me = c.me();
        switch (move) {
            case kAttract: {
                Vec3 p = c.localToWorld({Fx{}, Fx{}, Fx::fromInt(4)});
                me.tech[kKnotTimer] = 90;
                me.tech[kKnotX] = p.x.raw;
                me.tech[kKnotZ] = p.z.raw;
                c.emit(ImpactKind::TechniqueFx, p, {}, Fx::fromInt(1), ImpactClass::Heavy, 1);
                break;
            }
            case kFoldStep: {
                // blink 4 m forward, or back if the stick points back
                Fx dist = Fx::fromInt(me.input.current().dir <= 3 ? -4 : 4);
                c.emit(ImpactKind::TechniqueFx, me.pos, {}, Fx{}, ImpactClass::Light, 2);
                me.pos += me.facing * dist;
                c.emit(ImpactKind::TechniqueFx, me.pos, {}, Fx{}, ImpactClass::Light, 2);
                break;
            }
            case kSingularity: {
                Vec3 p = c.localToWorld({Fx{}, Fx::ratio(13, 10), Fx::ratio(15, 10)});
                c.spawnProjectile(p, me.facing * Fx::ratio(20, 100), Fx::fromInt(1), 120, kSingularity);
                break;
            }
            default: break;
        }
    }

    void onTick(const TechContext& c) const override {
        FighterState& me = c.me();
        if (me.tech[kKnotTimer] > 0) {
            --me.tech[kKnotTimer];
            pull(c.opp(), {Fx::fromRaw(me.tech[kKnotX]), Fx{}, Fx::fromRaw(me.tech[kKnotZ])}, Fx::fromInt(6),
                 Fx::ratio(8, 100));
        }
    }

    void onRealmTick(const TechContext& c) const override {
        // Endless Void keeps pulling the target in
        pull(c.opp(), c.me().pos, Fx::fromInt(30), Fx::ratio(3, 100));
    }

    bool nullifiesIncoming(const TechContext& c, const MoveDef& mv, bool projectile, int) const override {
        FighterState& me = c.me();
        if (me.burnout > 0 || me.ce < kFieldCost) return false;
        const bool light = mv.category == MoveCategory::Normal && mv.hit.impact == ImpactClass::Light;
        if (!projectile && !light) return false;
        me.ce -= kFieldCost;
        return true;
    }

private:
    static void pull(FighterState& target, Vec3 to, Fx range, Fx strength) {
        Vec3 d{to.x - target.pos.x, Fx{}, to.z - target.pos.z};
        Fx dist = length(d);
        if (dist > range || dist < Fx::ratio(1, 2)) return;
        target.pos += normalizeXZ(d, {}) * strength;
    }
};

const BoundlessLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.name = "Boundless";
    t.coreRule = "Infinite Gap: auto-blocks projectiles and lights at a CE cost.";
    t.weakness = "No CE, no gap. Grabs always go through.";
    t.moves.resize(kBoundlessMoveCount);
    {
        MoveDef& m = t.moves[kRepel];
        m.name = "Repel"; m.category = MoveCategory::Skill;
        m.startup = 11; m.active = 3; m.recovery = 20;
        m.hitboxes = {{{Fx{}, Fx::ratio(12, 10), Fx::ratio(12, 10)}, Fx::ratio(8, 10)}};
        m.hit = {80, 24, 14, 10, Fx::ratio(55, 100), Fx::ratio(8, 100), ImpactClass::Heavy, false};
    }
    {
        MoveDef& m = t.moves[kAttract];
        m.name = "Attract"; m.category = MoveCategory::Skill;
        m.startup = 14; m.active = 0; m.recovery = 18; m.eventFrame = 14;
    }
    {
        MoveDef& m = t.moves[kFoldStep];
        m.name = "Fold Step"; m.category = MoveCategory::Skill;
        m.startup = 6; m.active = 0; m.recovery = 10; m.eventFrame = 6;
        m.invulnFirst = 4; m.invulnLast = 8;
    }
    {
        MoveDef& m = t.moves[kSingularity];
        m.name = "Singularity"; m.category = MoveCategory::MaxOutput;
        m.startup = 45; m.active = 0; m.recovery = 30; m.eventFrame = 45;
        m.hit = {300, 40, 20, 16, Fx::ratio(60, 100), Fx::ratio(20, 100), ImpactClass::Crushing, false};
    }
    t.skills = {{{kRepel, 80, 90}, {kAttract, 120, 240}, {kFoldStep, 60, 120}}};
    t.maxOutputMove = kSingularity;
    t.maxOutputType = MaxOutputType::Burst;
    t.realmClass = RealmClass::Lock;
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& boundless() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
