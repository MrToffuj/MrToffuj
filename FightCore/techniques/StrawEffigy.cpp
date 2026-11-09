// Straw Effigy
// Core rule: nails leave resonance marks on whoever they hit; marks can be set off later from any range.
// Skills: Nail, Pin Burst (detonates every mark), Decoy Doll (takes the next hit for you).
// Max Output: Full Resonance. Realm: Resonance Chamber (debuff). Weakness: fragile, marks are visible.
#include "Common.h"
#include "Techniques.h"

namespace fc::techniques {
namespace {

enum : int { kMarks, kDecoy };

class EffigyLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        FighterState& me = c.me();
        switch (move) {
            case kNail:
                c.spawnProjectile(c.localToWorld({Fx{}, tenths(13), tenths(7)}), me.facing * pct(45), tenths(3), 60, kNail);
                break;
            case kPinBurst: detonate(c, kResonance); break;
            case kDecoyDoll: me.tech[kDecoy] = 180; break;
            case kFullResonance: detonate(c, kTotalResonance); break;
            default: break;
        }
    }

    void onTick(const TechContext& c) const override {
        if (c.me().tech[kDecoy] > 0) --c.me().tech[kDecoy];
        // marks fade off fighters that went down
        for (int j = 0; j < c.m.count; ++j)
            if (c.m.f[j].out) c.me().tech[kMarks] &= ~(1 << j);
    }

    void onHitLanded(const TechContext& c, int16_t move, bool, int victim) const override {
        if (move == kNail) c.me().tech[kMarks] |= 1 << victim;
    }

    bool nullifiesIncoming(const TechContext& c, const MoveDef&, bool, int) const override {
        if (c.me().tech[kDecoy] <= 0) return false;
        c.me().tech[kDecoy] = 0;
        c.emit(ImpactKind::TechniqueFx, c.me().pos, {}, Fx{}, ImpactClass::Medium, 8);
        return true;
    }

    int damageTakenPct(const TechContext&) const override { return 110; }

private:
    static void detonate(const TechContext& c, int16_t move) {
        const int32_t marks = c.me().tech[kMarks];
        c.me().tech[kMarks] = 0;
        for (int j = 0; j < c.m.count; ++j)
            if (marks & (1 << j)) c.strike(j, move);
    }
};

const EffigyLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.name = "Straw Effigy";
    t.coreRule = "Nails mark whoever they hit, marks detonate on command from any range.";
    t.weakness = "Fragile, and the marks are easy to see.";
    t.realmName = "Resonance Chamber";
    t.moves.resize(kEffigyMoveCount);
    t.moves[kNail] = move("Nail", MoveCategory::Skill, 10, 0, 14, 10);
    t.moves[kNail].hit = hit(35, 14, pct(8), Fx{}, ImpactClass::Light);
    t.moves[kPinBurst] = move("Pin Burst", MoveCategory::Skill, 8, 0, 16, 8);
    t.moves[kDecoyDoll] = move("Decoy Doll", MoveCategory::Skill, 4, 0, 10, 4);
    t.moves[kFullResonance] = move("Full Resonance", MoveCategory::MaxOutput, 30, 0, 26, 30);
    t.moves[kResonance] = move("Resonance", MoveCategory::Skill, 1, 0, 1);
    t.moves[kResonance].hit = hit(70, 24, pct(10), Fx{}, ImpactClass::Heavy, true);
    t.moves[kTotalResonance] = move("Total Resonance", MoveCategory::MaxOutput, 1, 0, 1);
    t.moves[kTotalResonance].hit = hit(190, 40, pct(20), pct(20), ImpactClass::Crushing, true);
    t.skills = {{{kNail, 30, 45}, {kPinBurst, 60, 150}, {kDecoyDoll, 70, 360}}};
    t.maxOutputMove = kFullResonance;
    t.maxOutputType = MaxOutputType::Burst;
    t.realmClass = RealmClass::Debuff;
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& strawEffigy() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
