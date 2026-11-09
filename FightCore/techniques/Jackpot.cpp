// Jackpot
// Core rule: a jackpot meter filled by landing and blocking hits. When it hits 100 you win the
// jackpot: unlimited CE and automatic healing for 20 seconds.
// Skills: Ball Rush, Shutter Wall, Reel Dash. Max Output: Fever Mode (short unlimited CE).
// Realm: Lucky Express, spins the reels; a win means jackpot. Weakness: thin without a jackpot.
#include "Common.h"
#include "Techniques.h"

namespace fc::techniques {
namespace {

enum : int { kMeter, kJackpotTime, kShutter, kFever };

uint32_t spin(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

void fill(const TechContext& c, int amount) {
    int32_t* t = c.me().tech.data();
    t[kMeter] += amount;
    if (t[kMeter] < 100) return;
    t[kMeter] = 0;
    t[kJackpotTime] = 1200;
    c.emit(ImpactKind::TechniqueFx, c.me().pos, {}, Fx::fromInt(1), ImpactClass::Crushing, 77);
}

class JackpotLogic final : public TechniqueLogic {
public:
    void onMoveEvent(const TechContext& c, int16_t move) const override {
        FighterState& me = c.me();
        switch (move) {
            case kBallRush: {
                const Vec3 right = rightOf(me.facing);
                const Vec3 from = c.localToWorld({Fx{}, tenths(12), tenths(8)});
                for (int k = -1; k <= 1; ++k) {
                    const Vec3 dir = normalizeXZ(me.facing + right * pct(15 * k), me.facing);
                    c.spawnProjectile(from, dir * pct(40), tenths(35) / Fx::fromInt(10), 50, kBallRush);
                }
                break;
            }
            case kShutterWall: me.tech[kShutter] = 45; break;
            case kReelDash: fill(c, 15); break;
            case kFeverMode: me.tech[kFever] = 300; break;
            default: break;
        }
    }

    void onTick(const TechContext& c) const override {
        FighterState& me = c.me();
        int32_t* t = me.tech.data();
        if (t[kShutter] > 0) --t[kShutter];
        if (t[kFever] > 0) --t[kFever];
        if (t[kJackpotTime] > 0) {
            --t[kJackpotTime];
            if ((c.m.frame & 1) == 0 && me.health < c.stats().maxHealth) ++me.health; // automatic Reverse Flow
        }
        if (t[kJackpotTime] > 0 || t[kFever] > 0) {
            me.ce = c.stats().ceMax;
            me.burnout = 0;
        }
    }

    // Lucky Express: spin the reels
    void onRealmOpen(const TechContext& c) const override {
        if (spin(static_cast<uint32_t>(c.m.frame) * 31u + static_cast<uint32_t>(c.self)) % 3 == 0) fill(c, 100);
        else fill(c, 40);
    }

    void onHitLanded(const TechContext& c, int16_t, bool, int) const override { fill(c, 8); }
    void onBlock(const TechContext& c, bool) const override { fill(c, 6); }

    bool nullifiesIncoming(const TechContext& c, const MoveDef&, bool, int attacker) const override {
        const FighterState& me = c.me();
        return me.tech[kShutter] > 0 && dot(me.facing, c.m.f[attacker].pos - me.pos) > Fx{};
    }
};

const JackpotLogic kLogic;

TechniqueDef build() {
    TechniqueDef t;
    t.id = "jackpot";
    t.name = "Jackpot";
    t.coreRule = "Landing and blocking hits fills the jackpot meter. Jackpot: unlimited CE and auto healing.";
    t.weakness = "Thin kit until the jackpot hits, and the realm can be countered.";
    t.realmName = "Lucky Express";
    t.moves.resize(kJackpotMoveCount);
    t.moves[kBallRush] = move("Ball Rush", MoveCategory::Skill, 12, 0, 18, 12);
    t.moves[kBallRush].hit = hit(45, 16, pct(15), Fx{}, ImpactClass::Medium);
    t.moves[kShutterWall] = move("Shutter Wall", MoveCategory::Skill, 4, 0, 20, 4);
    t.moves[kReelDash] = move("Reel Dash", MoveCategory::Skill, 1, 0, 10, 1);
    t.moves[kReelDash].forwardSpeed = pct(45);
    t.moves[kReelDash].moveFirst = 1;
    t.moves[kReelDash].moveLast = 10;
    t.moves[kReelDash].invulnFirst = 1;
    t.moves[kReelDash].invulnLast = 8;
    t.moves[kFeverMode] = move("Fever Mode", MoveCategory::MaxOutput, 20, 0, 10, 20);
    t.skills = {{{kBallRush, 60, 100}, {kShutterWall, 50, 240}, {kReelDash, 40, 90}}};
    t.maxOutputMove = kFeverMode;
    t.maxOutputType = MaxOutputType::Transform;
    t.realmClass = RealmClass::BuffField;
    t.logic = &kLogic;
    return t;
}

} // namespace

const TechniqueDef& jackpot() {
    static const TechniqueDef t = build();
    return t;
}

} // namespace fc::techniques
