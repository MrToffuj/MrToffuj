#include "Kael.h"

#include "fightcore/State.h"
#include "fightcore/Technique.h"

namespace fc::chars {
namespace {

// tech[] Layout
enum : int { kKnotTimer = 0, kKnotX = 1, kKnotY = 2, kKnotZ = 3 };
constexpr int32_t kKnotDuration = 90;

class GravityKnot final : public Technique {
public:
    void onMoveEvent(const TechContext& c, int16_t moveId) const override {
        FighterState& me = c.me();
        if (moveId == kKnot) {
            Vec3 p = c.localToWorld({Fx{}, Fx::ratio(3, 2), Fx::fromInt(4)});
            me.tech[kKnotTimer] = kKnotDuration;
            me.tech[kKnotX] = p.x.raw;
            me.tech[kKnotY] = p.y.raw;
            me.tech[kKnotZ] = p.z.raw;
            c.emit(ImpactKind::TechniqueFx, p, {}, Fx::fromInt(1), ImpactClass::Heavy, /*tag knot*/ 1);
        } else if (moveId == kRepelShot) {
            Vec3 p = c.localToWorld({Fx{}, Fx::ratio(6, 5), Fx::fromInt(1)});
            c.spawnProjectile(p, me.facing * Fx::ratio(35, 100), Fx::ratio(1, 2), 60, kRepelShot);
        }
    }

    void onTick(const TechContext& c) const override {
        FighterState& me = c.me();
        FighterState& opp = c.opp();
        if (me.tech[kKnotTimer] > 0) {
            --me.tech[kKnotTimer];
            Vec3 knot{Fx::fromRaw(me.tech[kKnotX]), Fx::fromRaw(me.tech[kKnotY]), Fx::fromRaw(me.tech[kKnotZ])};
            pull(opp, knot, Fx::fromInt(6), Fx::ratio(8, 100));
        }
        // Im eigenen Realm wird der Gegner permanent zu Kael gezogen.
        if (c.realmIsMine()) pull(opp, me.pos, Fx::fromInt(30), Fx::ratio(4, 100));
    }

    int damagePercent(const TechContext& c) const override { return c.realmIsMine() ? 125 : 100; }

private:
    static void pull(FighterState& target, Vec3 to, Fx range, Fx strength) {
        Vec3 d{to.x - target.pos.x, Fx{}, to.z - target.pos.z};
        Fx dist = length(d);
        if (dist > range || dist < Fx::ratio(1, 2)) return;
        target.pos += normalizeXZ(d, {}) * strength;
    }
};

const GravityKnot kTechnique;

CharacterDef build() {
    CharacterDef d;
    d.name = "Kael";
    d.maxHealth = 1000;
    d.walkSpeed = Fx::ratio(6, 100);
    d.backSpeed = Fx::ratio(5, 100);
    d.sideSpeed = Fx::ratio(5, 100);
    d.dashSpeed = Fx::ratio(25, 100);
    d.dashFrames = 12;
    d.jumpVelocity = Fx::ratio(30, 100);
    d.gravity = Fx::ratio(15, 1000);
    d.pushRadius = Fx::ratio(45, 100);
    d.hurtboxes = {
        {{Fx{}, Fx::ratio(5, 10), Fx{}}, Fx::ratio(45, 100)},  // Beine
        {{Fx{}, Fx::ratio(12, 10), Fx{}}, Fx::ratio(45, 100)}, // Torso
        {{Fx{}, Fx::ratio(17, 10), Fx{}}, Fx::ratio(30, 100)}, // Kopf
    };

    const uint8_t chainable = catBit(MoveCategory::Normal) | catBit(MoveCategory::Special) |
                              catBit(MoveCategory::Technique) | catBit(MoveCategory::Ultimate);
    const uint8_t intoTech = catBit(MoveCategory::Technique) | catBit(MoveCategory::Ultimate);

    d.moves.resize(kMoveCount);
    {
        MoveDef& m = d.moves[kJab];
        m.name = "Jab"; m.category = MoveCategory::Normal;
        m.startup = 5; m.active = 3; m.recovery = 8;
        m.hitboxes = {{{Fx{}, Fx::ratio(12, 10), Fx::ratio(9, 10)}, Fx::ratio(4, 10)}};
        m.hit = {40, 14, 10, 6, Fx::ratio(5, 100), Fx{}, 30, 20, ImpactClass::Light, false};
        m.cancelMask = chainable;
    }
    {
        MoveDef& m = d.moves[kHeavy];
        m.name = "Heavy Strike"; m.category = MoveCategory::Normal;
        m.startup = 12; m.active = 4; m.recovery = 18;
        m.hitboxes = {{{Fx{}, Fx::ratio(12, 10), Fx::ratio(11, 10)}, Fx::ratio(5, 10)}};
        m.hit = {90, 22, 16, 10, Fx::ratio(30, 100), Fx{}, 60, 40, ImpactClass::Heavy, false};
        m.forwardSpeed = Fx::ratio(8, 100); m.moveFirst = 6; m.moveLast = 12;
        m.cancelMask = intoTech;
    }
    {
        MoveDef& m = d.moves[kLauncher];
        m.name = "Rising Palm"; m.category = MoveCategory::Normal;
        m.startup = 10; m.active = 4; m.recovery = 22;
        m.hitboxes = {{{Fx{}, Fx::ratio(14, 10), Fx::ratio(8, 10)}, Fx::ratio(5, 10)}};
        m.hit = {70, 30, 14, 8, Fx::ratio(3, 100), Fx::ratio(32, 100), 50, 30, ImpactClass::Medium, false};
        m.cancelMask = intoTech;
    }
    {
        MoveDef& m = d.moves[kAirLight];
        m.name = "Air Kick"; m.category = MoveCategory::Normal;
        m.startup = 6; m.active = 4; m.recovery = 10;
        m.hitboxes = {{{Fx{}, Fx::ratio(8, 10), Fx::ratio(8, 10)}, Fx::ratio(45, 100)}};
        m.hit = {50, 16, 10, 6, Fx::ratio(8, 100), Fx::ratio(10, 100), 30, 20, ImpactClass::Medium, false};
        m.cancelMask = chainable;
    }
    {
        MoveDef& m = d.moves[kKnot];
        m.name = "Gravity Knot"; m.category = MoveCategory::Technique;
        m.startup = 14; m.active = 0; m.recovery = 20;
        m.essenceCost = 150; m.eventFrame = 14;
    }
    {
        MoveDef& m = d.moves[kRepelShot];
        m.name = "Repel Shot"; m.category = MoveCategory::Technique;
        m.startup = 16; m.active = 0; m.recovery = 24;
        m.essenceCost = 100; m.eventFrame = 16;
        m.hit = {60, 20, 12, 8, Fx::ratio(30, 100), Fx{}, 40, 20, ImpactClass::Heavy, false};
    }
    {
        MoveDef& m = d.moves[kRealm];
        m.name = "Realm: Event Horizon"; m.category = MoveCategory::Ultimate;
        m.startup = 40; m.active = 0; m.recovery = 30;
        m.essenceCost = kEssenceMax; m.eventFrame = 40;
        m.invulnFirst = 0; m.invulnLast = 40;
    }

    // Reihenfolge = Priorität (spezifische Richtungen zuerst)
    d.commands = {
        {0, BtnUltimate, false, kRealm},
        {6, BtnSpecial, false, kRepelShot},
        {0, BtnSpecial, false, kKnot},
        {2, BtnHeavy, false, kLauncher},
        {0, BtnHeavy, false, kHeavy},
        {0, BtnLight, false, kJab},
        {0, BtnLight, true, kAirLight},
    };
    d.technique = &kTechnique;
    return d;
}

} // namespace

const CharacterDef& kael() {
    static const CharacterDef def = build();
    return def;
}

} // namespace fc::chars
