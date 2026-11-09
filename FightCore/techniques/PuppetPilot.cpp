// Puppet Pilot
// Core rule: you fight through a remote battle frame while staying back yourself. The frame
// shows up at the start and every skill is an order to it.
// Skills: Cannon, Stray Drone (anti-air), Dash Blade. Max Output: Overdrive Frame (transform).
// Realm: Puppet Theater (lock). Weakness: frame destroyed = technique gone for the round.
#include "Common.h"
#include "Techniques.h"

namespace fc::techniques {
namespace {

enum : int { kDeployed, kOverdrive };

const CompanionDef& battleFrame() {
    static const CompanionDef d = [] {
        CompanionDef c;
        c.name = "Battle Frame";
        c.type = CompanionType::Zoner;
        c.maxHp = 320;
        c.upkeep = 3;
        c.cooldown = 70;
        MoveDef blade = move("Dash Blade", MoveCategory::Skill, 1, 0, 1);
        blade.hit = hit(90, 22, pct(30), Fx{}, ImpactClass::Heavy);
        MoveDef cannon = move("Cannon", MoveCategory::Skill, 1, 0, 1);
        cannon.hit = hit(80, 20, pct(25), Fx{}, ImpactClass::Heavy);
        MoveDef stray = move("Stray Drone", MoveCategory::Skill, 1, 0, 1);
        stray.hit = hit(60, 28, pct(5), pct(32), ImpactClass::Medium);
        c.moves = {blade, cannon, stray};
        c.actions = {act(AssistKind::Rush, 10, 5, 10, 0, 45), act(AssistKind::Shoot, 12, 1, 12, 1, 45),
                     act(AssistKind::Rise, 8, 6, 10, 2, 35), act(AssistKind::Shield, 6, 60, 8, 0, 35)};
        return c;
    }();
    return d;
}

class PuppetLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        const int slot = c.companionSlot(0);
        switch (move) {
            case kCannon: c.command(slot, AssistRole::Pressure); break;
            case kStrayDrone: c.command(slot, AssistRole::AntiAir); break;
            case kDashBlade: c.command(slot, AssistRole::Attack); break;
            case kOverdriveFrame:
                c.me().tech[kOverdrive] = 600;
                c.emit(ImpactKind::TechniqueFx, c.me().pos, {}, Fx::fromInt(1), ImpactClass::Heavy, 9);
                break;
            default: break;
        }
    }

    void onTick(const TechContext& c) const override {
        FighterState& me = c.me();
        if (!me.tech[kDeployed]) {
            me.tech[kDeployed] = 1;
            c.summon(0);
        }
        if (me.tech[kOverdrive] > 0) {
            --me.tech[kOverdrive];
            const int slot = c.companionSlot(0);
            if (slot >= 0) me.comp[slot].cooldown = 0;
        }
    }

    bool canUseSkill(const TechContext& c, int slot) const override {
        if (c.companionLost(0)) return false;
        if (slot == 3) return true;
        const int unit = c.companionSlot(0);
        return unit >= 0 && c.me().comp[unit].action < 0;
    }

    int damageTakenPct(const TechContext& c) const override { return c.me().tech[kOverdrive] > 0 ? 70 : 100; }
};

const PuppetLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.id = "puppet_pilot";
    t.name = "Puppet Pilot";
    t.coreRule = "A remote battle frame fights for you, every skill is an order to it.";
    t.weakness = "Lose the frame and the technique is gone for the round.";
    t.realmName = "Puppet Theater";
    t.moves.resize(kPuppetMoveCount);
    t.moves[kCannon] = move("Cannon", MoveCategory::Skill, 4, 0, 8, 4);
    t.moves[kStrayDrone] = move("Stray Drone", MoveCategory::Skill, 4, 0, 8, 4);
    t.moves[kDashBlade] = move("Dash Blade", MoveCategory::Skill, 4, 0, 8, 4);
    t.moves[kOverdriveFrame] = move("Overdrive Frame", MoveCategory::MaxOutput, 25, 0, 15, 25);
    t.skills = {{{kCannon, 40, 60}, {kStrayDrone, 40, 90}, {kDashBlade, 40, 90}}};
    t.maxOutputMove = kOverdriveFrame;
    t.maxOutputType = MaxOutputType::Transform;
    t.realmClass = RealmClass::Lock;
    t.companions = {&battleFrame()};
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& puppetPilot() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
