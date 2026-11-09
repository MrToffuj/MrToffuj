// Sky Weave
// Core rule: the sky turns into a film you can fold. Film charges build up over time (max 3)
// and are spent by Sky Drop, Sky Pull and the Max Output. Getting hit tears the film.
// Skills: Sky Drop, Sky Pull, Sky Wall. Max Output: Sky Tear (zone). Realm: Folded Heaven (debuff).
#include "Common.h"
#include "Techniques.h"

namespace fc::techniques {
namespace {

enum : int { kFilm, kFilmTick, kWall };

class SkyLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        FighterState& me = c.me();
        const FighterState& op = c.opp();
        switch (move) {
            case kSkyDrop:
                --me.tech[kFilm];
                c.spawnProjectile(op.pos + Vec3{Fx{}, Fx::fromInt(6), Fx{}}, {Fx{}, pct(-30), Fx{}}, tenths(9), 40, kSkyDrop, 6);
                break;
            case kSkyPull:
                --me.tech[kFilm];
                if (c.hasTarget() && flatDistance(me.pos, op.pos) < Fx::fromInt(14)) c.strike(me.target, kSkyPull);
                break;
            case kSkyWall:
                me.tech[kWall] = 120;
                c.emit(ImpactKind::TechniqueFx, c.localToWorld({Fx{}, Fx::fromInt(1), Fx::fromInt(1)}), me.facing, Fx{}, ImpactClass::Medium, 2);
                break;
            case kSkyTear:
                me.tech[kFilm] = 0;
                for (int16_t delay : {15, 45, 75})
                    c.spawnProjectile(op.pos + Vec3{Fx{}, Fx::fromInt(1), Fx{}}, {}, Fx::fromInt(3), 20, kSkyTear, delay);
                break;
            default: break;
        }
    }

    void onTick(const TechContext& c) const override {
        int32_t* t = c.me().tech.data();
        if (t[kWall] > 0) --t[kWall];
        if (t[kFilm] < 3 && ++t[kFilmTick] >= 120) { t[kFilmTick] = 0; ++t[kFilm]; }
    }

    bool canUseSkill(const TechContext& c, int slot) const override {
        const int film = c.me().tech[kFilm];
        if (slot == 3) return film >= 2;
        return slot == 2 || film > 0;
    }

    void onHitTaken(const TechContext& c, int) const override {
        c.me().tech[kFilm] = 0;
        c.me().tech[kFilmTick] = 0;
        c.me().tech[kWall] = 0;
    }

    bool nullifiesIncoming(const TechContext& c, const MoveDef&, bool projectile, int attacker) const override {
        const FighterState& me = c.me();
        return projectile && me.tech[kWall] > 0 && dot(me.facing, c.m.f[attacker].pos - me.pos) > Fx{};
    }
};

const SkyLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.name = "Sky Weave";
    t.coreRule = "The sky becomes a film to fold. Film charges build over time and are spent by the skills.";
    t.weakness = "Needs time to build film, any hit tears it.";
    t.realmName = "Folded Heaven";
    t.moves.resize(kSkyMoveCount);
    t.moves[kSkyDrop] = move("Sky Drop", MoveCategory::Skill, 16, 0, 22, 16);
    t.moves[kSkyDrop].hit = hit(90, 30, pct(10), Fx{}, ImpactClass::Heavy);
    t.moves[kSkyPull] = move("Sky Pull", MoveCategory::Skill, 12, 0, 18, 12);
    t.moves[kSkyPull].hit = hit(30, 26, pct(-35), Fx{}, ImpactClass::Medium);
    t.moves[kSkyWall] = move("Sky Wall", MoveCategory::Skill, 6, 0, 10, 6);
    t.moves[kSkyTear] = move("Sky Tear", MoveCategory::MaxOutput, 40, 0, 30, 40);
    t.moves[kSkyTear].hit = hit(120, 36, pct(20), pct(25), ImpactClass::Crushing);
    t.skills = {{{kSkyDrop, 80, 180}, {kSkyPull, 70, 200}, {kSkyWall, 60, 300}}};
    t.maxOutputMove = kSkyTear;
    t.maxOutputType = MaxOutputType::Zone;
    t.realmClass = RealmClass::Debuff;
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& skyWeave() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
