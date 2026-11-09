#include "fightcore/Simulation.h"

namespace fc {

// ============================================================ Basis-Moves (für alle Builds gleich)
namespace {

std::vector<MoveDef> buildBaseMoves() {
    const uint8_t intoSpecial = catBit(MoveCategory::Skill) | catBit(MoveCategory::Apex) | catBit(MoveCategory::Realm);
    std::vector<MoveDef> t(kBaseMoveCount);
    {
        MoveDef& m = t[kLight];
        m.name = "Light"; m.startup = 5; m.active = 3; m.recovery = 8;
        m.hitboxes = {{{Fx{}, Fx::ratio(12, 10), Fx::ratio(9, 10)}, Fx::ratio(4, 10)}};
        m.hit = {40, 14, 10, 6, Fx::ratio(5, 100), Fx{}, ImpactClass::Light, false};
        m.cancelMask = static_cast<uint8_t>(catBit(MoveCategory::Normal) | intoSpecial);
    }
    {
        MoveDef& m = t[kHeavy];
        m.name = "Heavy"; m.startup = 12; m.active = 4; m.recovery = 18;
        m.hitboxes = {{{Fx{}, Fx::ratio(12, 10), Fx::ratio(11, 10)}, Fx::ratio(5, 10)}};
        m.hit = {90, 22, 16, 10, Fx::ratio(30, 100), Fx{}, ImpactClass::Heavy, false};
        m.forwardSpeed = Fx::ratio(8, 100); m.moveFirst = 6; m.moveLast = 12;
        m.cancelMask = intoSpecial;
        m.armored = true;
    }
    {
        MoveDef& m = t[kLauncher];
        m.name = "Launcher"; m.startup = 10; m.active = 4; m.recovery = 22;
        m.hitboxes = {{{Fx{}, Fx::ratio(14, 10), Fx::ratio(8, 10)}, Fx::ratio(5, 10)}};
        m.hit = {70, 30, 14, 8, Fx::ratio(3, 100), Fx::ratio(32, 100), ImpactClass::Medium, false};
        m.cancelMask = intoSpecial;
    }
    {
        MoveDef& m = t[kAirLight];
        m.name = "Air Light"; m.startup = 6; m.active = 4; m.recovery = 10;
        m.hitboxes = {{{Fx{}, Fx::ratio(8, 10), Fx::ratio(8, 10)}, Fx::ratio(45, 100)}};
        m.hit = {50, 16, 10, 6, Fx::ratio(8, 100), Fx::ratio(10, 100), ImpactClass::Medium, false};
        m.cancelMask = static_cast<uint8_t>(catBit(MoveCategory::Normal) | catBit(MoveCategory::Skill));
    }
    {
        MoveDef& m = t[kGrab];
        m.name = "Grab"; m.category = MoveCategory::Grab; m.startup = 7; m.active = 2; m.recovery = 25;
        m.hitboxes = {{{Fx{}, Fx::ratio(11, 10), Fx::ratio(7, 10)}, Fx::ratio(4, 10)}};
        m.hit = {80, 40, 0, 8, Fx::ratio(25, 100), Fx::ratio(15, 100), ImpactClass::Heavy, true};
    }
    {
        MoveDef& m = t[kBurst];
        m.name = "Essence Burst"; m.category = MoveCategory::Burst; m.startup = 1; m.active = 4; m.recovery = 30;
        m.hitboxes = {{{Fx{}, Fx::fromInt(1), Fx{}}, Fx::ratio(16, 10)}};
        m.hit = {0, 20, 0, 4, Fx::ratio(45, 100), Fx::ratio(10, 100), ImpactClass::Heavy, true};
        m.invulnFirst = 0; m.invulnLast = 12;
        m.usableInStun = true;
    }
    {
        MoveDef& m = t[kRealmCast];
        m.name = "Realm Cast"; m.category = MoveCategory::Realm;
        m.startup = kRealmCastFrames; m.active = 0; m.recovery = 20;
        m.eventFrame = kRealmCastFrames; m.invulnFirst = 0; m.invulnLast = kRealmCastFrames;
    }
    return t;
}

const PactModifiers kNeutralPact{};

} // namespace

const MoveDef& baseMove(int16_t id) {
    static const std::vector<MoveDef> table = buildBaseMoves();
    return table[id];
}

const MoveDef& moveOf(const MatchSetup& s, int i, int16_t id) {
    if (id < kTechMoveOffset) return baseMove(id);
    return s.loadout[i].technique->moves[id - kTechMoveOffset];
}

void MatchSetup::finalize() {
    for (int i = 0; i < 2; ++i) {
        stats[i] = deriveStats(*loadout[i].body);
        pact[i] = evaluatePact(loadout[i].pact, *loadout[i].body, loadout[i].technique, loadout[i].profile);
    }
}

// ============================================================ TechContext
FighterState& TechContext::me() const { return m.f[self]; }
FighterState& TechContext::opp() const { return m.f[1 - self]; }
const BodyStats& TechContext::stats() const { return setup.stats[self]; }
bool TechContext::realmIsMine() const { return m.realm.owner == self && m.realm.clashTimer == 0; }

Vec3 TechContext::localToWorld(Vec3 off) const {
    const FighterState& f = me();
    Vec3 right{f.facing.z, Fx{}, -f.facing.x};
    return f.pos + right * off.x + Vec3{Fx{}, off.y, Fx{}} + f.facing * off.z;
}

int TechContext::spawnProjectile(Vec3 pos, Vec3 vel, Fx radius, int16_t life, int16_t moveIndex, int16_t armDelay) const {
    for (int i = 0; i < static_cast<int>(m.proj.size()); ++i) {
        Projectile& p = m.proj[i];
        if (p.alive) continue;
        p = Projectile{true, static_cast<uint8_t>(self), moveIndex, life, armDelay, pos, vel, radius};
        return i;
    }
    return -1;
}

void TechContext::emit(ImpactKind kind, Vec3 pos, Vec3 dir, Fx force, ImpactClass strength, int16_t tag) const {
    if (events) events->push_back({m.frame, kind, static_cast<uint8_t>(self), strength, tag, pos, dir, force});
}

void TechContext::damage(int target, int16_t amount) const {
    m.f[target].health = static_cast<int16_t>(m.f[target].health - amount);
}

// ============================================================ Simulation
namespace {

using Ev = EventSink*;

void emit(Ev ev, const MatchState& m, ImpactKind k, int src, ImpactClass s, Vec3 pos, Vec3 dir, Fx force, int16_t tag = 0) {
    if (ev) ev->push_back({m.frame, k, static_cast<uint8_t>(src), s, tag, pos, dir, force});
}

const PactModifiers& mods(const MatchState& m, const MatchSetup& s, int i) {
    if (s.pact[i].tier == PactTier::None || m.f[i].pactTimer == 0) return kNeutralPact;
    return s.pact[i].mods;
}

const TechniqueLogic* logicOf(const MatchSetup& s, int i) {
    const TechniqueDef* t = s.loadout[i].technique;
    return t ? t->logic : nullptr;
}

bool overlap(Vec3 a, Fx ra, Vec3 b, Fx rb) {
    int64_t r = static_cast<int64_t>(ra.raw) + rb.raw;
    return lengthSqRaw(a - b) <= r * r;
}

Fx distXZ(Vec3 a, Vec3 b) { return length(Vec3{a.x - b.x, Fx{}, a.z - b.z}); }

// Wirkt der Realm des Gegners (Sure-Hit / Debuff) auf `target`?
bool realmAffects(const MatchState& m, const MatchSetup& s, int target) {
    const RealmState& r = m.realm;
    if (r.owner < 0 || r.owner == target || r.clashTimer > 0) return false;
    if (s.stats[target].sureHitImmune || m.f[target].state == FState::Ward) return false;
    if (r.open && distXZ(m.f[r.owner].pos, m.f[target].pos) > Fx::fromInt(kOpenRealmRadius)) return false;
    return true;
}
RealmClass realmClass(const MatchState& m, const MatchSetup& s) { return s.loadout[m.realm.owner].technique->realmClass; }
bool debuffed(const MatchState& m, const MatchSetup& s, int i) {
    return realmAffects(m, s, i) && realmClass(m, s) == RealmClass::Debuff;
}
bool buffed(const MatchState& m, const MatchSetup& s, int i) {
    return m.realm.owner == i && m.realm.clashTimer == 0 && realmClass(m, s) == RealmClass::BuffField;
}

void triggerBurnout(MatchState& m, int i, Ev ev) {
    FighterState& f = m.f[i];
    f.ce = 0;
    f.reinforce = false;
    if (f.burnout == 0) {
        f.burnout = kBurnoutFrames;
        emit(ev, m, ImpactKind::Burnout, i, ImpactClass::Light, f.pos, {}, Fx{});
    }
}

int32_t ceMaxOf(const MatchSetup& s, const MatchState& m, int i) {
    return s.stats[i].ceMax * mods(m, s, i).cePoolPct / 100;
}

int32_t skillCost(const MatchState& m, const MatchSetup& s, int i, int slot) {
    int32_t c = s.loadout[i].technique->skills[slot].ceCost * 100;
    if (debuffed(m, s, i)) c = c * 150 / 100;
    if (buffed(m, s, i)) c = c / 2;
    return c;
}

struct Cmd {
    int16_t move = -1;
    int8_t skill = -1;
    int32_t ceCost = 0;
    bool apex = false, realm = false, ward = false;
    int age = 0;
    uint8_t button = 0;
};

Cmd resolveCommand(MatchState& m, const MatchSetup& s, int i, uint8_t allowed) {
    FighterState& f = m.f[i];
    const TechniqueDef* T = s.loadout[i].technique;
    const PactModifiers& pm = mods(m, s, i);
    auto age = [&](uint8_t b) { return f.input.pressAge(b, kBufferWindow); };
    // Modifier/Guard zählen im Frame des Tastendrucks, nicht beim (gebufferten) Auslesen.
    auto heldAt = [&](int a, uint8_t b) { return (f.input.ago(a).buttons & b) != 0; };
    auto ok = [&](Cmd c, uint8_t btn, int a) { c.button = btn; c.age = a; f.input.consume(btn, kBufferWindow); return c; };
    int a;

    if ((a = age(BtnUlt)) >= 0) {
        const bool mod = heldAt(a, BtnMod);
        if (!mod && m.realm.owner == 1 - i && m.realm.clashTimer == 0) {
            Cmd c; c.ward = true; return ok(c, BtnUlt, a);
        }
        if (mod) {
            bool can = T && !pm.noRealm && f.realmMeter >= kRealmMeterMax && m.realm.owner != i &&
                       (allowed & catBit(MoveCategory::Realm));
            if (can) { Cmd c; c.move = kRealmCast; c.realm = true; return ok(c, BtnUlt, a); }
        } else {
            int32_t cost = ceMaxOf(s, m, i) / 2;
            bool can = T && T->apexMove >= 0 && !f.apexUsed && !pm.noApex && f.burnout == 0 && cost > 0 &&
                       f.ce >= cost && (allowed & catBit(MoveCategory::Apex));
            if (can) {
                Cmd c; c.move = static_cast<int16_t>(T->apexMove + kTechMoveOffset); c.apex = true; c.ceCost = cost;
                return ok(c, BtnUlt, a);
            }
        }
        return {};
    }

    if ((a = age(BtnGrab)) >= 0 && heldAt(a, BtnGuard)) {
        int32_t cost = ceMaxOf(s, m, i) * 3 / 10;
        if (cost > 0 && f.ce >= cost && (allowed & catBit(MoveCategory::Burst))) {
            Cmd c; c.move = kBurst; c.ceCost = cost; return ok(c, BtnGrab, a);
        }
        return {};
    }

    const uint8_t btns[3] = {BtnLight, BtnHeavy, BtnGrab};
    for (int slot = 0; slot < 3; ++slot) {
        if ((a = age(btns[slot])) < 0 || !heldAt(a, BtnMod)) continue;
        // Mod gehalten -> Skill oder nichts (nie versehentliche Normals)
        if (!T || f.burnout > 0 || f.cooldown[slot] > 0 || (pm.lockedSkills & (1 << slot))) return {};
        if (!(allowed & catBit(MoveCategory::Skill))) return {};
        int32_t cost = skillCost(m, s, i, slot);
        if (f.ce < cost) return {};
        Cmd c; c.move = static_cast<int16_t>(T->skills[slot].move + kTechMoveOffset); c.skill = static_cast<int8_t>(slot);
        c.ceCost = cost;
        return ok(c, btns[slot], a);
    }

    if (!f.grounded) {
        if ((a = age(BtnLight)) >= 0 && (allowed & catBit(MoveCategory::Normal))) { Cmd c; c.move = kAirLight; return ok(c, BtnLight, a); }
        return {};
    }
    if ((a = age(BtnGrab)) >= 0 && (allowed & catBit(MoveCategory::Grab))) { Cmd c; c.move = kGrab; return ok(c, BtnGrab, a); }
    if (!(allowed & catBit(MoveCategory::Normal))) return {};
    if ((a = age(BtnHeavy)) >= 0) {
        Cmd c; c.move = f.input.ago(a).dir == 2 ? kLauncher : kHeavy; return ok(c, BtnHeavy, a);
    }
    if ((a = age(BtnLight)) >= 0) { Cmd c; c.move = kLight; return ok(c, BtnLight, a); }
    return {};
}

void startMove(MatchState& m, const MatchSetup& s, int i, const Cmd& c, Ev ev) {
    FighterState& f = m.f[i];
    const MoveDef& mv = moveOf(s, i, c.move);
    f.state = FState::Attack;
    f.moveId = c.move;
    f.moveFrame = 0;
    f.moveHasHit = false;
    f.pressFrame = m.frame - c.age;
    f.armorLeft = (mv.armored && s.loadout[i].body->has(PassiveSuperArmor)) ? 1 : 0;
    f.stun = 0;
    if (c.ceCost > 0) {
        f.ce -= c.ceCost;
        if (f.ce <= 0) triggerBurnout(m, i, ev);
    }
    if (c.skill >= 0) {
        int cd = s.loadout[i].technique->skills[c.skill].cooldown;
        cd = cd * s.stats[i].cooldownPct / 100 * mods(m, s, i).cooldownPct / 100;
        f.cooldown[c.skill] = static_cast<int16_t>(cd);
    }
    if (c.apex) {
        f.apexUsed = true;
        if (mods(m, s, i).apexNoStartup && mv.startup > 1) f.moveFrame = static_cast<int16_t>(mv.startup - 1);
    }
    if (c.realm) f.realmMeter = 0;
}

Vec3 dirToVelocity(const FighterState& f, const BodyStats& st, uint8_t dir, int speedPct) {
    Vec3 right{f.facing.z, Fx{}, -f.facing.x};
    Fx k = Fx::ratio(speedPct, 100);
    Fx w = st.walkSpeed * k, b = st.backSpeed * k, sd = st.sideSpeed * k;
    switch (dir) {
        case 6: return f.facing * w;
        case 4: return f.facing * -b;
        case 8: return right * -sd;
        case 2: return right * sd;
        case 9: return f.facing * w + right * -sd;
        case 7: return f.facing * -b + right * -sd;
        case 3: return f.facing * w + right * sd;
        case 1: return f.facing * -b + right * sd;
        default: return {};
    }
}

int speedPctOf(const MatchState& m, const MatchSetup& s, int i) {
    int p = mods(m, s, i).speedPct;
    // Geschlossener Realm: Besitzer muss ihn aufrechterhalten -> halbes Tempo. Offener Realm: volle Bewegung.
    if (m.realm.owner == i && !m.realm.open && m.realm.clashTimer == 0) p /= 2;
    return p;
}

void openRealm(MatchState& m, const MatchSetup& s, int i, Ev ev) {
    RealmState& r = m.realm;
    if (r.owner == 1 - i && r.clashTimer == 0) {
        // Gegen-Realm -> Clash (Input-Duell: Light hämmern)
        r.clashTimer = kClashFrames;
        r.challenger = static_cast<int8_t>(i);
        r.clashCount = {0, 0};
        for (FighterState& f : m.f) { f.state = FState::Clash; f.vel = {}; f.moveId = -1; }
        emit(ev, m, ImpactKind::RealmClash, i, ImpactClass::Crushing, m.f[i].pos, {}, Fx::fromInt(1));
        return;
    }
    r = RealmState{};
    r.owner = static_cast<int8_t>(i);
    r.timer = kRealmFrames;
    r.open = mods(m, s, i).openRealm;
    emit(ev, m, ImpactKind::RealmOpen, i, ImpactClass::Crushing, m.f[i].pos, {}, Fx::fromInt(1), r.open ? 1 : 0);
    if (const TechniqueLogic* l = logicOf(s, i)) l->onRealmOpen(TechContext{m, s, i, ev});
}

void tickResources(MatchState& m, const MatchSetup& s, int i, Ev ev) {
    FighterState& f = m.f[i];
    const BodyStats& st = s.stats[i];
    const PactModifiers& pm = mods(m, s, i);

    for (int16_t& cd : f.cooldown) {
        if (cd > 0) --cd;
        if (cd > 0 && buffed(m, s, i)) --cd; // BuffField: doppelt schnelle Cooldowns
    }
    if (f.burnout > 0) --f.burnout;
    if (f.pactTimer > 0) --f.pactTimer;

    const int32_t max = ceMaxOf(s, m, i);
    if (max <= 0) return;
    int32_t regen = 0;
    if (f.state != FState::Heal && !debuffed(m, s, i)) regen = st.ceRegen * pm.ceRegenPct / 100;
    regen -= pm.ceLeak;
    if (f.reinforce) regen -= 25;
    if (f.state == FState::Ward) regen -= st.wardDrain;
    if (f.state == FState::Heal) regen -= 150;
    const bool wasPositive = f.ce > 0;
    f.ce += regen;
    if (f.ce > max) f.ce = max;
    if (f.ce <= 0) {
        f.ce = 0;
        if (wasPositive && regen < 0) triggerBurnout(m, i, ev);
    }
}

void updateFighter(MatchState& m, const MatchSetup& s, int i, Ev ev) {
    FighterState& f = m.f[i];
    const FighterState& o = m.f[1 - i];
    const BodyStats& st = s.stats[i];
    const BodyDef& body = *s.loadout[i].body;
    const PactModifiers& pm = mods(m, s, i);
    const FrameInput& in = f.input.current();
    const uint8_t all = 0xFF;

    if (f.input.pressAge(BtnGuard, 1) == 0) f.guardPressFrame = m.frame;

    if (f.state != FState::Attack && f.state != FState::Hitstun && f.state != FState::Locked && f.state != FState::Clash)
        f.facing = normalizeXZ(o.pos - f.pos, f.facing);

    // Essenz-Burst geht auch aus Hit-/Blockstun (Notfall-Pushback)
    if (f.state == FState::Hitstun || f.state == FState::Blockstun) {
        Cmd c = resolveCommand(m, s, i, catBit(MoveCategory::Burst));
        if (c.move >= 0) {
            startMove(m, s, i, c, ev);
            emit(ev, m, ImpactKind::Burst, i, ImpactClass::Heavy, f.pos, {}, Fx::fromInt(1));
            return;
        }
    }

    switch (f.state) {
        case FState::Idle:
        case FState::Walk:
        case FState::Guard: {
            Cmd c = resolveCommand(m, s, i, all);
            if (c.ward) { f.state = FState::Ward; f.vel = {}; break; }
            if (c.move >= 0) { startMove(m, s, i, c, ev); f.vel = {}; break; }
            const bool mod = f.input.held(BtnMod);
            if (mod && f.input.held(BtnGuard) && f.ce > 0 && f.health < st.maxHealth) {
                f.state = FState::Heal; f.vel = {}; break;
            }
            if (f.input.pressedWithin(BtnDash, 3)) {
                f.input.consume(BtnDash, 3);
                if (mod) { // Verstärkung umschalten
                    f.reinforce = !f.reinforce && f.ce > 0 && f.burnout == 0;
                    break;
                }
                const bool dodge = f.input.held(BtnGuard);
                Vec3 dir = in.dir == 4 ? f.facing * Fx::fromInt(-1) : f.facing;
                if (in.dir == 8 || in.dir == 2) dir = normalizeXZ(dirToVelocity(f, st, in.dir, 100), f.facing);
                f.vel = dir * (st.dashSpeed * Fx::ratio(speedPctOf(m, s, i), 100));
                f.state = dodge ? FState::Dodge : FState::Dash;
                f.stateFrame = 0;
                emit(ev, m, ImpactKind::DashDust, i, ImpactClass::Medium, f.pos, dir, st.dashSpeed);
                break;
            }
            if (f.input.pressedWithin(BtnJump, 3)) {
                f.input.consume(BtnJump, 3);
                Vec3 h = dirToVelocity(f, st, in.dir, speedPctOf(m, s, i));
                f.vel = {h.x, st.jumpVelocity, h.z};
                f.state = FState::Air;
                f.grounded = false;
                emit(ev, m, ImpactKind::DashDust, i, ImpactClass::Light, f.pos, {}, st.jumpVelocity);
                break;
            }
            if ((in.buttons & BtnGuard) && !pm.noBlock) { f.state = FState::Guard; f.vel = {}; break; }
            f.vel = dirToVelocity(f, st, in.dir, speedPctOf(m, s, i));
            f.state = (in.dir == 5) ? FState::Idle : FState::Walk;
            break;
        }
        case FState::Dash:
        case FState::Dodge: {
            if (f.state == FState::Dash) {
                Cmd c = resolveCommand(m, s, i, all);
                if (c.move >= 0) { startMove(m, s, i, c, ev); break; }
            }
            int len = f.state == FState::Dodge ? 16 : st.dashFrames;
            if (++f.stateFrame >= len) { f.state = FState::Idle; f.vel = {}; }
            break;
        }
        case FState::Air: {
            Cmd c = resolveCommand(m, s, i, all);
            if (c.move >= 0) startMove(m, s, i, c, ev);
            break;
        }
        case FState::Attack: {
            const MoveDef& mv = moveOf(s, i, f.moveId);
            ++f.moveFrame;
            if (f.grounded) {
                bool moving = f.moveFrame >= mv.moveFirst && f.moveFrame <= mv.moveLast;
                Vec3 v = moving ? f.facing * mv.forwardSpeed : Vec3{};
                f.vel = {v.x, f.vel.y, v.z};
            }
            if (f.moveFrame == mv.eventFrame) {
                if (mv.category == MoveCategory::Realm) openRealm(m, s, i, ev);
                else if (f.moveId >= kTechMoveOffset)
                    if (const TechniqueLogic* l = logicOf(s, i))
                        l->onMoveEvent(TechContext{m, s, i, ev}, static_cast<int16_t>(f.moveId - kTechMoveOffset));
                if (f.state != FState::Attack) break; // z. B. Clash gestartet
            }
            if (f.moveHasHit) {
                if (mv.cancelMask) {
                    Cmd c = resolveCommand(m, s, i, mv.cancelMask);
                    if (c.move >= 0) { startMove(m, s, i, c, ev); break; }
                }
                if (body.has(PassiveDashCancel) && f.grounded && f.input.pressedWithin(BtnDash, 3)) {
                    f.input.consume(BtnDash, 3);
                    f.vel = f.facing * st.dashSpeed;
                    f.state = FState::Dash;
                    f.stateFrame = 0;
                    f.moveId = -1;
                    break;
                }
            }
            if (f.moveFrame >= mv.total()) {
                f.moveId = -1;
                f.state = f.grounded ? FState::Idle : FState::Air;
            }
            break;
        }
        case FState::Hitstun:
        case FState::Blockstun:
            if (f.stun > 0) --f.stun;
            if (f.stun == 0 && f.grounded) { f.state = FState::Idle; f.comboTaken = 0; }
            break;
        case FState::Knockdown:
            if (++f.stateFrame >= 30) { f.state = FState::Idle; f.comboTaken = 0; f.launched = false; }
            break;
        case FState::Heal: {
            const int16_t limit = static_cast<int16_t>(st.maxHealth * 3 / 10 * pm.healPct / 100);
            bool keep = f.input.held(BtnMod) && f.input.held(BtnGuard) && f.ce > 0 && f.healed < limit &&
                        f.health < st.maxHealth;
            if (!keep) { f.state = FState::Idle; break; }
            if ((m.frame & 1) == 0) { ++f.health; ++f.healed; }
            break;
        }
        case FState::Ward: {
            bool keep = f.input.held(BtnUlt) && f.ce > 0 && m.realm.owner == 1 - i;
            if (!keep) f.state = FState::Idle;
            break;
        }
        case FState::Locked:
            if (f.lock > 0) --f.lock;
            if (f.lock == 0) f.state = FState::Idle;
            break;
        case FState::Clash:
            if (f.input.pressedWithin(BtnLight, 1)) { f.input.consume(BtnLight, 1); ++m.realm.clashCount[i]; }
            break;
    }
}

void physics(MatchState& m, const MatchSetup& s, int i, Ev ev) {
    FighterState& f = m.f[i];
    const BodyStats& st = s.stats[i];
    if (!f.grounded) f.vel.y -= st.gravity;
    f.pos += f.vel;

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
            ImpactClass c = impactSpeed > Fx::ratio(1, 2) ? ImpactClass::Crushing
                          : impactSpeed > Fx::ratio(1, 4) ? ImpactClass::Heavy : ImpactClass::Medium;
            emit(ev, m, ImpactKind::GroundSlam, i, c, f.pos, {Fx{}, Fx::fromInt(-1), Fx{}}, impactSpeed);
        } else if (f.state == FState::Attack) {
            emit(ev, m, ImpactKind::Landing, i, ImpactClass::Light, f.pos, {}, impactSpeed);
        }
    }

    // Zerstörbare Wände (gameplay-relevant)
    const Fx h = s.stage.halfExtent;
    Fx* axis[4] = {&f.pos.x, &f.pos.x, &f.pos.z, &f.pos.z};
    Fx* vaxis[4] = {&f.vel.x, &f.vel.x, &f.vel.z, &f.vel.z};
    for (int w = 0; w < 4; ++w) {
        const bool positive = (w % 2) == 0;
        WallState& wall = m.walls[w];
        Fx bound = wall.broken ? h + h : h;
        Fx p = *axis[w];
        if (!(positive ? p > bound : p < -bound)) continue;
        *axis[w] = positive ? bound : -bound;
        Fx speed = abs(*vaxis[w]);
        if (!wall.broken && f.state == FState::Hitstun && speed >= s.stage.wallImpactSpeed) {
            wall.hp = static_cast<int16_t>(wall.hp - (speed * Fx::fromInt(400)).toInt());
            Vec3 n{};
            if (w < 2) n.x = positive ? Fx::fromInt(1) : Fx::fromInt(-1);
            else n.z = positive ? Fx::fromInt(1) : Fx::fromInt(-1);
            if (wall.hp <= 0) {
                wall.broken = true;
                emit(ev, m, ImpactKind::WallBreak, i, ImpactClass::Crushing, f.pos, n, speed);
                continue;
            }
            emit(ev, m, ImpactKind::WallSplat, i, ImpactClass::Heavy, f.pos, n, speed);
            f.stun = static_cast<int16_t>(f.stun + 10);
        }
        *vaxis[w] = -(*vaxis[w]) * Fx::ratio(1, 2);
    }
}

void pushApart(MatchState& m, const MatchSetup& s) {
    FighterState& a = m.f[0];
    FighterState& b = m.f[1];
    if (abs(a.pos.y - b.pos.y) > Fx::fromInt(2)) return;
    Vec3 diff{b.pos.x - a.pos.x, Fx{}, b.pos.z - a.pos.z};
    Fx minDist = s.stats[0].pushRadius + s.stats[1].pushRadius;
    Fx dist = length(diff);
    if (dist >= minDist) return;
    Vec3 half = normalizeXZ(diff, a.facing) * ((minDist - dist) * Fx::ratio(1, 2));
    a.pos = a.pos - half;
    b.pos = b.pos + half;
}

bool isInvulnerable(const MatchState& m, const MatchSetup& s, int i) {
    const FighterState& f = m.f[i];
    if (f.state == FState::Dodge) return f.stateFrame >= 1 && f.stateFrame <= 10;
    if (f.state == FState::Clash) return true;
    if (f.state != FState::Attack) return false;
    const MoveDef& mv = moveOf(s, i, f.moveId);
    return f.moveFrame >= mv.invulnFirst && f.moveFrame <= mv.invulnLast;
}

struct PendingHit {
    bool valid = false;
    const MoveDef* move = nullptr;
    int16_t moveId = -1; // wie FighterState::moveId (Projektil: Index + 100)
    bool projectile = false;
    Vec3 point{}, dir{};
};

void addRealmMeter(FighterState& f, int amount) {
    int v = f.realmMeter + amount;
    f.realmMeter = static_cast<int16_t>(v > kRealmMeterMax ? kRealmMeterMax : v);
}

void applyHit(MatchState& m, const MatchSetup& s, int atk, const PendingHit& h, Ev ev) {
    FighterState& a = m.f[atk];
    const int di = 1 - atk;
    FighterState& d = m.f[di];
    const MoveDef& mv = *h.move;
    const HitProps& p = mv.hit;
    const TechContext actx{m, s, atk, ev};
    const TechContext dctx{m, s, di, ev};
    const TechniqueLogic* al = logicOf(s, atk);
    const TechniqueLogic* dl = logicOf(s, di);
    const int16_t techIdx = h.moveId >= kTechMoveOffset ? static_cast<int16_t>(h.moveId - kTechMoveOffset) : -1;

    // Core Rules wie ein Auto-Block-Feld
    if (dl && dl->nullifiesIncoming(dctx, mv, h.projectile)) {
        a.hitstop = d.hitstop = 4;
        emit(ev, m, ImpactKind::Nullified, di, p.impact, h.point, h.dir, p.knockback);
        return;
    }

    const bool facingAtk = dot(d.facing, a.pos - d.pos) > Fx{};
    const bool guarding = d.state == FState::Guard || d.state == FState::Blockstun;
    const bool blocked = !p.unblockable && d.grounded && facingAtk && guarding;
    a.hitstop = d.hitstop = p.hitstop;

    if (blocked) {
        const bool parry = d.state == FState::Guard && m.frame - d.guardPressFrame <= s.stats[di].parryWindow &&
                           mv.category != MoveCategory::Apex;
        if (parry) {
            if (!h.projectile) { a.state = FState::Hitstun; a.stun = 18; a.moveId = -1; a.vel = {}; }
            addRealmMeter(d, 100);
            emit(ev, m, ImpactKind::Parry, di, p.impact, h.point, h.dir, p.knockback);
            if (dl) dl->onParry(dctx, mv);
            return;
        }
        d.state = FState::Blockstun;
        d.stun = static_cast<int16_t>(p.blockstun * (200 - mods(m, s, di).blockPct) / 100);
        d.vel = h.dir * (p.knockback * Fx::ratio(1, 2));
        emit(ev, m, ImpactKind::Block, atk, p.impact, h.point, h.dir, p.knockback);
        return;
    }

    // Schaden berechnen
    int dmg = p.damage;
    int scale = 100 - d.comboTaken * 10;
    dmg = dmg * (scale < 30 ? 30 : scale) / 100;
    dmg = dmg * s.stats[atk].damagePct / 100;
    dmg = dmg * mods(m, s, atk).damagePct / 100;
    if (al) dmg = dmg * al->damageDealtPct(actx, techIdx) / 100;
    if (dl) dmg = dmg * dl->damageTakenPct(dctx) / 100;
    if (buffed(m, s, atk) || debuffed(m, s, di)) dmg = dmg * 115 / 100;
    const bool ceAttack = h.projectile || mv.category == MoveCategory::Skill || mv.category == MoveCategory::Apex;
    if (a.reinforce) dmg = dmg * 120 / 100;
    if (d.reinforce && ceAttack) dmg = dmg * 80 / 100;
    if (mv.category == MoveCategory::Grab && s.loadout[atk].body->has(PassiveStrongGrab)) dmg = dmg * 150 / 100;
    if (!h.projectile && s.loadout[atk].body->has(PassiveFlankCrit) && dot(d.facing, h.dir) > Fx::ratio(-1, 2))
        dmg = dmg * 130 / 100; // Treffer von Seite/Rücken

    // Nullfunke: Normal perfekt auf das Hitstop-Ende des vorherigen Treffers getimed
    bool zeroSpark = false;
    if (!h.projectile && mv.category == MoveCategory::Normal) {
        int window = s.stats[atk].zeroSparkWindow + mods(m, s, atk).extraZeroSpark;
        int diff = a.pressFrame - a.linkFrame;
        if (diff < 0) diff = -diff;
        if (diff <= window) {
            zeroSpark = true;
            dmg = dmg * 250 / 100;
            addRealmMeter(a, 250);
            a.hitstop = d.hitstop = static_cast<int16_t>(p.hitstop + 8);
            emit(ev, m, ImpactKind::ZeroSpark, atk, ImpactClass::Crushing, h.point, h.dir, p.knockback);
        }
    }

    // Super Armor: Treffer wird geschluckt (Schaden ja, Stun nein)
    if (d.state == FState::Attack && d.armorLeft > 0) {
        const MoveDef& dm = moveOf(s, di, d.moveId);
        if (d.moveFrame < dm.startup + dm.active) {
            --d.armorLeft;
            d.health = static_cast<int16_t>(d.health - dmg);
            emit(ev, m, ImpactKind::Hit, atk, p.impact, h.point, h.dir, Fx{}, 1);
            return;
        }
    }

    d.health = static_cast<int16_t>(d.health - dmg);
    d.state = FState::Hitstun;
    d.stun = p.hitstun;
    d.moveId = -1;
    d.vel = h.dir * p.knockback + Vec3{Fx{}, p.launch, Fx{}};
    if (p.launch > Fx{}) { d.grounded = false; d.launched = true; }
    ++d.comboTaken;
    a.linkFrame = m.frame + a.hitstop + 1;
    addRealmMeter(a, dmg / 2 * s.stats[atk].realmGainPct / 100 * mods(m, s, atk).realmGainPct / 100);
    addRealmMeter(d, dmg / 4);
    if (!zeroSpark) emit(ev, m, ImpactKind::Hit, atk, p.impact, h.point, h.dir, p.knockback + p.launch);
    if (al) al->onHitLanded(actx, techIdx, h.projectile);
}

PendingHit checkMelee(const MatchState& m, const MatchSetup& s, int atk) {
    const FighterState& a = m.f[atk];
    if (a.state != FState::Attack || a.moveHasHit) return {};
    const MoveDef& mv = moveOf(s, atk, a.moveId);
    if (!mv.isActiveFrame(a.moveFrame) || isInvulnerable(m, s, 1 - atk)) return {};
    if (mv.category == MoveCategory::Grab && (!m.f[1 - atk].grounded || m.f[1 - atk].state == FState::Hitstun)) return {};
    MatchState& mm = const_cast<MatchState&>(m);
    const TechContext actx{mm, s, atk, nullptr};
    const TechContext dctx{mm, s, 1 - atk, nullptr};
    const bool base = a.moveId < kTechMoveOffset;
    // Hurtbox: einfache Kapsel aus 3 Kugeln (Beine, Torso, Kopf)
    static const HitboxDef hurt[3] = {
        {{Fx{}, Fx::ratio(5, 10), Fx{}}, Fx::ratio(45, 100)},
        {{Fx{}, Fx::ratio(12, 10), Fx{}}, Fx::ratio(45, 100)},
        {{Fx{}, Fx::ratio(17, 10), Fx{}}, Fx::ratio(30, 100)},
    };
    for (const HitboxDef& hb : mv.hitboxes) {
        Vec3 off = hb.offset;
        if (base) off.z = off.z * s.stats[atk].reach; // Range-Stat des Bodys
        Vec3 c = actx.localToWorld(off);
        for (const HitboxDef& hu : hurt)
            if (overlap(c, hb.radius, dctx.localToWorld(hu.offset), hu.radius))
                return {true, &mv, a.moveId, false, c, a.facing};
    }
    return {};
}

void updateProjectiles(MatchState& m, const MatchSetup& s, Ev ev) {
    const Fx limit = s.stage.halfExtent * Fx::fromInt(3);
    for (Projectile& p : m.proj) {
        if (!p.alive) continue;
        if (p.armDelay > 0) { --p.armDelay; continue; }
        p.pos += p.vel;
        const int target = 1 - p.owner;
        const TechContext dctx{m, s, target, nullptr};
        bool hit = false;
        if (!isInvulnerable(m, s, target)) {
            for (Fx y : {Fx::ratio(5, 10), Fx::ratio(12, 10), Fx::ratio(17, 10)})
                if (overlap(p.pos, p.radius, dctx.localToWorld({Fx{}, y, Fx{}}), Fx::ratio(45, 100))) { hit = true; break; }
        }
        if (hit) {
            const MoveDef& mv = s.loadout[p.owner].technique->moves[p.moveIndex];
            PendingHit h{true, &mv, static_cast<int16_t>(p.moveIndex + kTechMoveOffset), true, p.pos,
                         normalizeXZ(p.vel, m.f[p.owner].facing)};
            if (p.vel == Vec3{}) h.dir = normalizeXZ(m.f[target].pos - p.pos, m.f[p.owner].facing);
            applyHit(m, s, p.owner, h, ev);
            p.alive = false;
            continue;
        }
        if (--p.life <= 0 || abs(p.pos.x) > limit || abs(p.pos.z) > limit) {
            p.alive = false;
            emit(ev, m, ImpactKind::ProjectileBurst, p.owner, ImpactClass::Medium, p.pos, {}, length(p.vel));
        }
    }
}

void tickRealm(MatchState& m, const MatchSetup& s, Ev ev) {
    RealmState& r = m.realm;
    if (r.owner < 0) return;

    if (r.clashTimer > 0) {
        if (--r.clashTimer > 0) return;
        const int owner = r.owner, ch = r.challenger;
        const int winner = r.clashCount[ch] > r.clashCount[owner] ? ch : owner; // Gleichstand: Verteidiger hält
        for (FighterState& f : m.f) f.state = FState::Idle;
        r = RealmState{};
        r.owner = static_cast<int8_t>(winner);
        r.timer = kRealmFrames;
        r.open = mods(m, s, winner).openRealm;
        emit(ev, m, ImpactKind::RealmOpen, winner, ImpactClass::Crushing, m.f[winner].pos, {}, Fx::fromInt(1), 2);
        return;
    }

    const int o = r.owner, t = 1 - o;
    const int elapsed = kRealmFrames - r.timer;
    FighterState& tf = m.f[t];
    if (realmAffects(m, s, t)) {
        switch (realmClass(m, s)) {
            case RealmClass::Damage:
                if (elapsed % 30 == 0) {
                    tf.health = static_cast<int16_t>(tf.health - 20);
                    emit(ev, m, ImpactKind::SureHit, o, ImpactClass::Medium, tf.pos, {}, Fx{}, 0);
                }
                break;
            case RealmClass::Lock:
                if (elapsed % 200 == 0 && tf.state != FState::Attack) {
                    tf.health = static_cast<int16_t>(tf.health - 5);
                    tf.state = FState::Locked;
                    tf.lock = 90;
                    tf.vel = {};
                    emit(ev, m, ImpactKind::SureHit, o, ImpactClass::Heavy, tf.pos, {}, Fx{}, 1);
                }
                break;
            case RealmClass::Debuff:
            case RealmClass::BuffField:
                break; // wirken über debuffed()/buffed()
        }
    }
    if (const TechniqueLogic* l = logicOf(s, o)) l->onRealmTick(TechContext{m, s, o, ev});
    if (--r.timer <= 0) {
        emit(ev, m, ImpactKind::RealmEnd, o, ImpactClass::Medium, m.f[o].pos, {}, Fx{});
        r = RealmState{};
        if (m.f[t].state == FState::Ward) m.f[t].state = FState::Idle;
    }
}

} // namespace

MatchState createMatch(const MatchSetup& s) {
    MatchState m{};
    for (int i = 0; i < 2; ++i) {
        FighterState& f = m.f[i];
        const PactModifiers& pm = s.pact[i].tier == PactTier::None ? kNeutralPact : s.pact[i].mods;
        f.health = static_cast<int16_t>(s.stats[i].maxHealth * pm.maxHealthPct / 100);
        f.ce = s.stats[i].ceMax * pm.cePoolPct / 100;
        f.pactTimer = pm.durationFrames;
        f.pos = {Fx{}, Fx{}, Fx::fromInt(i == 0 ? -3 : 3)};
        f.facing = {Fx{}, Fx{}, Fx::fromInt(i == 0 ? 1 : -1)};
    }
    for (WallState& w : m.walls) w.hp = s.stage.wallHp;
    return m;
}

void step(MatchState& m, const MatchSetup& s, const FrameInput inputs[2], EventSink* ev) {
    if (m.winner != 255) return;
    for (int i = 0; i < 2; ++i) m.f[i].input.push(inputs[i]);

    bool frozen[2];
    for (int i = 0; i < 2; ++i) {
        frozen[i] = m.f[i].hitstop > 0;
        if (frozen[i]) --m.f[i].hitstop;
    }

    for (int i = 0; i < 2; ++i) tickResources(m, s, i, ev);
    for (int i = 0; i < 2; ++i) if (!frozen[i]) updateFighter(m, s, i, ev);
    for (int i = 0; i < 2; ++i) if (!frozen[i]) physics(m, s, i, ev);
    pushApart(m, s);

    for (int i = 0; i < 2; ++i)
        if (const TechniqueLogic* l = logicOf(s, i)) l->onTick(TechContext{m, s, i, ev});

    // Treffer beider Seiten erst sammeln, dann anwenden -> echte Trades
    PendingHit hits[2];
    for (int i = 0; i < 2; ++i) if (!frozen[i]) hits[i] = checkMelee(m, s, i);
    for (int i = 0; i < 2; ++i) {
        if (!hits[i].valid) continue;
        m.f[i].moveHasHit = true;
        applyHit(m, s, i, hits[i], ev);
    }

    updateProjectiles(m, s, ev);
    tickRealm(m, s, ev);

    const bool dead0 = m.f[0].health <= 0, dead1 = m.f[1].health <= 0;
    if (dead0 && dead1) m.winner = 2;
    else if (dead0) m.winner = 1;
    else if (dead1) m.winner = 0;

    ++m.frame;
}

// ============================================================ Hash
namespace {
struct Hasher {
    uint64_t h = 14695981039346656037ull;
    void add(uint64_t v) { for (int i = 0; i < 8; ++i) { h ^= (v >> (i * 8)) & 0xFF; h *= 1099511628211ull; } }
    void add(Fx v) { add(static_cast<uint64_t>(static_cast<uint32_t>(v.raw))); }
    void add(Vec3 v) { add(v.x); add(v.y); add(v.z); }
    template <typename T> void addInt(T v) { add(static_cast<uint64_t>(static_cast<int64_t>(v))); }
};
} // namespace

uint64_t InputBuffer::hash(uint64_t h) const {
    Hasher x{h};
    x.add(head_);
    for (const FrameInput& in : hist_) x.add(static_cast<uint64_t>(in.dir | (in.buttons << 8) | (in.consumed << 16)));
    return x.h;
}

uint64_t hashState(const MatchState& m) {
    Hasher x;
    x.addInt(m.frame);
    for (const FighterState& f : m.f) {
        x.add(f.pos); x.add(f.vel); x.add(f.facing);
        x.addInt(f.ce); x.addInt(f.pressFrame); x.addInt(f.linkFrame); x.addInt(f.guardPressFrame); x.addInt(f.pactTimer);
        x.addInt(f.health); x.addInt(f.realmMeter); x.addInt(f.stateFrame); x.addInt(f.moveId); x.addInt(f.moveFrame);
        x.addInt(f.stun); x.addInt(f.hitstop); x.addInt(f.comboTaken); x.addInt(f.burnout); x.addInt(f.lock);
        x.addInt(f.healed);
        for (int16_t c : f.cooldown) x.addInt(c);
        x.addInt(static_cast<int>(f.state)); x.addInt(f.armorLeft);
        x.addInt(f.moveHasHit | (f.grounded << 1) | (f.launched << 2) | (f.reinforce << 3) | (f.apexUsed << 4));
        x.h = f.input.hash(x.h);
        for (int32_t t : f.tech) x.addInt(t);
    }
    for (const Projectile& p : m.proj) {
        x.addInt(p.alive);
        if (!p.alive) continue;
        x.addInt(p.owner); x.addInt(p.moveIndex); x.addInt(p.life); x.addInt(p.armDelay);
        x.add(p.pos); x.add(p.vel); x.add(p.radius);
    }
    for (const WallState& w : m.walls) { x.addInt(w.hp); x.addInt(w.broken); }
    const RealmState& r = m.realm;
    x.addInt(r.owner); x.addInt(r.challenger); x.addInt(r.open); x.addInt(r.timer); x.addInt(r.clashTimer);
    x.addInt(r.clashCount[0]); x.addInt(r.clashCount[1]);
    x.addInt(m.winner);
    return x.h;
}

} // namespace fc
