// Light & Malice
// Core rule: moves at the speed of light, and a second technique that makes killing intent physical.
// Skills: Light Beam, Light Step, Killing Intent (freezes nearby enemies in fear for a moment).
// Max Output: Reversal Beam. Realm: Blinding Hall (damage).
// Weakness: the body can't take light speed, every skill hurts you. Costs are high on purpose.
#include "Common.h"
#include "Techniques.h"

namespace fc::techniques {
namespace {

class LightLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        FighterState& me = c.me();
        switch (move) {
            case kLightBeam:
                c.spawnBeam(c.localToWorld({Fx{}, tenths(13), tenths(6)}), me.facing, Fx::fromInt(16), tenths(35), 3, kLightBeam);
                recoil(c, 12);
                break;
            case kLightStep:
                c.emit(ImpactKind::TechniqueFx, me.pos, me.facing, Fx{}, ImpactClass::Light, 10);
                me.pos += me.facing * Fx::fromInt(me.input.current().dir <= 3 ? -6 : 6);
                recoil(c, 15);
                break;
            case kKillingIntent:
                for (int j = 0; j < c.m.count; ++j) {
                    FighterState& e = c.m.f[j];
                    if (!c.isEnemy(j) || e.out || flatDistance(me.pos, e.pos) > Fx::fromInt(7)) continue;
                    if (dot(me.facing, e.pos - me.pos) <= Fx{}) continue; // only what you're looking at
                    lockFighter(e, 40);
                }
                c.emit(ImpactKind::TechniqueFx, me.pos, me.facing, Fx::fromInt(1), ImpactClass::Heavy, 11);
                break;
            case kReversalBeam:
                c.spawnBeam(c.localToWorld({Fx{}, tenths(13), tenths(6)}), me.facing, Fx::fromInt(22), Fx::fromInt(1), 6, kReversalBeam);
                recoil(c, 60);
                break;
            default: break;
        }
    }
};

const LightLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.name = "Light & Malice";
    t.coreRule = "Light-speed attacks plus killing intent you can feel.";
    t.weakness = "Every skill tears at your own body, and they're all expensive.";
    t.realmName = "Blinding Hall";
    t.moves.resize(kLightMoveCount);
    t.moves[kLightBeam] = move("Light Beam", MoveCategory::Skill, 10, 0, 18, 10);
    t.moves[kLightBeam].hit = hit(90, 22, pct(30), Fx{}, ImpactClass::Heavy);
    t.moves[kLightStep] = move("Light Step", MoveCategory::Skill, 3, 0, 8, 3);
    t.moves[kLightStep].invulnFirst = 0;
    t.moves[kLightStep].invulnLast = 6;
    t.moves[kKillingIntent] = move("Killing Intent", MoveCategory::Skill, 10, 0, 20, 10);
    t.moves[kReversalBeam] = move("Reversal Beam", MoveCategory::MaxOutput, 40, 0, 34, 40);
    t.moves[kReversalBeam].hit = hit(320, 50, pct(70), pct(25), ImpactClass::Crushing);
    t.skills = {{{kLightBeam, 110, 150}, {kLightStep, 80, 120}, {kKillingIntent, 120, 360}}};
    t.maxOutputMove = kReversalBeam;
    t.maxOutputType = MaxOutputType::Burst;
    t.realmClass = RealmClass::Damage;
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& lightMalice() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
