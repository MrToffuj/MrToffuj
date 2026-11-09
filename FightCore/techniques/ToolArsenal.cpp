// Tool Arsenal (Heaven's Bind only)
// No technique and no CE, a bag of cursed tools instead. Using a tool equips it, switching is free.
// The equipped tool boosts plain attacks. Skills: Cloud Staff, Endless Chain (range grab),
// Reversal Spear (shuts down the victim's technique). Max Output: Tool Storm.
// Instead of a realm: Kill Window, a short lock on the target where everything hits harder.
#include "Common.h"
#include "Techniques.h"

namespace fc::techniques {
namespace {

enum : int { kTool, kKillWindow };
enum Tool : int { kBlade, kStaff, kChainTool, kSpearTool };

class ArsenalLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        FighterState& me = c.me();
        switch (move) {
            case kCloudStaff: me.tech[kTool] = kStaff; break;
            case kReversalSpear: me.tech[kTool] = kSpearTool; break;
            case kEndlessChain:
                me.tech[kTool] = kChainTool;
                c.spawnBeam(c.localToWorld({Fx{}, tenths(12), tenths(6)}), me.facing, Fx::fromInt(9), tenths(45), 5, kEndlessChain);
                break;
            default: break;
        }
    }

    void onTick(const TechContext& c) const override {
        if (c.me().tech[kKillWindow] > 0) --c.me().tech[kKillWindow];
    }

    // Kill Window
    void onRealmOpen(const TechContext& c) const override {
        FighterState& me = c.me();
        if (!c.hasTarget() || flatDistance(me.pos, c.opp().pos) > Fx::fromInt(12)) return;
        lockFighter(c.opp(), 90);
        me.tech[kKillWindow] = 180;
        c.emit(ImpactKind::TechniqueFx, c.opp().pos, {}, Fx::fromInt(1), ImpactClass::Crushing, 4);
    }

    void onHitLanded(const TechContext& c, int16_t move, bool, int victim) const override {
        if (move != kReversalSpear) return;
        // cancels whatever technique the victim has going
        FighterState& v = c.m.f[victim];
        v.realm = RealmState{};
        if (v.burnout < 180) v.burnout = 180;
        v.reinforce = false;
        c.emit(ImpactKind::TechniqueFx, v.pos, {}, Fx{}, ImpactClass::Heavy, 5);
    }

    int damageDealtPct(const TechContext& c, int16_t move) const override {
        static const int toolBonus[4] = {110, 125, 110, 115};
        int p = move < 0 ? toolBonus[c.me().tech[kTool] & 3] : 100;
        if (c.me().tech[kKillWindow] > 0) p = p * 160 / 100;
        return p;
    }
};

const ArsenalLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.name = "Tool Arsenal";
    t.coreRule = "No CE, a wheel of cursed tools. Using one equips it and boosts your plain attacks.";
    t.weakness = "No CE and no Reverse Flow, ranged builds can keep you out.";
    t.realmName = "Kill Window";
    t.ceFree = true;
    t.replacesRealm = true;
    t.moves.resize(kArsenalMoveCount);
    t.moves[kCloudStaff] = melee("Cloud Staff", MoveCategory::Skill, 8, 5, 14, {Fx{}, tenths(12), tenths(14)}, tenths(7),
                                 hit(110, 26, pct(35), Fx{}, ImpactClass::Heavy));
    t.moves[kCloudStaff].eventFrame = 1;
    t.moves[kEndlessChain] = move("Endless Chain", MoveCategory::Skill, 10, 0, 20, 10);
    t.moves[kEndlessChain].hit = hit(50, 30, pct(-40), Fx{}, ImpactClass::Medium);
    t.moves[kReversalSpear] = melee("Reversal Spear", MoveCategory::Skill, 10, 3, 16, {Fx{}, tenths(12), tenths(13)}, tenths(5),
                                    hit(90, 24, pct(25), Fx{}, ImpactClass::Heavy));
    t.moves[kReversalSpear].eventFrame = 1;
    t.moves[kToolStorm] = melee("Tool Storm", MoveCategory::MaxOutput, 20, 30, 30, {Fx{}, tenths(12), tenths(12)}, tenths(16),
                                hit(260, 45, pct(60), pct(25), ImpactClass::Crushing));
    t.skills = {{{kCloudStaff, 0, 100}, {kEndlessChain, 0, 160}, {kReversalSpear, 0, 200}}};
    t.maxOutputMove = kToolStorm;
    t.maxOutputType = MaxOutputType::Burst;
    t.realmClass = RealmClass::Lock;
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& toolArsenal() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
