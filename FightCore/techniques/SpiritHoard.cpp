// Spirit Hoard
// Core rule: a pool of up to three absorbed spirits that fight for you (companion system).
// Skills: Prism Serpent, Release (calls the other spirits), Absorb (eats projectiles and brings a
// lost spirit back). Max Output: Spiral Collapse, spends every spirit on the field for one big shot.
// Realm: Spirit Nest (debuff). Weakness: an empty pool leaves you with nothing, spirits can be killed.
#include "Common.h"
#include "Techniques.h"

namespace fc::techniques {
namespace {

enum : int { kAbsorbTimer, kNextRelease, kConsumed };
enum Spirit : int16_t { kSerpent, kSwarm, kGrasp };

CompanionDef spirit(const char* name, CompanionType type, int16_t hp, MoveDef a, MoveDef b, std::array<AssistAction, 4> actions) {
    CompanionDef c;
    c.name = name;
    c.type = type;
    c.maxHp = hp;
    c.upkeep = 4;
    c.cooldown = 150;
    c.moves = {a, b};
    c.actions = actions;
    return c;
}

MoveDef spiritHit(const char* name, HitProps h) {
    MoveDef m = move(name, MoveCategory::Skill, 1, 0, 1);
    m.hit = h;
    return m;
}

const CompanionDef& serpent() {
    static const CompanionDef d = spirit("Prism Serpent", CompanionType::Striker, 260,
        spiritHit("Coil Strike", hit(100, 24, pct(35), Fx{}, ImpactClass::Heavy)),
        spiritHit("Rising Coil", hit(70, 28, pct(5), pct(30), ImpactClass::Heavy)),
        {act(AssistKind::Rush, 14, 6, 12, 0, 34), act(AssistKind::Rush, 10, 6, 10, 0, 40),
         act(AssistKind::Rise, 12, 5, 12, 1, 30), act(AssistKind::Shield, 8, 60, 8, 0, 30)});
    return d;
}
const CompanionDef& swarm() {
    static const CompanionDef d = spirit("Swarm Spirit", CompanionType::Zoner, 140,
        spiritHit("Swarm", hit(50, 16, pct(10), Fx{}, ImpactClass::Light)),
        spiritHit("Swarm Lift", hit(50, 24, pct(5), pct(28), ImpactClass::Medium)),
        {act(AssistKind::Shoot, 12, 1, 12, 0, 34), act(AssistKind::Shoot, 10, 1, 10, 0, 40),
         act(AssistKind::Rise, 10, 5, 12, 1, 30), act(AssistKind::Shield, 10, 40, 10, 0, 30)});
    return d;
}
const CompanionDef& grasp() {
    static const CompanionDef d = spirit("Grasping Spirit", CompanionType::Support, 220,
        spiritHit("Grasp", hit(40, 32, pct(-30), Fx{}, ImpactClass::Medium, true)),
        spiritHit("Slam", hit(60, 24, pct(5), pct(22), ImpactClass::Medium)),
        {act(AssistKind::Pull, 14, 4, 14, 0, 32), act(AssistKind::Pull, 11, 4, 12, 0, 36),
         act(AssistKind::Rise, 14, 4, 12, 1, 25), act(AssistKind::Shield, 10, 60, 10, 1, 30)});
    return d;
}

void callSpirit(const TechContext& c, int16_t which, AssistRole role) {
    const int slot = c.companionSlot(which);
    if (slot >= 0) c.command(slot, role);
    else c.summon(which);
}

class HoardLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        FighterState& me = c.me();
        switch (move) {
            case kPrismSerpent:
                callSpirit(c, kSerpent, AssistRole::Attack);
                break;
            case kRelease: {
                // alternate between the two smaller spirits, skip the dead one
                int16_t which = me.tech[kNextRelease] == 0 ? kSwarm : kGrasp;
                if (c.companionLost(which)) which = which == kSwarm ? kGrasp : kSwarm;
                me.tech[kNextRelease] ^= 1;
                callSpirit(c, which, AssistRole::Pressure);
                break;
            }
            case kAbsorb:
                me.tech[kAbsorbTimer] = 45;
                break;
            case kSpiralCollapse: {
                int consumed = 0;
                for (int16_t s = kSerpent; s <= kGrasp; ++s) {
                    const int slot = c.companionSlot(s);
                    if (slot < 0) continue;
                    me.comp[slot] = CompanionState{};
                    me.lostMask = static_cast<uint8_t>(me.lostMask | (1u << (c.setup.techRosterStart[c.self] + s)));
                    ++consumed;
                }
                me.tech[kConsumed] = consumed;
                c.spawnProjectile(c.localToWorld({Fx{}, tenths(13), tenths(15)}), me.facing * pct(30), tenths(12), 90, kSpiralCollapse);
                break;
            }
            default: break;
        }
    }

    void onTick(const TechContext& c) const override {
        if (c.me().tech[kAbsorbTimer] > 0) --c.me().tech[kAbsorbTimer];
    }

    bool canUseSkill(const TechContext& c, int slot) const override {
        if (slot == 0) return !c.companionLost(kSerpent);
        if (slot == 1) return !c.companionLost(kSwarm) || !c.companionLost(kGrasp);
        if (slot == 3) return c.companionSlot(kSerpent) >= 0 || c.companionSlot(kSwarm) >= 0 || c.companionSlot(kGrasp) >= 0;
        return true;
    }

    bool nullifiesIncoming(const TechContext& c, const MoveDef&, bool projectile, int) const override {
        FighterState& me = c.me();
        if (!projectile || me.tech[kAbsorbTimer] <= 0) return false;
        // swallow it and get one fallen spirit back
        for (int16_t s = kSerpent; s <= kGrasp; ++s) {
            const uint8_t bit = static_cast<uint8_t>(1u << (c.setup.techRosterStart[c.self] + s));
            if (me.lostMask & bit) { me.lostMask = static_cast<uint8_t>(me.lostMask & ~bit); break; }
        }
        c.emit(ImpactKind::TechniqueFx, me.pos, {}, Fx{}, ImpactClass::Medium, 7);
        return true;
    }

    int damageDealtPct(const TechContext& c, int16_t move) const override {
        return move == kSpiralCollapse ? 100 + 60 * c.me().tech[kConsumed] : 100;
    }
};

const HoardLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.id = "spirit_hoard";
    t.name = "Spirit Hoard";
    t.coreRule = "Up to three absorbed spirits fight for you.";
    t.weakness = "An empty pool leaves you bare, and spirits can be killed.";
    t.realmName = "Spirit Nest";
    t.moves.resize(kHoardMoveCount);
    t.moves[kPrismSerpent] = move("Prism Serpent", MoveCategory::Skill, 10, 0, 12, 10);
    t.moves[kRelease] = move("Release", MoveCategory::Skill, 10, 0, 12, 10);
    t.moves[kAbsorb] = move("Absorb", MoveCategory::Skill, 3, 0, 30, 3);
    t.moves[kSpiralCollapse] = move("Spiral Collapse", MoveCategory::MaxOutput, 45, 0, 30, 45);
    t.moves[kSpiralCollapse].hit = hit(120, 42, pct(55), pct(20), ImpactClass::Crushing);
    t.skills = {{{kPrismSerpent, 80, 150}, {kRelease, 60, 150}, {kAbsorb, 50, 240}}};
    t.maxOutputMove = kSpiralCollapse;
    t.maxOutputType = MaxOutputType::Burst;
    t.realmClass = RealmClass::Debuff;
    t.companions = {&serpent(), &swarm(), &grasp()};
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& spiritHoard() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
