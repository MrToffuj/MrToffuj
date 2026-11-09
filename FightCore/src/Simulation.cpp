#include "fightcore/Simulation.h"

#include "Internal.h"

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

ModeRules rulesFor(GameMode mode) {
    ModeRules r;
    switch (mode) {
        case GameMode::Duel:
            break;
        case GameMode::Teams:
            r.fighters = 4;
            r.teams = true;
            r.arenaHalfExtent = Fx::fromInt(14);
            break;
        case GameMode::Brawl:
            r.fighters = kMaxFighters;
            r.respawnFrames = 300;            // 5 s
            r.timeLimitFrames = 5 * 60 * 60;  // 5 min
            r.koLimit = 15;
            r.arenaHalfExtent = Fx::fromInt(60);
            break;
    }
    return r;
}

void MatchSetup::finalize() {
    const ModeRules rules = rulesFor(mode);
    if (count > rules.fighters) count = rules.fighters;
    if (count < 2) count = 2;
    stage.halfExtent = rules.arenaHalfExtent;
    for (int i = 0; i < count; ++i) {
        if (!rules.teams) loadout[i].team = static_cast<uint8_t>(i);
        stats[i] = deriveStats(*loadout[i].body);
        pact[i] = evaluatePact(loadout[i].pact, *loadout[i].body, loadout[i].technique, loadout[i].profile);
    }
    detail::buildRoster(*this);
}

bool isEnemy(const MatchState& m, int a, int b) { return a != b && m.f[a].team != m.f[b].team; }

// ============================================================ TechContext
FighterState& TechContext::me() const { return m.f[self]; }
FighterState& TechContext::opp() const { return m.f[hasTarget() ? m.f[self].target : self]; }
bool TechContext::hasTarget() const { return m.f[self].target >= 0; }
const BodyStats& TechContext::stats() const { return setup.stats[self]; }
bool TechContext::realmIsMine() const { return m.f[self].realm.active(); }

Vec3 TechContext::localToWorld(Vec3 off) const {
    const FighterState& f = me();
    Vec3 right{f.facing.z, Fx{}, -f.facing.x};
    return f.pos + right * off.x + Vec3{Fx{}, off.y, Fx{}} + f.facing * off.z;
}

int TechContext::spawnProjectile(Vec3 pos, Vec3 vel, Fx radius, int16_t life, int16_t moveIndex, int16_t armDelay) const {
    for (int i = 0; i < kMaxProjectiles; ++i) {
        Projectile& p = m.proj[i];
        if (p.alive) continue;
        p = Projectile{true, static_cast<uint8_t>(self), 0, moveIndex, life, armDelay, pos, vel, radius};
        return i;
    }
    return -1;
}

void TechContext::emit(ImpactKind kind, Vec3 pos, Vec3 dir, Fx force, ImpactClass strength, int16_t tag) const {
    if (events) events->push_back({m.frame, kind, static_cast<uint8_t>(self), strength, tag, pos, dir, force});
}

void TechContext::damage(int target, int16_t amount) const {
    FighterState& t = m.f[target];
    if (t.out) return;
    t.health = static_cast<int16_t>(t.health - amount);
    t.lastHitBy = static_cast<int8_t>(self);
}

// ============================================================ Simulation
namespace detail {

void emit(Ev ev, const MatchState& m, ImpactKind k, int src, ImpactClass s, Vec3 pos, Vec3 dir, Fx force, int16_t tag) {
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

// ---- Realms
bool inRealmArea(const MatchState& m, int owner, Vec3 p) {
    const FighterState& o = m.f[owner];
    const Vec3 center = o.realm.open ? o.pos : o.realm.center;
    const Fx radius = Fx::fromInt(o.realm.open ? kOpenRealmRadius : kRealmRadius);
    return distXZ(center, p) <= radius;
}

bool realmAffects(const MatchState& m, const MatchSetup& s, int owner, int target) {
    const FighterState& t = m.f[target];
    if (!m.f[owner].realm.active() || !isEnemy(m, owner, target) || t.out) return false;
    if (s.stats[target].sureHitImmune || t.state == FState::Ward) return false;
    return inRealmArea(m, owner, t.pos);
}

int enemyRealmAround(const MatchState& m, int i) {
    for (int o = 0; o < m.count; ++o)
        if (m.f[o].realm.active() && isEnemy(m, o, i) && inRealmArea(m, o, m.f[i].pos)) return o;
    return -1;
}

RealmClass realmClassOf(const MatchSetup& s, int owner) { return s.loadout[owner].technique->realmClass; }

bool debuffed(const MatchState& m, const MatchSetup& s, int i) {
    for (int o = 0; o < m.count; ++o)
        if (realmAffects(m, s, o, i) && realmClassOf(s, o) == RealmClass::Debuff) return true;
    return false;
}
bool buffed(const MatchState& m, const MatchSetup& s, int i) {
    return m.f[i].realm.active() && realmClassOf(s, i) == RealmClass::BuffField;
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

// ---- Ziele
int nearestEnemy(const MatchState& m, int i) {
    int best = -1;
    int64_t bestDist = 0;
    for (int j = 0; j < m.count; ++j) {
        if (!isEnemy(m, i, j) || m.f[j].out) continue;
        const Vec3 d = m.f[j].pos - m.f[i].pos;
        const int64_t dist = lengthSqRaw(Vec3{d.x, Fx{}, d.z});
        if (best < 0 || dist < bestDist) { best = j; bestDist = dist; }
    }
    return best;
}

void updateTarget(MatchState& m, int i) {
    FighterState& f = m.f[i];
    const bool valid = f.target >= 0 && isEnemy(m, i, f.target) && !m.f[f.target].out;
    if (f.input.pressedWithin(BtnTarget, 3)) {
        f.input.consume(BtnTarget, 3);
        // Nächster lebender Gegner nach Index (zyklisch)
        for (int k = 1; k <= m.count; ++k) {
            const int j = (f.target + k + m.count) % m.count;
            if (isEnemy(m, i, j) && !m.f[j].out) { f.target = static_cast<int8_t>(j); f.manualTarget = true; return; }
        }
    }
    if (!valid) {
        f.target = static_cast<int8_t>(nearestEnemy(m, i));
        f.manualTarget = false;
    } else if (!f.manualTarget && (m.frame + i) % 20 == 0) {
        f.target = static_cast<int8_t>(nearestEnemy(m, i)); // Auto-Lock auf den Nächsten
    }
}

// ---- Commands
struct Cmd {
    int16_t move = -1;
    int8_t skill = -1;
    int32_t ceCost = 0;
    bool apex = false, realm = false, ward = false;
    int age = 0;
    uint16_t button = 0;
};

Cmd resolveCommand(MatchState& m, const MatchSetup& s, int i, uint8_t allowed) {
    FighterState& f = m.f[i];
    const TechniqueDef* T = s.loadout[i].technique;
    const PactModifiers& pm = mods(m, s, i);
    auto age = [&](uint16_t b) { return f.input.pressAge(b, kBufferWindow); };
    // Modifier/Guard zählen im Frame des Tastendrucks, nicht beim (gebufferten) Auslesen.
    auto heldAt = [&](int a, uint16_t b) { return (f.input.ago(a).buttons & b) != 0; };
    auto ok = [&](Cmd c, uint16_t btn, int a) { c.button = btn; c.age = a; f.input.consume(btn, kBufferWindow); return c; };
    int a;

    if ((a = age(BtnUlt)) >= 0) {
        const bool mod = heldAt(a, BtnMod);
        if (!mod && enemyRealmAround(m, i) >= 0) {
            Cmd c; c.ward = true; return ok(c, BtnUlt, a);
        }
        if (mod) {
            bool can = T && !pm.noRealm && f.realmMeter >= kRealmMeterMax && f.realm.timer == 0 &&
                       f.realm.clashWith < 0 && (allowed & catBit(MoveCategory::Realm));
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

    const uint16_t btns[3] = {BtnLight, BtnHeavy, BtnGrab};
    for (int slot = 0; slot < 3; ++slot) {
        if ((a = age(btns[slot])) < 0 || !heldAt(a, BtnMod)) continue;
        // Mod gehalten -> Skill oder nichts (nie versehentliche Normals)
        if (!T || f.burnout > 0 || f.cooldown[slot] > 0 || (pm.lockedSkills & (1 << slot))) return {};
        if (T->logic && !T->logic->canUseSkill(TechContext{m, s, i, nullptr}, slot)) return {};
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
        Cmd c; c.move = f.input.ago(a).dir <= 3 ? kLauncher : kHeavy; return ok(c, BtnHeavy, a); // zurück + Heavy
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
        case 8: return f.facing * w;
        case 2: return f.facing * -b;
        case 4: return right * -sd;
        case 6: return right * sd;
        case 7: return f.facing * w + right * -sd;
        case 9: return f.facing * w + right * sd;
        case 1: return f.facing * -b + right * -sd;
        case 3: return f.facing * -b + right * sd;
        default: return {};
    }
}

int speedPctOf(const MatchState& m, const MatchSetup& s, int i) {
    int p = mods(m, s, i).speedPct;
    // Geschlossener Realm: Besitzer muss ihn aufrechterhalten -> halbes Tempo. Offener Realm: volle Bewegung.
    const RealmState& r = m.f[i].realm;
    if (r.active() && !r.open) p /= 2;
    return p;
}

void openRealm(MatchState& m, const MatchSetup& s, int i, Ev ev) {
    FighterState& f = m.f[i];
    const int defender = enemyRealmAround(m, i);
    if (defender >= 0 && m.f[defender].realm.clashWith < 0) {
        // Gegen-Realm im gegnerischen Realm -> Clash (Input-Duell: Light hämmern)
        for (int k : {i, defender}) {
            FighterState& x = m.f[k];
            x.realm.clashWith = static_cast<int8_t>(k == i ? defender : i);
            x.realm.clashTimer = kClashFrames;
            x.realm.clashCount = 0;
            x.state = FState::Clash;
            x.vel = {};
            x.moveId = -1;
        }
        emit(ev, m, ImpactKind::RealmClash, i, ImpactClass::Crushing, f.pos, {}, Fx::fromInt(1), static_cast<int16_t>(defender));
        return;
    }
    f.realm = RealmState{};
    f.realm.timer = kRealmFrames;
    f.realm.open = mods(m, s, i).openRealm;
    f.realm.center = f.pos;
    emit(ev, m, ImpactKind::RealmOpen, i, ImpactClass::Crushing, f.pos, {}, Fx::fromInt(1), f.realm.open ? 1 : 0);
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
    regen -= companionUpkeep(m, s, i);
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
    const BodyStats& st = s.stats[i];
    const BodyDef& body = *s.loadout[i].body;
    const PactModifiers& pm = mods(m, s, i);
    const FrameInput& in = f.input.current();
    const uint8_t all = 0xFF;

    if (f.input.pressAge(BtnGuard, 1) == 0) f.guardPressFrame = m.frame;
    updateTarget(m, i);

    if (f.target >= 0 && f.state != FState::Attack && f.state != FState::Hitstun && f.state != FState::Locked &&
        f.state != FState::Clash)
        f.facing = normalizeXZ(m.f[f.target].pos - f.pos, f.facing);

    // Essenz-Burst geht auch aus Hit-/Blockstun (Notfall-Pushback)
    if (f.state == FState::Hitstun || f.state == FState::Blockstun) {
        Cmd c = resolveCommand(m, s, i, catBit(MoveCategory::Burst));
        if (c.move >= 0) {
            startMove(m, s, i, c, ev);
            emit(ev, m, ImpactKind::Burst, i, ImpactClass::Heavy, f.pos, {}, Fx::fromInt(1));
            return;
        }
    }

    // Begleiter rufen geht parallel zu fast allem (nicht aus Stun / Lock / Clash)
    if (f.state != FState::Hitstun && f.state != FState::Blockstun && f.state != FState::Locked &&
        f.state != FState::Clash && f.state != FState::Knockdown)
        tryAssistCommand(m, s, i, ev);

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
                Vec3 dir = in.dir == 2 ? f.facing * Fx::fromInt(-1) : f.facing;
                if (in.dir == 4 || in.dir == 6) dir = normalizeXZ(dirToVelocity(f, st, in.dir, 100), f.facing);
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
            bool keep = f.input.held(BtnUlt) && f.ce > 0 && enemyRealmAround(m, i) >= 0;
            if (!keep) f.state = FState::Idle;
            break;
        }
        case FState::Locked:
            if (f.lock > 0) --f.lock;
            if (f.lock == 0) f.state = FState::Idle;
            break;
        case FState::Clash:
            if (f.input.pressedWithin(BtnLight, 1)) { f.input.consume(BtnLight, 1); ++f.realm.clashCount; }
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
    for (int i = 0; i < m.count; ++i) {
        if (m.f[i].out) continue;
        for (int j = i + 1; j < m.count; ++j) {
            FighterState& a = m.f[i];
            FighterState& b = m.f[j];
            if (b.out || abs(a.pos.y - b.pos.y) > Fx::fromInt(2)) continue;
            Vec3 diff{b.pos.x - a.pos.x, Fx{}, b.pos.z - a.pos.z};
            const Fx minDist = s.stats[i].pushRadius + s.stats[j].pushRadius;
            if (abs(diff.x) >= minDist || abs(diff.z) >= minDist) continue; // schneller Ausschluss
            const Fx dist = length(diff);
            if (dist >= minDist) continue;
            Vec3 half = normalizeXZ(diff, a.facing) * ((minDist - dist) * Fx::ratio(1, 2));
            a.pos = a.pos - half;
            b.pos = b.pos + half;
        }
    }
}

bool isInvulnerable(const MatchState& m, const MatchSetup& s, int i) {
    const FighterState& f = m.f[i];
    if (f.out) return true;
    if (f.state == FState::Dodge) return f.stateFrame >= 1 && f.stateFrame <= 10;
    if (f.state == FState::Clash) return true;
    if (f.state != FState::Attack) return false;
    const MoveDef& mv = moveOf(s, i, f.moveId);
    return f.moveFrame >= mv.invulnFirst && f.moveFrame <= mv.invulnLast;
}

void addRealmMeter(FighterState& f, int amount) {
    int v = f.realmMeter + amount;
    f.realmMeter = static_cast<int16_t>(v > kRealmMeterMax ? kRealmMeterMax : v);
}

void applyHit(MatchState& m, const MatchSetup& s, int atk, const PendingHit& h, Ev ev) {
    const int di = h.target;
    FighterState& a = m.f[atk];
    FighterState& d = m.f[di];
    if (d.out) return;
    const MoveDef& mv = *h.move;
    const HitProps& p = mv.hit;
    const TechContext actx{m, s, atk, ev};
    const TechContext dctx{m, s, di, ev};
    const TechniqueLogic* al = logicOf(s, atk);
    const TechniqueLogic* dl = logicOf(s, di);
    const int16_t techIdx = h.moveId >= kTechMoveOffset ? static_cast<int16_t>(h.moveId - kTechMoveOffset) : -1;
    const bool fromCompanion = h.companion >= 0;
    const Vec3 src = fromCompanion ? h.source : a.pos;

    // Wächter-Begleiter fängt Treffer von vorne ab (Grabs gehen durch)
    if (!p.unblockable) {
        int guard = shieldingCompanion(m, s, di, src);
        if (guard >= 0) {
            a.hitstop = 4;
            damageCompanion(m, s, di, guard, p.damage, mv.category, ev);
            d.bond = static_cast<int16_t>(d.bond < kBondMax ? d.bond + 1 : kBondMax);
            emit(ev, m, ImpactKind::Block, di, p.impact, h.point, h.dir, p.knockback, 2);
            return;
        }
    }

    // Core Rules wie ein Auto-Block-Feld
    if (dl && dl->nullifiesIncoming(dctx, mv, h.projectile)) {
        a.hitstop = d.hitstop = 4;
        emit(ev, m, ImpactKind::Nullified, di, p.impact, h.point, h.dir, p.knockback);
        return;
    }

    const bool facingAtk = dot(d.facing, src - d.pos) > Fx{};
    const bool guarding = d.state == FState::Guard || d.state == FState::Blockstun;
    const bool blocked = !p.unblockable && d.grounded && facingAtk && guarding;
    a.hitstop = d.hitstop = p.hitstop;

    if (blocked) {
        const bool parry = d.state == FState::Guard && m.frame - d.guardPressFrame <= s.stats[di].parryWindow &&
                           mv.category != MoveCategory::Apex;
        if (parry) {
            if (!h.projectile && !fromCompanion) { a.state = FState::Hitstun; a.stun = 18; a.moveId = -1; a.vel = {}; }
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
    if (buffed(m, s, atk) || debuffed(m, s, di)) dmg = dmg * (bondLevel(a) >= 3 ? 125 : 115) / 100;
    const bool ceAttack = h.projectile || mv.category == MoveCategory::Skill || mv.category == MoveCategory::Apex;
    if (a.reinforce) dmg = dmg * 120 / 100;
    if (d.reinforce && ceAttack) dmg = dmg * 80 / 100;
    if (mv.category == MoveCategory::Grab && s.loadout[atk].body->has(PassiveStrongGrab)) dmg = dmg * 150 / 100;
    if (fromCompanion) {
        dmg = dmg * kCompanionDamagePct / 100;
        if (h.link) dmg = dmg * 125 / 100;
    }
    if (!h.projectile && !fromCompanion && s.loadout[atk].body->has(PassiveFlankCrit) && dot(d.facing, h.dir) > Fx::ratio(-1, 2))
        dmg = dmg * 130 / 100; // Treffer von Seite/Rücken

    // Nullfunke: Normal perfekt auf das Hitstop-Ende des vorherigen Treffers getimed
    bool zeroSpark = false;
    if (!h.projectile && !fromCompanion && mv.category == MoveCategory::Normal) {
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

    d.lastHitBy = static_cast<int8_t>(atk);

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
    leash(m, s, di); // getroffener Spieler -> seine Begleiter brechen ab
    if (fromCompanion) a.bond = static_cast<int16_t>(a.bond < kBondMax ? a.bond + 1 : kBondMax);
    else a.linkFrame = m.frame + a.hitstop + 1;
    addRealmMeter(a, dmg / 2 * s.stats[atk].realmGainPct / 100 * mods(m, s, atk).realmGainPct / 100);
    addRealmMeter(d, dmg / 4);
    if (!zeroSpark) emit(ev, m, ImpactKind::Hit, atk, p.impact, h.point, h.dir, p.knockback + p.launch);
    if (al && !fromCompanion) al->onHitLanded(actx, techIdx, h.projectile, di);
}

bool hitsFighter(const MatchState& m, int target, Vec3 c, Fx r) {
    // Hurtbox: einfache Kapsel aus 3 Kugeln (Beine, Torso, Kopf)
    static const HitboxDef hurt[3] = {
        {{Fx{}, Fx::ratio(5, 10), Fx{}}, Fx::ratio(45, 100)},
        {{Fx{}, Fx::ratio(12, 10), Fx{}}, Fx::ratio(45, 100)},
        {{Fx{}, Fx::ratio(17, 10), Fx{}}, Fx::ratio(30, 100)},
    };
    const FighterState& f = m.f[target];
    // Grober Vorab-Test, damit 20 Kämpfer billig bleiben
    if (abs(c.x - f.pos.x) > r + Fx::fromInt(1) || abs(c.z - f.pos.z) > r + Fx::fromInt(1)) return false;
    for (const HitboxDef& hu : hurt)
        if (overlap(c, r, f.pos + Vec3{Fx{}, hu.offset.y, Fx{}}, hu.radius)) return true;
    return false;
}

int firstEnemyHit(const MatchState& m, const MatchSetup& s, int atk, Vec3 c, Fx r, bool grab) {
    // Lock-on-Ziel zuerst, dann alle anderen Gegner
    const int t = m.f[atk].target;
    for (int k = -1; k < m.count; ++k) {
        const int j = k < 0 ? t : k;
        if (j < 0 || (k >= 0 && j == t) || !isEnemy(m, atk, j) || isInvulnerable(m, s, j)) continue;
        if (grab && (!m.f[j].grounded || m.f[j].state == FState::Hitstun)) continue;
        if (hitsFighter(m, j, c, r)) return j;
    }
    return -1;
}

void moveToward(Vec3& p, Vec3 target, Fx speed) {
    Vec3 d = target - p;
    Fx len = length(d);
    if (len <= speed || len.raw == 0) { p = target; return; }
    p += Vec3{d.x / len, d.y / len, d.z / len} * speed;
}

struct MeleeCheck {
    PendingHit hit;
    bool active = false; // aktiver Hitbox-Frame
    Vec3 center{};
    Fx radius{};
};

MeleeCheck checkMelee(const MatchState& m, const MatchSetup& s, int atk) {
    const FighterState& a = m.f[atk];
    if (a.out || a.state != FState::Attack || a.moveHasHit) return {};
    const MoveDef& mv = moveOf(s, atk, a.moveId);
    if (!mv.isActiveFrame(a.moveFrame) || mv.hitboxes.empty()) return {};
    MatchState& mm = const_cast<MatchState&>(m);
    const TechContext actx{mm, s, atk, nullptr};
    const bool base = a.moveId < kTechMoveOffset;
    MeleeCheck out;
    out.active = true;
    for (const HitboxDef& hb : mv.hitboxes) {
        Vec3 off = hb.offset;
        if (base) off.z = off.z * s.stats[atk].reach; // Range-Stat des Bodys
        Vec3 c = actx.localToWorld(off);
        out.center = c;
        out.radius = hb.radius;
        const int target = firstEnemyHit(m, s, atk, c, hb.radius, mv.category == MoveCategory::Grab);
        if (target >= 0) {
            out.hit = {true, &mv, a.moveId, false, c, a.facing};
            out.hit.target = static_cast<int8_t>(target);
            return out;
        }
    }
    return out;
}

void updateProjectiles(MatchState& m, const MatchSetup& s, Ev ev) {
    const Fx limit = s.stage.halfExtent * Fx::fromInt(3);
    for (Projectile& p : m.proj) {
        if (!p.alive) continue;
        if (p.armDelay > 0) { --p.armDelay; continue; }
        p.pos += p.vel;
        const bool fromCompanion = p.source > 0;
        const MoveDef& mv = fromCompanion ? companionMove(s, p.owner, p.source - 1, p.moveIndex)
                                          : s.loadout[p.owner].technique->moves[p.moveIndex];
        if (projectileHitsCompanion(m, s, p, mv, ev)) { p.alive = false; continue; }
        const int target = firstEnemyHit(m, s, p.owner, p.pos, p.radius, false);
        if (target >= 0) {
            PendingHit h{true, &mv, fromCompanion ? int16_t(-1) : static_cast<int16_t>(p.moveIndex + kTechMoveOffset),
                         true, p.pos, normalizeXZ(p.vel, m.f[p.owner].facing)};
            h.target = static_cast<int8_t>(target);
            if (p.vel == Vec3{}) h.dir = normalizeXZ(m.f[target].pos - p.pos, m.f[p.owner].facing);
            if (fromCompanion) { h.companion = static_cast<int8_t>(p.source - 1); h.source = p.pos - h.dir; }
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

void resolveClash(MatchState& m, const MatchSetup& s, int a, int b, Ev ev) {
    // a = Verteidiger (hatte den Realm), b = Herausforderer. Gleichstand: Verteidiger hält.
    FighterState& fa = m.f[a];
    FighterState& fb = m.f[b];
    const int winner = fb.realm.clashCount > fa.realm.clashCount ? b : a;
    fa.realm = RealmState{};
    fb.realm = RealmState{};
    fa.state = fb.state = FState::Idle;
    FighterState& w = m.f[winner];
    w.realm.timer = kRealmFrames;
    w.realm.open = mods(m, s, winner).openRealm;
    w.realm.center = w.pos;
    emit(ev, m, ImpactKind::RealmOpen, winner, ImpactClass::Crushing, w.pos, {}, Fx::fromInt(1), 2);
}

void tickRealms(MatchState& m, const MatchSetup& s, Ev ev) {
    for (int o = 0; o < m.count; ++o) {
        FighterState& of = m.f[o];
        RealmState& r = of.realm;

        if (r.clashTimer > 0) {
            const int other = r.clashWith;
            if (other < o && m.f[other].realm.clashTimer > 0) continue; // Paar wird beim kleineren Index behandelt
            --r.clashTimer;
            if (m.f[other].realm.clashTimer > 0) --m.f[other].realm.clashTimer;
            if (r.clashTimer == 0) {
                const bool oDefends = r.timer > 0;
                resolveClash(m, s, oDefends ? o : other, oDefends ? other : o, ev);
            }
            continue;
        }
        if (r.timer <= 0) continue;

        const int elapsed = kRealmFrames - r.timer;
        const RealmClass cls = realmClassOf(s, o);
        const bool bond3 = bondLevel(of) >= 3; // Begleiter wirkt im Realm mit
        realmHitsCompanions(m, s, o, cls, bondLevel(of), ev); // Begleiter sind NICHT realm-immun
        for (int t = 0; t < m.count; ++t) {
            if (!realmAffects(m, s, o, t)) continue;
            FighterState& tf = m.f[t];
            if (cls == RealmClass::Damage && elapsed % 30 == 0) {
                tf.health = static_cast<int16_t>(tf.health - (bond3 ? 25 : 20));
                tf.lastHitBy = static_cast<int8_t>(o);
                emit(ev, m, ImpactKind::SureHit, o, ImpactClass::Medium, tf.pos, {}, Fx{}, 0);
            } else if (cls == RealmClass::Lock && elapsed % 200 == 0 && tf.state != FState::Attack) {
                tf.health = static_cast<int16_t>(tf.health - 5);
                tf.lastHitBy = static_cast<int8_t>(o);
                tf.state = FState::Locked;
                tf.lock = static_cast<int16_t>(bond3 ? 120 : 90);
                tf.vel = {};
                emit(ev, m, ImpactKind::SureHit, o, ImpactClass::Heavy, tf.pos, {}, Fx{}, 1);
            }
        }
        if (const TechniqueLogic* l = logicOf(s, o)) l->onRealmTick(TechContext{m, s, o, ev});
        if (--r.timer <= 0) {
            emit(ev, m, ImpactKind::RealmEnd, o, ImpactClass::Medium, of.pos, {}, Fx{});
            r = RealmState{};
        }
    }
}

// ---- Spawn, K.O., Respawn, Sieg
Vec3 spawnPoint(const MatchSetup& s, int i) {
    switch (s.mode) {
        case GameMode::Duel:
            return {Fx{}, Fx{}, Fx::fromInt(i == 0 ? -3 : 3)};
        case GameMode::Teams: {
            int slot = 0;
            for (int j = 0; j < i; ++j) slot += s.loadout[j].team == s.loadout[i].team;
            const Fx x = Fx::ratio(slot % 2 == 0 ? -15 : 15, 10) * Fx::fromInt(1 + slot / 2);
            return {x, Fx{}, Fx::fromInt(s.loadout[i].team == 0 ? -4 : 4)};
        }
        case GameMode::Brawl: {
            // Gleichmäßig auf einem quadratischen Ring verteilt (ohne Trigonometrie -> deterministisch)
            const Fx r = s.stage.halfExtent * Fx::ratio(6, 10);
            const int perSide = (s.count + 3) / 4;
            const int side = i % 4, k = i / 4;
            const Fx t = -r + (r + r) * Fx::ratio(k + 1, perSide + 1);
            switch (side) {
                case 0: return {t, Fx{}, -r};
                case 1: return {r, Fx{}, t};
                case 2: return {-t, Fx{}, r};
                default: return {-r, Fx{}, -t};
            }
        }
    }
    return {};
}

void resetFighter(MatchState& m, const MatchSetup& s, int i, bool matchStart) {
    FighterState& f = m.f[i];
    const PactModifiers& pm = s.pact[i].tier == PactTier::None ? kNeutralPact : s.pact[i].mods;
    const FighterState keep = f;
    f = FighterState{};
    f.team = s.loadout[i].team;
    f.input = keep.input;
    if (!matchStart) {
        f.kos = keep.kos;
        f.deaths = keep.deaths;
        f.lostMask = keep.lostMask;
        f.apexUsed = keep.apexUsed;
        f.pactTimer = keep.pactTimer;
    } else {
        f.pactTimer = pm.durationFrames;
    }
    f.health = static_cast<int16_t>(s.stats[i].maxHealth * pm.maxHealthPct / 100);
    f.ce = s.stats[i].ceMax * pm.cePoolPct / 100;
    f.pos = spawnPoint(s, i);
    f.facing = normalizeXZ(Vec3{} - f.pos, Vec3{Fx{}, Fx{}, Fx::fromInt(i % 2 == 0 ? 1 : -1)});
    initCompanions(m, s, i);
}

void handleKnockouts(MatchState& m, const MatchSetup& s, Ev ev) {
    const ModeRules rules = rulesFor(s.mode);
    for (int i = 0; i < m.count; ++i) {
        FighterState& f = m.f[i];
        if (f.out) {
            if (f.respawn > 0 && --f.respawn == 0) {
                resetFighter(m, s, i, false);
                emit(ev, m, ImpactKind::Respawn, i, ImpactClass::Light, f.pos, {}, Fx{});
            }
            continue;
        }
        if (f.health > 0) continue;
        f.out = true;
        f.state = FState::Knockdown;
        f.vel = {};
        f.realm = RealmState{};
        ++f.deaths;
        if (f.lastHitBy >= 0 && f.lastHitBy != i) ++m.f[f.lastHitBy].kos;
        for (CompanionState& c : f.comp) c = CompanionState{};
        f.respawn = rules.respawnFrames > 0 ? rules.respawnFrames : 0;
        emit(ev, m, ImpactKind::KO, i, ImpactClass::Crushing, f.pos, {}, Fx{}, f.lastHitBy);
    }
}

void checkWinner(MatchState& m, const MatchSetup& s) {
    const ModeRules rules = rulesFor(s.mode);
    // Elimination: genau ein Team lebt noch
    if (rules.respawnFrames < 0) {
        int aliveTeam = -1;
        bool several = false;
        for (int i = 0; i < m.count; ++i) {
            if (m.f[i].out) continue;
            if (aliveTeam < 0) aliveTeam = m.f[i].team;
            else if (m.f[i].team != aliveTeam) several = true;
        }
        if (aliveTeam < 0) m.winner = kDraw;
        else if (!several) m.winner = static_cast<uint8_t>(aliveTeam);
        return;
    }
    // Brawl: K.O.-Limit oder Zeitlimit -> meiste K.O.s
    int best = -1;
    bool tie = false;
    for (int i = 0; i < m.count; ++i) {
        if (best < 0 || m.f[i].kos > m.f[best].kos) { best = i; tie = false; }
        else if (m.f[i].kos == m.f[best].kos) tie = true;
    }
    const bool limitReached = rules.koLimit > 0 && m.f[best].kos >= rules.koLimit;
    const bool timeUp = rules.timeLimitFrames > 0 && m.frame + 1 >= rules.timeLimitFrames;
    if (limitReached) m.winner = m.f[best].team;
    else if (timeUp) m.winner = tie ? kDraw : m.f[best].team;
}

} // namespace detail

using namespace detail;

int bondLevel(const FighterState& f) { return 1 + (f.bond >= 3) + (f.bond >= 6); }

MatchState createMatch(const MatchSetup& s) {
    MatchState m{};
    m.count = static_cast<uint8_t>(s.count);
    for (int i = 0; i < s.count; ++i) resetFighter(m, s, i, true);
    for (int i = 0; i < s.count; ++i) m.f[i].target = static_cast<int8_t>(nearestEnemy(m, i));
    for (WallState& w : m.walls) w.hp = s.stage.wallHp;
    return m;
}

void step(MatchState& m, const MatchSetup& s, const FrameInput inputs[], EventSink* ev) {
    if (m.winner != kNoWinner) return;
    const int n = m.count;
    for (int i = 0; i < n; ++i)
        m.f[i].input.push(s.loadout[i].bot ? botInput(m, s, i) : inputs[i]);

    bool frozen[kMaxFighters];
    for (int i = 0; i < n; ++i) {
        frozen[i] = m.f[i].out || m.f[i].hitstop > 0;
        if (m.f[i].hitstop > 0) --m.f[i].hitstop;
    }

    for (int i = 0; i < n; ++i) if (!m.f[i].out) tickResources(m, s, i, ev);
    for (int i = 0; i < n; ++i) if (!frozen[i]) updateFighter(m, s, i, ev);
    for (int i = 0; i < n; ++i) if (!frozen[i]) physics(m, s, i, ev);
    pushApart(m, s);
    for (int i = 0; i < n; ++i) if (!m.f[i].out) tickCompanions(m, s, i, ev);

    for (int i = 0; i < n; ++i)
        if (!m.f[i].out)
            if (const TechniqueLogic* l = logicOf(s, i)) l->onTick(TechContext{m, s, i, ev});

    // Treffer aller Kämpfer erst sammeln, dann anwenden -> echte Trades
    MeleeCheck hits[kMaxFighters];
    for (int i = 0; i < n; ++i) if (!frozen[i]) hits[i] = checkMelee(m, s, i);
    for (int i = 0; i < n; ++i) {
        if (hits[i].hit.valid) {
            m.f[i].moveHasHit = true;
            applyHit(m, s, i, hits[i].hit, ev);
        } else if (hits[i].active &&
                   meleeHitsCompanion(m, s, i, moveOf(s, i, m.f[i].moveId), hits[i].center, hits[i].radius, ev)) {
            m.f[i].moveHasHit = true; // Begleiter getroffen statt Spieler
        }
    }
    for (int i = 0; i < n; ++i) if (!m.f[i].out) companionAttacks(m, s, i, ev);

    updateProjectiles(m, s, ev);
    tickRealms(m, s, ev);
    handleKnockouts(m, s, ev);
    checkWinner(m, s);
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
    for (const FrameInput& in : hist_) x.add(static_cast<uint64_t>(in.dir | (in.buttons << 8) | (static_cast<uint64_t>(in.consumed) << 24)));
    return x.h;
}

uint64_t hashState(const MatchState& m) {
    Hasher x;
    x.addInt(m.frame); x.addInt(m.count); x.addInt(m.winner);
    for (int i = 0; i < m.count; ++i) {
        const FighterState& f = m.f[i];
        x.add(f.pos); x.add(f.vel); x.add(f.facing);
        x.addInt(f.ce); x.addInt(f.pressFrame); x.addInt(f.linkFrame); x.addInt(f.guardPressFrame); x.addInt(f.pactTimer);
        x.addInt(f.health); x.addInt(f.realmMeter); x.addInt(f.stateFrame); x.addInt(f.moveId); x.addInt(f.moveFrame);
        x.addInt(f.stun); x.addInt(f.hitstop); x.addInt(f.comboTaken); x.addInt(f.burnout); x.addInt(f.lock);
        x.addInt(f.healed);
        for (int16_t c : f.cooldown) x.addInt(c);
        x.addInt(static_cast<int>(f.state)); x.addInt(f.armorLeft);
        x.addInt(f.moveHasHit | (f.grounded << 1) | (f.launched << 2) | (f.reinforce << 3) | (f.apexUsed << 4) |
                 (f.manualTarget << 5) | (f.out << 6));
        x.h = f.input.hash(x.h);
        for (int32_t t : f.tech) x.addInt(t);
        x.addInt(f.lostMask); x.addInt(f.bond);
        x.addInt(f.team); x.addInt(f.target); x.addInt(f.respawn); x.addInt(f.kos); x.addInt(f.deaths); x.addInt(f.lastHitBy);
        x.addInt(f.realm.timer); x.addInt(f.realm.open); x.add(f.realm.center); x.addInt(f.realm.clashWith);
        x.addInt(f.realm.clashTimer); x.addInt(f.realm.clashCount);
        for (const CompanionState& c : f.comp) {
            x.addInt(c.roster); x.addInt(c.action); x.addInt(c.link | (c.hasHit << 1)); x.addInt(c.hp);
            x.addInt(c.actionFrame); x.addInt(c.cooldown); x.addInt(c.down); x.addInt(c.lock); x.addInt(c.expires);
            x.addInt(c.since); x.addInt(c.adapted); x.add(c.pos); x.add(c.facing);
        }
    }
    for (const Projectile& p : m.proj) {
        x.addInt(p.alive);
        if (!p.alive) continue;
        x.addInt(p.owner); x.addInt(p.source); x.addInt(p.moveIndex); x.addInt(p.life); x.addInt(p.armDelay);
        x.add(p.pos); x.add(p.vel); x.add(p.radius);
    }
    for (const WallState& w : m.walls) { x.addInt(w.hp); x.addInt(w.broken); }
    return x.h;
}

} // namespace fc
