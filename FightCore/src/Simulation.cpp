#include "fightcore/Simulation.h"

namespace fc {

// ---------------------------------------------------------------- TechContext
const CharacterDef& TechContext::def() const { return *setup.chars[self]; }

Vec3 TechContext::localToWorld(Vec3 off) const {
    const FighterState& f = me();
    Vec3 right{f.facing.z, Fx{}, -f.facing.x};
    return f.pos + right * off.x + Vec3{Fx{}, off.y, Fx{}} + f.facing * off.z;
}

int TechContext::spawnProjectile(Vec3 pos, Vec3 vel, Fx radius, int16_t life, int16_t moveId) const {
    for (int i = 0; i < static_cast<int>(m.proj.size()); ++i) {
        Projectile& p = m.proj[i];
        if (p.alive) continue;
        p = Projectile{true, static_cast<uint8_t>(self), moveId, life, pos, vel, radius};
        return i;
    }
    return -1;
}

void TechContext::emit(ImpactKind kind, Vec3 pos, Vec3 dir, Fx force, ImpactClass strength, int16_t tag) const {
    if (events) events->push_back({m.frame, kind, static_cast<uint8_t>(self), strength, tag, pos, dir, force});
}

namespace {

bool overlap(Vec3 a, Fx ra, Vec3 b, Fx rb) {
    int64_t r = static_cast<int64_t>(ra.raw) + rb.raw;
    return lengthSqRaw(a - b) <= r * r;
}

bool isInvulnerable(const FighterState& f, const CharacterDef& def) {
    if (f.state != FState::Attack) return false;
    const MoveDef& mv = def.moves[f.moveId];
    return f.moveFrame >= mv.invulnFirst && f.moveFrame <= mv.invulnLast;
}

int resolveCommand(FighterState& f, const CharacterDef& def, uint8_t allowedCats, const MatchState& m, int self) {
    const uint8_t dir = f.input.current().dir;
    for (const CommandDef& c : def.commands) {
        if (c.air == f.grounded) continue;
        if (c.dir != 0 && c.dir != dir) continue;
        if (!f.input.pressedWithin(c.button, kBufferWindow)) continue;
        const MoveDef& mv = def.moves[c.moveId];
        if (!(allowedCats & catBit(mv.category))) continue;
        if (f.essence < mv.essenceCost) continue;
        if (mv.category == MoveCategory::Ultimate && m.realm.owner == self) continue;
        f.input.consume(c.button, kBufferWindow);
        return c.moveId;
    }
    return -1;
}

void startMove(FighterState& f, const CharacterDef& def, int16_t id) {
    f.state = FState::Attack;
    f.moveId = id;
    f.moveFrame = 0;
    f.moveHasHit = false;
    f.essence = static_cast<int16_t>(f.essence - def.moves[id].essenceCost);
}

Vec3 dirToVelocity(const FighterState& f, const CharacterDef& d, uint8_t dir) {
    Vec3 right{f.facing.z, Fx{}, -f.facing.x};
    switch (dir) {
        case 6: return f.facing * d.walkSpeed;
        case 4: return f.facing * -d.backSpeed;
        case 8: return right * -d.sideSpeed;
        case 2: return right * d.sideSpeed;
        case 9: return f.facing * d.walkSpeed + right * -d.sideSpeed;
        case 7: return f.facing * -d.backSpeed + right * -d.sideSpeed;
        case 3: return f.facing * d.walkSpeed + right * d.sideSpeed;
        case 1: return f.facing * -d.backSpeed + right * d.sideSpeed;
        default: return {};
    }
}

void emit(EventSink* ev, const MatchState& m, ImpactKind k, int src, ImpactClass s, Vec3 pos, Vec3 dir, Fx force) {
    if (ev) ev->push_back({m.frame, k, static_cast<uint8_t>(src), s, 0, pos, dir, force});
}

void updateFighter(MatchState& m, const MatchSetup& setup, int i, EventSink* ev) {
    FighterState& f = m.f[i];
    const FighterState& o = m.f[1 - i];
    const CharacterDef& d = *setup.chars[i];
    TechContext ctx{m, setup, i, ev};

    // Blickrichtung zum Gegner (nicht während Angriffen -> Commitment)
    if (f.state != FState::Attack && f.state != FState::Hitstun)
        f.facing = normalizeXZ(o.pos - f.pos, f.facing);

    const FrameInput& in = f.input.current();
    const uint8_t all = 0xFF;

    switch (f.state) {
        case FState::Idle:
        case FState::Walk:
        case FState::Guard: {
            int id = resolveCommand(f, d, all, m, i);
            if (id >= 0) { startMove(f, d, static_cast<int16_t>(id)); f.vel = {}; break; }
            if (f.input.pressedWithin(BtnJump, 3)) {
                f.input.consume(BtnJump, 3);
                Vec3 h = dirToVelocity(f, d, in.dir);
                f.vel = {h.x, d.jumpVelocity, h.z};
                f.state = FState::Air;
                f.grounded = false;
                emit(ev, m, ImpactKind::DashDust, i, ImpactClass::Light, f.pos, {}, d.jumpVelocity);
                break;
            }
            if (f.input.pressedWithin(BtnDash, 3)) {
                f.input.consume(BtnDash, 3);
                Vec3 dir = in.dir == 4 ? f.facing * Fx::fromInt(-1) : f.facing;
                if (in.dir == 8 || in.dir == 2) dir = normalizeXZ(dirToVelocity(f, d, in.dir), f.facing);
                f.vel = dir * d.dashSpeed;
                f.state = FState::Dash;
                f.stateFrame = 0;
                emit(ev, m, ImpactKind::DashDust, i, ImpactClass::Medium, f.pos, dir, d.dashSpeed);
                break;
            }
            if (in.buttons & BtnGuard) { f.state = FState::Guard; f.vel = {}; break; }
            f.vel = dirToVelocity(f, d, in.dir);
            f.state = (in.dir == 5) ? FState::Idle : FState::Walk;
            break;
        }
        case FState::Dash: {
            int id = resolveCommand(f, d, all, m, i);
            if (id >= 0) { startMove(f, d, static_cast<int16_t>(id)); break; }
            if (++f.stateFrame >= d.dashFrames) { f.state = FState::Idle; f.vel = {}; }
            break;
        }
        case FState::Air: {
            int id = resolveCommand(f, d, all, m, i);
            if (id >= 0) startMove(f, d, static_cast<int16_t>(id));
            break;
        }
        case FState::Attack: {
            const MoveDef& mv = d.moves[f.moveId];
            ++f.moveFrame;
            if (f.grounded) {
                bool moving = f.moveFrame >= mv.moveFirst && f.moveFrame <= mv.moveLast;
                Vec3 v = moving ? f.facing * mv.forwardSpeed : Vec3{};
                f.vel = {v.x, f.vel.y, v.z};
            }
            if (f.moveFrame == mv.eventFrame) {
                if (mv.category == MoveCategory::Ultimate) {
                    m.realm = {static_cast<int8_t>(i), kRealmFrames};
                    emit(ev, m, ImpactKind::RealmOpen, i, ImpactClass::Crushing, f.pos, {}, Fx::fromInt(1));
                    if (d.technique) d.technique->onRealmOpen(ctx);
                }
                if (d.technique) d.technique->onMoveEvent(ctx, f.moveId);
            }
            if (f.moveHasHit && mv.cancelMask) {
                int id = resolveCommand(f, d, mv.cancelMask, m, i);
                if (id >= 0) { startMove(f, d, static_cast<int16_t>(id)); break; }
            }
            if (f.moveFrame >= mv.total()) {
                f.moveId = -1;
                f.state = f.grounded ? FState::Idle : FState::Air;
            }
            break;
        }
        case FState::Hitstun:
        case FState::Blockstun: {
            if (f.stun > 0) --f.stun;
            if (f.stun == 0 && f.grounded) {
                f.state = FState::Idle;
                f.comboTaken = 0;
            }
            break;
        }
        case FState::Knockdown: {
            if (++f.stateFrame >= 30) { f.state = FState::Idle; f.comboTaken = 0; f.launched = false; }
            break;
        }
    }
}

void physics(MatchState& m, const MatchSetup& setup, int i, EventSink* ev) {
    FighterState& f = m.f[i];
    const CharacterDef& d = *setup.chars[i];
    if (!f.grounded) f.vel.y -= d.gravity;
    f.pos += f.vel;

    // Bodenreibung bei Hit-/Blockstun
    if (f.grounded && (f.state == FState::Hitstun || f.state == FState::Blockstun || f.state == FState::Knockdown))
        f.vel = f.vel * Fx::ratio(85, 100);

    if (!f.grounded && f.pos.y <= Fx{}) {
        Fx impactSpeed = -f.vel.y;
        f.pos.y = Fx{};
        f.vel.y = Fx{};
        f.grounded = true;
        if (f.state == FState::Air) {
            f.state = FState::Idle;
            emit(ev, m, ImpactKind::Landing, i, ImpactClass::Light, f.pos, {}, impactSpeed);
        } else if (f.state == FState::Hitstun && f.launched) {
            f.state = FState::Knockdown;
            f.stateFrame = 0;
            ImpactClass s = impactSpeed > Fx::ratio(1, 2) ? ImpactClass::Crushing
                          : impactSpeed > Fx::ratio(1, 4) ? ImpactClass::Heavy : ImpactClass::Medium;
            emit(ev, m, ImpactKind::GroundSlam, i, s, f.pos, {Fx{}, Fx::fromInt(-1), Fx{}}, impactSpeed);
        } else if (f.state == FState::Attack) {
            emit(ev, m, ImpactKind::Landing, i, ImpactClass::Light, f.pos, {}, impactSpeed);
        }
    }

    // Wände: Kämpfer in Hitstun mit genug Speed beschädigen sie (gameplay-relevante Zerstörung).
    const Fx h = setup.stage.halfExtent;
    Fx* axis[4] = {&f.pos.x, &f.pos.x, &f.pos.z, &f.pos.z};
    Fx* vaxis[4] = {&f.vel.x, &f.vel.x, &f.vel.z, &f.vel.z};
    for (int w = 0; w < 4; ++w) {
        const bool positive = (w % 2) == 0;
        WallState& wall = m.walls[w];
        Fx bound = wall.broken ? h + h : h;
        Fx p = *axis[w];
        bool outside = positive ? p > bound : p < -bound;
        if (!outside) continue;
        *axis[w] = positive ? bound : -bound;
        Fx speed = abs(*vaxis[w]);
        if (!wall.broken && f.state == FState::Hitstun && speed >= setup.stage.wallImpactSpeed) {
            int16_t dmg = static_cast<int16_t>((speed * Fx::fromInt(400)).toInt());
            wall.hp = static_cast<int16_t>(wall.hp - dmg);
            Vec3 n{};
            if (w < 2) n.x = positive ? Fx::fromInt(1) : Fx::fromInt(-1);
            else n.z = positive ? Fx::fromInt(1) : Fx::fromInt(-1);
            if (wall.hp <= 0) {
                wall.broken = true;
                emit(ev, m, ImpactKind::WallBreak, i, ImpactClass::Crushing, f.pos, n, speed);
                continue; // Kämpfer fliegt durch die Wand
            }
            emit(ev, m, ImpactKind::WallSplat, i, ImpactClass::Heavy, f.pos, n, speed);
            f.stun = static_cast<int16_t>(f.stun + 10); // Wall-Splat verlängert Combo
        }
        *vaxis[w] = -(*vaxis[w]) * Fx::ratio(1, 2);
    }
}

void pushApart(MatchState& m, const MatchSetup& setup) {
    FighterState& a = m.f[0];
    FighterState& b = m.f[1];
    Vec3 diff{b.pos.x - a.pos.x, Fx{}, b.pos.z - a.pos.z};
    Fx minDist = setup.chars[0]->pushRadius + setup.chars[1]->pushRadius;
    if (abs(a.pos.y - b.pos.y) > Fx::fromInt(2)) return;
    Fx dist = length(diff);
    if (dist >= minDist) return;
    Vec3 n = normalizeXZ(diff, a.facing);
    Vec3 half = n * ((minDist - dist) * Fx::ratio(1, 2));
    a.pos = a.pos - half;
    b.pos = b.pos + half;
}

struct PendingHit {
    bool valid = false;
    const HitProps* props = nullptr;
    Vec3 point{}, dir{};
};

void applyHit(MatchState& m, const MatchSetup& setup, int atk, const PendingHit& h, EventSink* ev) {
    FighterState& a = m.f[atk];
    FighterState& d = m.f[1 - atk];
    const HitProps& p = *h.props;
    Vec3 toAtk = a.pos - d.pos;
    bool facingAtk = dot(d.facing, toAtk) > Fx{};
    bool blocked = !p.unblockable && d.grounded && facingAtk &&
                   (d.state == FState::Guard || d.state == FState::Blockstun);
    a.hitstop = d.hitstop = p.hitstop;

    if (blocked) {
        d.state = FState::Blockstun;
        d.stun = p.blockstun;
        d.vel = h.dir * (p.knockback * Fx::ratio(1, 2));
        d.essence = static_cast<int16_t>(d.essence + p.essenceGainDefender / 2);
        emit(ev, m, ImpactKind::Block, atk, p.impact, h.point, h.dir, p.knockback);
    } else {
        TechContext ctx{m, setup, atk, ev};
        const Technique* t = setup.chars[atk]->technique;
        int pct = t ? t->damagePercent(ctx) : 100;
        int scale = 100 - d.comboTaken * 10;
        if (scale < 30) scale = 30;
        int dmg = p.damage * scale / 100 * pct / 100;
        d.health = static_cast<int16_t>(d.health - dmg);
        d.state = FState::Hitstun;
        d.stun = p.hitstun;
        d.moveId = -1;
        d.vel = h.dir * p.knockback + Vec3{Fx{}, p.launch, Fx{}};
        if (p.launch > Fx{}) { d.grounded = false; d.launched = true; }
        ++d.comboTaken;
        d.essence = static_cast<int16_t>(d.essence + p.essenceGainDefender);
        a.essence = static_cast<int16_t>(a.essence + p.essenceGainAttacker);
        emit(ev, m, ImpactKind::Hit, atk, p.impact, h.point, h.dir, p.knockback + p.launch);
    }
    if (a.essence > kEssenceMax) a.essence = kEssenceMax;
    if (d.essence > kEssenceMax) d.essence = kEssenceMax;
}

PendingHit checkMelee(MatchState& m, const MatchSetup& setup, int atk) {
    FighterState& a = m.f[atk];
    const FighterState& d = m.f[1 - atk];
    if (a.state != FState::Attack || a.moveHasHit) return {};
    const MoveDef& mv = setup.chars[atk]->moves[a.moveId];
    if (!mv.isActiveFrame(a.moveFrame)) return {};
    if (isInvulnerable(d, *setup.chars[1 - atk])) return {};
    TechContext actx{m, setup, atk, nullptr};
    TechContext dctx{m, setup, 1 - atk, nullptr};
    for (const HitboxDef& hb : mv.hitboxes) {
        Vec3 c = actx.localToWorld(hb.offset);
        for (const HurtboxDef& hu : setup.chars[1 - atk]->hurtboxes) {
            if (overlap(c, hb.radius, dctx.localToWorld(hu.offset), hu.radius))
                return {true, &mv.hit, c, a.facing};
        }
    }
    return {};
}

void updateProjectiles(MatchState& m, const MatchSetup& setup, EventSink* ev) {
    const Fx limit = setup.stage.halfExtent * Fx::fromInt(3);
    for (Projectile& p : m.proj) {
        if (!p.alive) continue;
        p.pos += p.vel;
        int target = 1 - p.owner;
        const FighterState& d = m.f[target];
        TechContext dctx{m, setup, target, nullptr};
        bool hit = false;
        if (!isInvulnerable(d, *setup.chars[target])) {
            for (const HurtboxDef& hu : setup.chars[target]->hurtboxes)
                if (overlap(p.pos, p.radius, dctx.localToWorld(hu.offset), hu.radius)) { hit = true; break; }
        }
        if (hit) {
            const MoveDef& mv = setup.chars[p.owner]->moves[p.moveId];
            applyHit(m, setup, p.owner, {true, &mv.hit, p.pos, normalizeXZ(p.vel, m.f[p.owner].facing)}, ev);
            p.alive = false;
            continue;
        }
        if (--p.life <= 0 || abs(p.pos.x) > limit || abs(p.pos.z) > limit) {
            p.alive = false;
            emit(ev, m, ImpactKind::ProjectileBurst, p.owner, ImpactClass::Medium, p.pos, {}, length(p.vel));
        }
    }
}

} // namespace

MatchState createMatch(const MatchSetup& setup) {
    MatchState m{};
    for (int i = 0; i < 2; ++i) {
        FighterState& f = m.f[i];
        f.health = setup.chars[i]->maxHealth;
        f.pos = {Fx{}, Fx{}, Fx::fromInt(i == 0 ? -3 : 3)};
        f.facing = {Fx{}, Fx{}, Fx::fromInt(i == 0 ? 1 : -1)};
    }
    for (WallState& w : m.walls) w.hp = setup.stage.wallHp;
    return m;
}

void step(MatchState& m, const MatchSetup& setup, const FrameInput inputs[2], EventSink* ev) {
    if (m.winner != 255) return;
    for (int i = 0; i < 2; ++i) m.f[i].input.push(inputs[i]);

    // Hitstop friert Kämpfer ein (Impact-Gefühl), Inputs werden trotzdem gebuffert.
    bool frozen[2];
    for (int i = 0; i < 2; ++i) {
        frozen[i] = m.f[i].hitstop > 0;
        if (frozen[i]) --m.f[i].hitstop;
    }

    for (int i = 0; i < 2; ++i) if (!frozen[i]) updateFighter(m, setup, i, ev);
    for (int i = 0; i < 2; ++i) if (!frozen[i]) physics(m, setup, i, ev);
    pushApart(m, setup);

    for (int i = 0; i < 2; ++i) {
        const Technique* t = setup.chars[i]->technique;
        if (t) t->onTick(TechContext{m, setup, i, ev});
    }

    // Treffer beider Seiten erst sammeln, dann anwenden -> echte Trades.
    PendingHit hits[2];
    for (int i = 0; i < 2; ++i) if (!frozen[i]) hits[i] = checkMelee(m, setup, i);
    for (int i = 0; i < 2; ++i) {
        if (!hits[i].valid) continue;
        m.f[i].moveHasHit = true;
        applyHit(m, setup, i, hits[i], ev);
    }

    updateProjectiles(m, setup, ev);

    if (m.realm.owner >= 0 && --m.realm.timer <= 0) m.realm = {};

    bool dead0 = m.f[0].health <= 0, dead1 = m.f[1].health <= 0;
    if (dead0 && dead1) m.winner = 2;
    else if (dead0) m.winner = 1;
    else if (dead1) m.winner = 0;

    ++m.frame;
}

// ---------------------------------------------------------------- Hash
namespace {
constexpr uint64_t kFnvPrime = 1099511628211ull;
uint64_t mix(uint64_t h, uint64_t v) {
    for (int i = 0; i < 8; ++i) { h ^= (v >> (i * 8)) & 0xFF; h *= kFnvPrime; }
    return h;
}
uint64_t mixV(uint64_t h, Vec3 v) {
    return mix(mix(mix(h, static_cast<uint32_t>(v.x.raw)), static_cast<uint32_t>(v.y.raw)), static_cast<uint32_t>(v.z.raw));
}
} // namespace

uint64_t InputBuffer::hash(uint64_t h) const {
    h = mix(h, head_);
    for (const FrameInput& in : hist_) h = mix(h, in.dir | (in.buttons << 8) | (in.consumed << 16));
    return h;
}

uint64_t hashState(const MatchState& m) {
    uint64_t h = 14695981039346656037ull;
    h = mix(h, static_cast<uint32_t>(m.frame));
    for (const FighterState& f : m.f) {
        h = mixV(mixV(mixV(h, f.pos), f.vel), f.facing);
        h = mix(h, static_cast<uint16_t>(f.health) | (static_cast<uint64_t>(static_cast<uint16_t>(f.essence)) << 16) |
                       (static_cast<uint64_t>(f.state) << 32));
        h = mix(h, static_cast<uint16_t>(f.stateFrame) | (static_cast<uint64_t>(static_cast<uint16_t>(f.moveId)) << 16) |
                       (static_cast<uint64_t>(static_cast<uint16_t>(f.moveFrame)) << 32));
        h = mix(h, static_cast<uint16_t>(f.stun) | (static_cast<uint64_t>(static_cast<uint16_t>(f.hitstop)) << 16) |
                       (static_cast<uint64_t>(static_cast<uint16_t>(f.comboTaken)) << 32));
        h = mix(h, f.moveHasHit | (f.grounded << 1) | (f.launched << 2));
        h = f.input.hash(h);
        for (int32_t t : f.tech) h = mix(h, static_cast<uint32_t>(t));
    }
    for (const Projectile& p : m.proj) {
        if (!p.alive) { h = mix(h, 0); continue; }
        h = mix(h, 1 | (p.owner << 8) | (static_cast<uint64_t>(static_cast<uint16_t>(p.moveId)) << 16) |
                       (static_cast<uint64_t>(static_cast<uint16_t>(p.life)) << 32));
        h = mix(mixV(mixV(h, p.pos), p.vel), static_cast<uint32_t>(p.radius.raw));
    }
    for (const WallState& w : m.walls) h = mix(h, static_cast<uint16_t>(w.hp) | (w.broken << 16));
    h = mix(h, static_cast<uint8_t>(m.realm.owner) | (static_cast<uint32_t>(static_cast<uint16_t>(m.realm.timer)) << 8));
    return mix(h, m.winner);
}

} // namespace fc
