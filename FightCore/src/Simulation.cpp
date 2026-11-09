#include "fightcore/Simulation.h"

#include "Internal.h"

namespace fc {

namespace {

std::vector<MoveDef> buildBaseMoves() {
    const uint8_t intoSpecial = catBit(MoveCategory::Skill) | catBit(MoveCategory::MaxOutput) | catBit(MoveCategory::Realm);
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
        m.name = "CE Burst"; m.category = MoveCategory::Burst; m.startup = 1; m.active = 4; m.recovery = 30;
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

const OathModifiers kNeutralOath{};

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
            r.map = &maps::shrineWard();
            break;
        case GameMode::Teams:
            r.fighters = 4;
            r.teamCount = 2;
            r.map = &maps::harborDistrict();
            break;
        case GameMode::Brawl:
            r.fighters = kMaxFighters;
            r.respawnFrames = 300;
            r.objective = Objective::None;
            r.cursedObjectSlots = 2;
            r.map = &maps::oldCapital();
            break;
        case GameMode::Custom:
            r.fighters = kMaxFighters;
            r.teamCount = 2;
            r.respawnFrames = 300;
            r.timeLimitFrames = 10 * 60 * 60;
            r.scoreLimit = 50;
            r.objective = Objective::Deathmatch;
            r.map = &maps::oldCapital();
            r.cursedObjectSlots = 3;
            break;
    }
    return r;
}

const MapDef& mapOf(const MatchSetup& s) { return *s.rules.map; }

void MatchSetup::finalize() {
    if (!rules.map) rules = rulesFor(mode);
    if (rules.teamCount == 1) rules.teamCount = 0;
    if (count > rules.fighters) count = rules.fighters;
    if (count > kMaxFighters) count = kMaxFighters;
    if (count < 2) count = 2;
    for (int i = 0; i < count; ++i) {
        if (rules.teamCount == 0) loadout[i].team = static_cast<uint8_t>(i);
        else if (loadout[i].team >= rules.teamCount) loadout[i].team = static_cast<uint8_t>(loadout[i].team % rules.teamCount);
        stats[i] = deriveStats(*loadout[i].body);
        oath[i] = evaluateOath(loadout[i].oath, *loadout[i].body, loadout[i].technique, loadout[i].profile);

        // only as many cursed objects as the mode allows
        items[i] = ItemModifiers{};
        const int slots = rules.cursedObjectSlots < kMaxCursedObjects ? rules.cursedObjectSlots : kMaxCursedObjects;
        for (int k = 0; k < slots; ++k)
            if (loadout[i].cursed[k]) items[i].add(*loadout[i].cursed[k]);
        const ItemModifiers& it = items[i];
        BodyStats& st = stats[i];
        st.maxHealth = static_cast<int16_t>(st.maxHealth * it.pct(Effect::MaxHealthPct) / 100);
        st.ceMax = st.ceMax * it.pct(Effect::CeMaxPct) / 100;
        st.ceRegen = st.ceRegen * it.pct(Effect::CeRegenPct) / 100;
        st.damagePct = static_cast<int16_t>(st.damagePct * it.pct(Effect::DamagePct) / 100);
        st.cooldownPct = static_cast<int16_t>(st.cooldownPct * it.pct(Effect::CooldownPct) / 100);
        st.realmGainPct = static_cast<int16_t>(st.realmGainPct * it.pct(Effect::RealmGainPct) / 100);
        st.blackSparkWindow = static_cast<int16_t>(st.blackSparkWindow + it[Effect::BlackSparkWindow]);
        st.parryWindow = static_cast<int16_t>(st.parryWindow + it[Effect::ParryWindow]);
        const Fx speed = Fx::ratio(it.pct(Effect::SpeedPct), 100);
        st.walkSpeed = st.walkSpeed * speed;
        st.backSpeed = st.backSpeed * speed;
        st.sideSpeed = st.sideSpeed * speed;
        st.dashSpeed = st.dashSpeed * speed;
    }
    detail::buildRoster(*this);
}

bool isEnemy(const MatchState& m, int a, int b) { return a != b && m.f[a].team != m.f[b].team; }

bool canHurt(const MatchState& m, const MatchSetup& s, int attacker, int victim) {
    return attacker != victim && (m.f[attacker].team != m.f[victim].team || s.rules.friendlyFire);
}

bool carriesObjective(const MatchState& m, int i) {
    if (m.objective.ballCarrier == i) return true;
    for (const FlagState& f : m.objective.flags)
        if (f.carrier == i) return true;
    return false;
}

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

int TechContext::spawnBeam(Vec3 start, Vec3 dir, Fx length, Fx radius, int16_t life, int16_t moveIndex, int16_t armDelay) const {
    const int i = spawnProjectile(start, normalizeXZ(dir, m.f[self].facing), radius, life, moveIndex, armDelay);
    if (i >= 0) {
        m.proj[i].beam = true;
        m.proj[i].length = length;
    }
    return i;
}

void TechContext::strike(int target, int16_t moveIndex) const {
    if (target < 0 || target == self || m.f[target].out) return;
    const MoveDef& mv = setup.loadout[self].technique->moves[moveIndex];
    detail::PendingHit h{true, &mv, static_cast<int16_t>(moveIndex + kTechMoveOffset), false, m.f[target].pos,
                         normalizeXZ(m.f[target].pos - m.f[self].pos, m.f[self].facing)};
    h.target = static_cast<int8_t>(target);
    detail::applyHit(m, setup, self, h, events);
}

void TechContext::burnOut(int16_t frames) const {
    FighterState& f = m.f[self];
    f.ce = 0;
    f.reinforce = false;
    if (f.burnout < frames) f.burnout = frames;
    emit(ImpactKind::Burnout, f.pos, {}, Fx{}, ImpactClass::Light);
}

bool TechContext::isEnemy(int other) const { return fc::isEnemy(m, self, other); }

void TechContext::emit(ImpactKind kind, Vec3 pos, Vec3 dir, Fx force, ImpactClass strength, int16_t tag) const {
    if (events) events->push_back({m.frame, kind, static_cast<uint8_t>(self), strength, tag, pos, dir, force});
}

void TechContext::damage(int target, int16_t amount) const {
    FighterState& t = m.f[target];
    if (t.out) return;
    t.health = static_cast<int16_t>(t.health - amount);
    t.lastHitBy = static_cast<int8_t>(self);
}

namespace detail {

void emit(Ev ev, const MatchState& m, ImpactKind k, int src, ImpactClass s, Vec3 pos, Vec3 dir, Fx force, int16_t tag) {
    if (ev) ev->push_back({m.frame, k, static_cast<uint8_t>(src), s, tag, pos, dir, force});
}

const OathModifiers& mods(const MatchState& m, const MatchSetup& s, int i) {
    if (s.oath[i].tier == OathTier::None || m.f[i].oathTimer == 0) return kNeutralOath;
    return s.oath[i].mods;
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

bool inRealmArea(const MatchState& m, int owner, Vec3 p) {
    const FighterState& o = m.f[owner];
    const Vec3 center = o.realm.open ? o.pos : o.realm.center;
    const Fx radius = Fx::fromInt(o.realm.open ? kOpenRealmRadius : kRealmRadius);
    return distXZ(center, p) <= radius;
}

bool realmAffects(const MatchState& m, const MatchSetup& s, int owner, int target) {
    const FighterState& t = m.f[target];
    if (!m.f[owner].realm.active() || !isEnemy(m, owner, target) || t.out) return false;
    if (s.stats[target].guaranteedHitImmune || t.state == FState::SimpleRealm) return false;
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
    if (s.loadout[i].technique->ceFree) return 0;
    int32_t c = s.loadout[i].technique->skills[slot].ceCost * 100;
    if (debuffed(m, s, i)) c = c * 150 / 100;
    if (buffed(m, s, i)) c = c / 2;
    return c;
}

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

// Numpad direction of a world space vector relative to the facing
uint8_t numpadFrom(Vec3 facing, Vec3 v) {
    const Fx len = length(v);
    if (len.raw == 0) return 5;
    const Vec3 right{facing.z, Fx{}, -facing.x};
    const Fx fwd = dot(v, facing), side = dot(v, right), cut = len * Fx::ratio(38, 100);
    const int vert = fwd > cut ? 1 : (fwd < -cut ? -1 : 0);
    const int horiz = side > cut ? 1 : (side < -cut ? -1 : 0);
    static const uint8_t numpad[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    return numpad[vert + 1][horiz + 1];
}

Vec3 worldMove(const FrameInput& in) { return {Fx::fromInt(in.moveX), Fx{}, Fx::fromInt(in.moveZ)}; }

// What L3 locks on to: something in front of you beats something slightly closer behind you
int lockCandidate(const MatchState& m, int i) {
    const FighterState& f = m.f[i];
    int best = -1;
    Fx bestScore{};
    for (int j = 0; j < m.count; ++j) {
        if (!isEnemy(m, i, j) || m.f[j].out) continue;
        const Vec3 d{m.f[j].pos.x - f.pos.x, Fx{}, m.f[j].pos.z - f.pos.z};
        const Fx dist = length(d);
        if (dist > Fx::fromInt(30)) continue;
        const Fx score = dist - dot(normalizeXZ(d, f.facing), f.facing) * Fx::fromInt(4);
        if (best < 0 || score < bestScore) { best = j; bestScore = score; }
    }
    return best;
}

void updateTarget(MatchState& m, int i) {
    FighterState& f = m.f[i];
    if (f.input.pressedWithin(BtnTarget, 3)) {
        f.input.consume(BtnTarget, 3);
        if (f.lockOn) {
            f.lockOn = false;
            f.manualTarget = false;
        } else if (const int t = lockCandidate(m, i); t >= 0) {
            f.target = static_cast<int8_t>(t);
            f.lockOn = true;
            f.manualTarget = true;
        }
    }
    if (f.lockOn && f.input.pressedWithin(BtnTargetNext, 3)) {
        f.input.consume(BtnTargetNext, 3);
        for (int k = 1; k <= m.count; ++k) {
            const int j = (f.target + k + m.count) % m.count;
            if (isEnemy(m, i, j) && !m.f[j].out) { f.target = static_cast<int8_t>(j); f.manualTarget = true; break; }
        }
    }
    const bool valid = f.target >= 0 && isEnemy(m, i, f.target) && !m.f[f.target].out;
    if (!valid) {
        f.target = static_cast<int8_t>(nearestEnemy(m, i));
        f.manualTarget = false;
    } else if (!f.manualTarget && (m.frame + i) % 20 == 0) {
        f.target = static_cast<int8_t>(nearestEnemy(m, i));
    }
}

struct Cmd {
    int16_t move = -1;
    int8_t skill = -1;
    int32_t ceCost = 0;
    bool maxOutput = false, realm = false, simpleRealm = false;
    int age = 0;
    uint16_t button = 0;
};

Cmd resolveCommand(MatchState& m, const MatchSetup& s, int i, uint8_t allowed) {
    FighterState& f = m.f[i];
    const TechniqueDef* T = s.loadout[i].technique;
    const OathModifiers& pm = mods(m, s, i);
    auto age = [&](uint16_t b) { return f.input.pressAge(b, kBufferWindow); };
    // Modifiers count if they were held when the button went down, not when we read the buffer.
    auto heldAt = [&](int a, uint16_t b) { return (f.input.ago(a).buttons & b) != 0; };
    auto ok = [&](Cmd c, uint16_t btn, int a) { c.button = btn; c.age = a; f.input.consume(btn, kBufferWindow); return c; };
    int a;

    const bool carrying = carriesObjective(m, i);
    if ((a = age(BtnUlt)) >= 0) {
        if (carrying) return {};
        const bool mod = heldAt(a, BtnMod);
        if (!mod && enemyRealmAround(m, i) >= 0) {
            Cmd c; c.simpleRealm = true; return ok(c, BtnUlt, a);
        }
        if (mod) {
            bool can = T && !pm.noRealm && f.realmMeter >= kRealmMeterMax && f.realm.timer == 0 &&
                       f.realm.clashWith < 0 && (allowed & catBit(MoveCategory::Realm));
            if (can) { Cmd c; c.move = kRealmCast; c.realm = true; return ok(c, BtnUlt, a); }
        } else {
            const int32_t cost = T && T->ceFree ? 0 : ceMaxOf(s, m, i) / 2;
            bool can = T && T->maxOutputMove >= 0 && !f.maxOutputUsed && !pm.noMaxOutput && f.burnout == 0 &&
                       (T->ceFree || (cost > 0 && f.ce >= cost)) && (allowed & catBit(MoveCategory::MaxOutput)) &&
                       (!T->logic || T->logic->canUseSkill(TechContext{m, s, i, nullptr}, 3));
            if (can) {
                Cmd c; c.move = static_cast<int16_t>(T->maxOutputMove + kTechMoveOffset); c.maxOutput = true; c.ceCost = cost;
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
        // holding Mod never falls through to a normal
        if (!T || carrying || f.burnout > 0 || f.cooldown[slot] > 0 || (pm.lockedSkills & (1 << slot))) return {};
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
        Cmd c; c.move = f.input.ago(a).dir <= 3 ? kLauncher : kHeavy; return ok(c, BtnHeavy, a);  // back + Heavy launches
    }
    if ((a = age(BtnLight)) >= 0) { Cmd c; c.move = kLight; return ok(c, BtnLight, a); }
    return {};
}

void startMove(MatchState& m, const MatchSetup& s, int i, const Cmd& c, Ev ev) {
    FighterState& f = m.f[i];
    if (!f.lockOn) {
        // soft aim: snap to an enemy that's close and roughly in front
        const int t = nearestEnemy(m, i);
        if (t >= 0) {
            const Vec3 d = m.f[t].pos - f.pos;
            if (distXZ(m.f[t].pos, f.pos) < Fx::fromInt(4) && dot(normalizeXZ(d, f.facing), f.facing) > Fx{})
                f.facing = normalizeXZ(d, f.facing);
        }
    }
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
    if (c.maxOutput) {
        f.maxOutputUsed = true;
        if (mods(m, s, i).maxOutputNoStartup && mv.startup > 1) f.moveFrame = static_cast<int16_t>(mv.startup - 1);
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
    // keeping a closed realm up slows you down
    const RealmState& r = m.f[i].realm;
    if (r.active() && !r.open) p /= 2;
    if (const TechniqueLogic* l = logicOf(s, i)) p = p * l->speedPct(TechContext{const_cast<MatchState&>(m), s, i, nullptr}) / 100;
    return p;
}

void openRealm(MatchState& m, const MatchSetup& s, int i, Ev ev) {
    FighterState& f = m.f[i];
    const int defender = enemyRealmAround(m, i);
    if (defender >= 0 && m.f[defender].realm.clashWith < 0) {
        // casting inside an enemy realm starts a clash
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
    const OathModifiers& pm = mods(m, s, i);

    for (int16_t& cd : f.cooldown) {
        if (cd > 0) --cd;
        if (cd > 0 && buffed(m, s, i)) --cd;
    }
    if (f.burnout > 0) --f.burnout;
    if (f.oathTimer > 0) --f.oathTimer;

    const int32_t max = ceMaxOf(s, m, i);
    if (max <= 0) return;
    int32_t regen = 0;
    if (f.state != FState::Heal && !debuffed(m, s, i)) regen = st.ceRegen * pm.ceRegenPct / 100;
    regen -= pm.ceLeak;
    if (f.reinforce) regen -= 25;
    if (f.state == FState::SimpleRealm) regen -= st.simpleRealmDrain;
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
    const OathModifiers& pm = mods(m, s, i);
    const FrameInput& in = f.input.current();
    const uint8_t all = 0xFF;

    if (f.input.pressAge(BtnGuard, 1) == 0) f.guardPressFrame = m.frame;
    updateTarget(m, i);

    const bool freeMove = !f.lockOn && in.hasWorldMove();
    if (f.lockOn && f.target >= 0 && f.state != FState::Attack && f.state != FState::Hitstun && f.state != FState::Locked &&
        f.state != FState::Clash)
        f.facing = normalizeXZ(m.f[f.target].pos - f.pos, f.facing);
    else if (freeMove && (f.state == FState::Idle || f.state == FState::Walk || f.state == FState::Guard))
        f.facing = normalizeXZ(worldMove(in), f.facing); // free movement turns you where you're going

    // CE burst works out of hitstun
    if (f.state == FState::Hitstun || f.state == FState::Blockstun) {
        Cmd c = resolveCommand(m, s, i, catBit(MoveCategory::Burst));
        if (c.move >= 0) {
            startMove(m, s, i, c, ev);
            emit(ev, m, ImpactKind::Burst, i, ImpactClass::Heavy, f.pos, {}, Fx::fromInt(1));
            return;
        }
    }

    if (f.state != FState::Hitstun && f.state != FState::Blockstun && f.state != FState::Locked &&
        f.state != FState::Clash && f.state != FState::Knockdown)
        tryAssistCommand(m, s, i, ev);

    switch (f.state) {
        case FState::Idle:
        case FState::Walk:
        case FState::Guard: {
            Cmd c = resolveCommand(m, s, i, all);
            if (c.simpleRealm) { f.state = FState::SimpleRealm; f.vel = {}; break; }
            if (c.move >= 0) { startMove(m, s, i, c, ev); f.vel = {}; break; }
            const bool mod = f.input.held(BtnMod);
            if (mod && f.input.held(BtnGuard) && f.ce > 0 && f.health < st.maxHealth) {
                f.state = FState::Heal; f.vel = {}; break;
            }
            if (f.input.pressedWithin(BtnDash, 3)) {
                f.input.consume(BtnDash, 3);
                if (mod) {
                    f.reinforce = !f.reinforce && f.ce > 0 && f.burnout == 0;
                    break;
                }
                const bool dodge = f.input.held(BtnGuard);
                Vec3 dir = in.dir == 2 ? f.facing * Fx::fromInt(-1) : f.facing;
                if (in.dir == 4 || in.dir == 6) dir = normalizeXZ(dirToVelocity(f, st, in.dir, 100), f.facing);
                if (freeMove) dir = f.facing;
                f.vel = dir * (st.dashSpeed * Fx::ratio(speedPctOf(m, s, i), 100));
                f.state = dodge ? FState::Dodge : FState::Dash;
                f.stateFrame = 0;
                emit(ev, m, ImpactKind::DashDust, i, ImpactClass::Medium, f.pos, dir, st.dashSpeed);
                break;
            }
            if (f.input.pressedWithin(BtnJump, 3)) {
                f.input.consume(BtnJump, 3);
                Vec3 h = freeMove ? f.facing * (st.walkSpeed * Fx::ratio(speedPctOf(m, s, i), 100))
                                  : dirToVelocity(f, st, in.dir, speedPctOf(m, s, i));
                f.vel = {h.x, st.jumpVelocity, h.z};
                f.state = FState::Air;
                f.grounded = false;
                emit(ev, m, ImpactKind::DashDust, i, ImpactClass::Light, f.pos, {}, st.jumpVelocity);
                break;
            }
            if ((in.buttons & BtnGuard) && !pm.noBlock) { f.state = FState::Guard; f.vel = {}; break; }
            if (freeMove) {
                f.vel = f.facing * (st.walkSpeed * Fx::ratio(speedPctOf(m, s, i), 100));
                f.state = FState::Walk;
            } else {
                f.vel = dirToVelocity(f, st, in.dir, speedPctOf(m, s, i));
                f.state = (in.dir == 5) ? FState::Idle : FState::Walk;
            }
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
                if (mv.category == MoveCategory::Realm) {
                    const TechniqueDef* T = s.loadout[i].technique;
                    if (T && T->replacesRealm) T->logic->onRealmOpen(TechContext{m, s, i, ev});
                    else openRealm(m, s, i, ev);
                }
                else if (f.moveId >= kTechMoveOffset)
                    if (const TechniqueLogic* l = logicOf(s, i))
                        l->onMoveEvent(TechContext{m, s, i, ev}, static_cast<int16_t>(f.moveId - kTechMoveOffset));
                if (f.state != FState::Attack) break;  // a clash took over
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
        case FState::SimpleRealm: {
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

bool insideBox(const ObstacleDef& o, Vec3 p, Fx margin) {
    return abs(p.x - o.center.x) < o.halfX + margin && abs(p.z - o.center.z) < o.halfZ + margin && p.y < o.height;
}

void damageObstacle(MatchState& m, const MatchSetup& s, int index, int amount, int source, Vec3 at, Vec3 normal, Fx force, Ev ev) {
    ObstacleState& o = m.obstacles[index];
    if (o.broken) return;
    o.hp = static_cast<int16_t>(o.hp - amount);
    if (o.hp <= 0) {
        o.broken = true;
        emit(ev, m, ImpactKind::WallBreak, source, ImpactClass::Crushing, at, normal, force, static_cast<int16_t>(index));
    } else {
        emit(ev, m, ImpactKind::WallSplat, source, ImpactClass::Heavy, at, normal, force, static_cast<int16_t>(index));
    }
    (void)s;
}

void collideWithMap(MatchState& m, const MatchSetup& s, int i, Ev ev) {
    FighterState& f = m.f[i];
    const MapDef& map = mapOf(s);
    const Fx body = s.stats[i].pushRadius;
    const bool flung = f.state == FState::Hitstun;

    // The barrier: indestructible, bodies bounce off it
    const Vec3 flat{f.pos.x, Fx{}, f.pos.z};
    const Fx dist = length(flat);
    const Fx limit = map.radius - body;
    if (dist > limit) {
        const Vec3 n = normalizeXZ(flat, Vec3{Fx{}, Fx{}, Fx::fromInt(1)});
        f.pos = Vec3{n.x * limit, f.pos.y, n.z * limit};
        const Fx into = dot(f.vel, n);
        if (into > Fx{}) {
            if (flung && into >= map.impactSpeed) {
                emit(ev, m, ImpactKind::BarrierHit, i, ImpactClass::Heavy, f.pos, n, into);
                f.stun = static_cast<int16_t>(f.stun + 10);
            }
            f.vel = f.vel - n * (into + into * Fx::ratio(1, 2)); // reflect, lose half the speed
        }
    }

    // Buildings / pillars: solid until wrecked
    for (int k = 0; k < static_cast<int>(map.obstacles.size()) && k < kMaxObstacles; ++k) {
        if (m.obstacles[k].broken) continue;
        const ObstacleDef& o = map.obstacles[k];
        if (!insideBox(o, f.pos, body)) continue;
        const Fx dx = f.pos.x - o.center.x, dz = f.pos.z - o.center.z;
        const Fx penX = o.halfX + body - abs(dx), penZ = o.halfZ + body - abs(dz);
        Vec3 n{};
        if (penX < penZ) n.x = dx.raw < 0 ? Fx::fromInt(-1) : Fx::fromInt(1);
        else n.z = dz.raw < 0 ? Fx::fromInt(-1) : Fx::fromInt(1);
        const Fx speed = abs(dot(f.vel, n));
        if (flung && speed >= map.impactSpeed) {
            damageObstacle(m, s, k, (speed * Fx::fromInt(400)).toInt(), i, f.pos, n, speed, ev);
            if (m.obstacles[k].broken) continue; // smashed straight through it
            f.stun = static_cast<int16_t>(f.stun + 10);
        }
        if (penX < penZ) { f.pos.x += n.x * penX; f.vel.x = -f.vel.x * Fx::ratio(1, 2); }
        else { f.pos.z += n.z * penZ; f.vel.z = -f.vel.z * Fx::ratio(1, 2); }
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

    collideWithMap(m, s, i, ev);
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
            if (abs(diff.x) >= minDist || abs(diff.z) >= minDist) continue;
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

    // a shielding companion eats hits from the front
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

    if (dl && !mv.isCounter && dl->nullifiesIncoming(dctx, mv, h.projectile, atk)) {
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
                           mv.category != MoveCategory::MaxOutput;
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
        if (al) al->onBlock(actx, true);
        if (dl) dl->onBlock(dctx, false);
        return;
    }

    int dmg = p.damage;
    int scale = 100 - d.comboTaken * 10;
    dmg = dmg * (scale < 30 ? 30 : scale) / 100;
    dmg = dmg * s.stats[atk].damagePct / 100;
    dmg = dmg * mods(m, s, atk).damagePct / 100;
    if (al) dmg = dmg * al->damageDealtPct(actx, techIdx) / 100;
    if (dl) dmg = dmg * dl->damageTakenPct(dctx) / 100;
    if (buffed(m, s, atk) || debuffed(m, s, di)) dmg = dmg * (bondLevel(a) >= 3 ? 125 : 115) / 100;
    const bool ceAttack = h.projectile || mv.category == MoveCategory::Skill || mv.category == MoveCategory::MaxOutput;
    if (a.reinforce) dmg = dmg * 120 / 100;
    if (d.reinforce && ceAttack) dmg = dmg * 80 / 100;
    if (mv.category == MoveCategory::Grab && s.loadout[atk].body->has(PassiveStrongGrab)) dmg = dmg * 150 / 100;
    const ItemModifiers& ai = s.items[atk];
    const ItemModifiers& di_ = s.items[di];
    if (mv.category == MoveCategory::Normal || mv.category == MoveCategory::Grab) dmg = dmg * ai.pct(Effect::NormalDamagePct) / 100;
    if (!fromCompanion && h.moveId >= kTechMoveOffset) dmg = dmg * ai.pct(Effect::SkillDamagePct) / 100;
    if (ai[Effect::OvertimeDamagePct] && m.frame - a.spawnFrame >= 3600) dmg = dmg * ai.pct(Effect::OvertimeDamagePct) / 100;
    dmg = dmg * di_.pct(Effect::DamageTakenPct) / 100;
    if (ai[Effect::CritChancePct] > 0) {
        uint32_t roll = static_cast<uint32_t>(m.frame) * 2654435761u ^ static_cast<uint32_t>(atk * 97 + di * 13);
        roll ^= roll >> 15;
        if (static_cast<int>(roll % 100) < ai[Effect::CritChancePct]) dmg *= 2;
    }
    if (fromCompanion) {
        dmg = dmg * kCompanionDamagePct / 100 * ai.pct(Effect::CompanionDamagePct) / 100;
        if (h.link) dmg = dmg * 125 / 100;
    }
    if (!h.projectile && !fromCompanion && s.loadout[atk].body->has(PassiveFlankCrit) && dot(d.facing, h.dir) > Fx::ratio(-1, 2))
        dmg = dmg * 130 / 100;  // speedster flank crit

    // Black Spark: a normal pressed right as the previous hit's hitstop ends
    bool blackSpark = false;
    if (!h.projectile && !fromCompanion && mv.category == MoveCategory::Normal) {
        int window = s.stats[atk].blackSparkWindow + mods(m, s, atk).extraBlackSpark;
        int diff = a.pressFrame - a.linkFrame;
        if (diff < 0) diff = -diff;
        if (diff <= window) {
            blackSpark = true;
            dmg = dmg * 250 / 100;
            addRealmMeter(a, 250);
            a.hitstop = d.hitstop = static_cast<int16_t>(p.hitstop + 8);
            emit(ev, m, ImpactKind::BlackSpark, atk, ImpactClass::Crushing, h.point, h.dir, p.knockback);
        }
    }

    d.lastHitBy = static_cast<int8_t>(atk);

    // super armor takes the damage but not the stun
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
    if (ai[Effect::LifestealPct] > 0) {
        const int heal = dmg * ai[Effect::LifestealPct] / 100;
        a.health = static_cast<int16_t>(a.health + heal > s.stats[atk].maxHealth ? s.stats[atk].maxHealth : a.health + heal);
    }
    if (di_[Effect::ThornsPct] > 0 && !h.projectile && !fromCompanion)
        a.health = static_cast<int16_t>(a.health - dmg * di_[Effect::ThornsPct] / 100);
    d.state = FState::Hitstun;
    d.stun = p.hitstun;
    d.moveId = -1;
    d.vel = h.dir * p.knockback + Vec3{Fx{}, p.launch, Fx{}};
    if (p.launch > Fx{}) { d.grounded = false; d.launched = true; }
    ++d.comboTaken;
    leash(m, s, di);  // leash: getting hit cancels your companions
    if (fromCompanion) a.bond = static_cast<int16_t>(a.bond < kBondMax ? a.bond + 1 : kBondMax);
    else a.linkFrame = m.frame + a.hitstop + 1;
    addRealmMeter(a, dmg / 2 * s.stats[atk].realmGainPct / 100 * mods(m, s, atk).realmGainPct / 100);
    addRealmMeter(d, dmg / 4);
    if (!blackSpark) emit(ev, m, ImpactKind::Hit, atk, p.impact, h.point, h.dir, p.knockback + p.launch);
    if (al && !fromCompanion) al->onHitLanded(actx, techIdx, h.projectile, di);
    if (dl) dl->onHitTaken(dctx, atk);
}

bool hitsFighter(const MatchState& m, int target, Vec3 c, Fx r) {
    // legs, torso, head
    static const HitboxDef hurt[3] = {
        {{Fx{}, Fx::ratio(5, 10), Fx{}}, Fx::ratio(45, 100)},
        {{Fx{}, Fx::ratio(12, 10), Fx{}}, Fx::ratio(45, 100)},
        {{Fx{}, Fx::ratio(17, 10), Fx{}}, Fx::ratio(30, 100)},
    };
    const FighterState& f = m.f[target];
    if (abs(c.x - f.pos.x) > r + Fx::fromInt(1) || abs(c.z - f.pos.z) > r + Fx::fromInt(1)) return false;
    for (const HitboxDef& hu : hurt)
        if (overlap(c, r, f.pos + Vec3{Fx{}, hu.offset.y, Fx{}}, hu.radius)) return true;
    return false;
}

int firstEnemyHit(const MatchState& m, const MatchSetup& s, int atk, Vec3 c, Fx r, bool grab) {
    // lock-on target first
    const int t = m.f[atk].target;
    for (int k = -1; k < m.count; ++k) {
        const int j = k < 0 ? t : k;
        if (j < 0 || (k >= 0 && j == t) || !canHurt(m, s, atk, j) || isInvulnerable(m, s, j)) continue;
        if (grab && (!m.f[j].grounded || m.f[j].state == FState::Hitstun)) continue;
        if (hitsFighter(m, j, c, r)) return j;
    }
    return -1;
}

int firstBeamHit(const MatchState& m, const MatchSetup& s, const Projectile& p) {
    static const Fx hurtY[3] = {Fx::ratio(5, 10), Fx::ratio(12, 10), Fx::ratio(17, 10)};
    int best = -1;
    Fx bestT{};
    for (int j = 0; j < m.count; ++j) {
        if (!canHurt(m, s, p.owner, j) || isInvulnerable(m, s, j)) continue;
        for (Fx y : hurtY) {
            const Vec3 c = m.f[j].pos + Vec3{Fx{}, y, Fx{}};
            const Fx t = dot(c - p.pos, p.vel);
            if (t < Fx{} || t > p.length) continue;
            const Vec3 closest = p.pos + p.vel * t;
            if (overlap(closest, p.radius, c, Fx::ratio(45, 100)) && (best < 0 || t < bestT)) { best = j; bestT = t; }
        }
    }
    return best;
}

void moveToward(Vec3& p, Vec3 target, Fx speed) {
    Vec3 d = target - p;
    Fx len = length(d);
    if (len <= speed || len.raw == 0) { p = target; return; }
    p += Vec3{d.x / len, d.y / len, d.z / len} * speed;
}

struct MeleeCheck {
    PendingHit hit;
    bool active = false;
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
        if (base) off.z = off.z * s.stats[atk].reach;
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
    const MapDef& map = mapOf(s);
    for (Projectile& p : m.proj) {
        if (!p.alive) continue;
        if (p.armDelay > 0) { --p.armDelay; continue; }
        if (!p.beam) p.pos += p.vel;
        const bool fromCompanion = p.source > 0;
        const MoveDef& mv = fromCompanion ? companionMove(s, p.owner, p.source - 1, p.moveIndex)
                                          : s.loadout[p.owner].technique->moves[p.moveIndex];
        if (p.beam) {
            const int victim = firstBeamHit(m, s, p);
            if (victim >= 0) {
                PendingHit h{true, &mv, static_cast<int16_t>(p.moveIndex + kTechMoveOffset), true, m.f[victim].pos, p.vel};
                h.target = static_cast<int8_t>(victim);
                applyHit(m, s, p.owner, h, ev);
                p.alive = false;
            } else if (--p.life <= 0) {
                p.alive = false;
            }
            continue;
        }
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
        bool hitScenery = false;
        for (int k = 0; k < static_cast<int>(map.obstacles.size()) && k < kMaxObstacles && !hitScenery; ++k) {
            if (m.obstacles[k].broken || !insideBox(map.obstacles[k], p.pos, p.radius)) continue;
            damageObstacle(m, s, k, mv.hit.damage / 2, p.owner, p.pos, normalizeXZ(p.vel, {}), length(p.vel), ev);
            hitScenery = true;
        }
        const bool outside = length(Vec3{p.pos.x, Fx{}, p.pos.z}) > map.radius;
        if (hitScenery || outside || --p.life <= 0) {
            p.alive = false;
            emit(ev, m, ImpactKind::ProjectileBurst, p.owner, ImpactClass::Medium, p.pos, {}, length(p.vel));
        }
    }
}

void resolveClash(MatchState& m, const MatchSetup& s, int a, int b, Ev ev) {
    // a had the realm, b challenged it. Ties go to a.
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
            if (other < o && m.f[other].realm.clashTimer > 0) continue;  // handled once, by the lower index
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
        const bool bond3 = bondLevel(of) >= 3;
        realmHitsCompanions(m, s, o, cls, bondLevel(of), ev);
        for (int t = 0; t < m.count; ++t) {
            if (!realmAffects(m, s, o, t)) continue;
            FighterState& tf = m.f[t];
            if (cls == RealmClass::Damage && elapsed % 30 == 0) {
                tf.health = static_cast<int16_t>(tf.health - (bond3 ? 25 : 20) * s.items[t].pct(Effect::GuaranteedHitTakenPct) / 100);
                tf.lastHitBy = static_cast<int8_t>(o);
                emit(ev, m, ImpactKind::GuaranteedHit, o, ImpactClass::Medium, tf.pos, {}, Fx{}, 0);
            } else if (cls == RealmClass::Lock && elapsed % 200 == 0 && tf.state != FState::Attack) {
                tf.health = static_cast<int16_t>(tf.health - 5);
                tf.lastHitBy = static_cast<int8_t>(o);
                tf.state = FState::Locked;
                tf.lock = static_cast<int16_t>(bond3 ? 120 : 90);
                tf.vel = {};
                emit(ev, m, ImpactKind::GuaranteedHit, o, ImpactClass::Heavy, tf.pos, {}, Fx{}, 1);
            }
        }
        if (const TechniqueLogic* l = logicOf(s, o)) l->onRealmTick(TechContext{m, s, o, ev});
        if (--r.timer <= 0) {
            emit(ev, m, ImpactKind::RealmEnd, o, ImpactClass::Medium, of.pos, {}, Fx{});
            r = RealmState{};
        }
    }
}

Vec3 teamBase(const MatchSetup& s, int team) {
    // Up to four team bases on the cardinal directions
    const Fx r = mapOf(s).radius * Fx::ratio(7, 10);
    switch (team % 4) {
        case 0: return {Fx{}, Fx{}, -r};
        case 1: return {Fx{}, Fx{}, r};
        case 2: return {-r, Fx{}, Fx{}};
        default: return {r, Fx{}, Fx{}};
    }
}

Vec3 spawnPoint(const MatchSetup& s, int i) {
    if (s.mode == GameMode::Duel) return {Fx{}, Fx{}, Fx::fromInt(i == 0 ? -4 : 4)};
    if (s.rules.teamCount > 0) {
        const int team = s.loadout[i].team;
        int slot = 0;
        for (int j = 0; j < i; ++j) slot += s.loadout[j].team == team;
        const Vec3 base = teamBase(s, team);
        const Vec3 toCenter = normalizeXZ(Vec3{} - base, Vec3{Fx{}, Fx{}, Fx::fromInt(1)});
        const Vec3 side{toCenter.z, Fx{}, -toCenter.x};
        const Fx offset = Fx::ratio(slot % 2 == 0 ? -15 : 15, 10) * Fx::fromInt(1 + slot / 2);
        return base + side * offset + toCenter * Fx::fromInt(slot / 4);
    }
    // Free-for-all: spread over a square ring that fits inside the colony
    const Fx r = mapOf(s).radius * Fx::ratio(5, 10);
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

void resetFighter(MatchState& m, const MatchSetup& s, int i, bool matchStart) {
    FighterState& f = m.f[i];
    const OathModifiers& pm = s.oath[i].tier == OathTier::None ? kNeutralOath : s.oath[i].mods;
    const FighterState keep = f;
    f = FighterState{};
    f.team = s.loadout[i].team;
    f.input = keep.input;
    f.lockOn = s.loadout[i].lockOnAtStart || s.loadout[i].bot;
    f.spawnFrame = m.frame;
    f.realmMeter = static_cast<int16_t>(kRealmMeterMax * s.items[i][Effect::StartRealmMeterPct] / 100);
    if (!matchStart) {
        f.kos = keep.kos;
        f.deaths = keep.deaths;
        f.lostMask = keep.lostMask;
        f.maxOutputUsed = keep.maxOutputUsed;
        f.oathTimer = keep.oathTimer;
    } else {
        f.oathTimer = pm.durationFrames;
    }
    f.health = static_cast<int16_t>(s.stats[i].maxHealth * pm.maxHealthPct / 100);
    f.ce = s.stats[i].ceMax * pm.cePoolPct / 100;
    f.pos = spawnPoint(s, i);
    f.facing = normalizeXZ(Vec3{} - f.pos, Vec3{Fx{}, Fx{}, Fx::fromInt(i % 2 == 0 ? 1 : -1)});
    initCompanions(m, s, i);
}

void handleKnockouts(MatchState& m, const MatchSetup& s, Ev ev) {
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
        const int killer = f.lastHitBy;
        if (killer >= 0 && killer != i) {
            ++m.f[killer].kos;
            FighterState& k = m.f[killer];
            const int heal = s.items[killer][Effect::HealOnKo];
            if (heal > 0 && !k.out) k.health = static_cast<int16_t>(k.health + heal > s.stats[killer].maxHealth ? s.stats[killer].maxHealth : k.health + heal);
            if (s.rules.objective == Objective::Deathmatch && isEnemy(m, killer, i)) ++m.objective.score[m.f[killer].team];
        }
        for (CompanionState& c : f.comp) c = CompanionState{};
        f.respawn = s.rules.respawnFrames > 0 ? s.rules.respawnFrames : 0;
        emit(ev, m, ImpactKind::KO, i, ImpactClass::Crushing, f.pos, {}, Fx{}, static_cast<int16_t>(killer));
    }
}

bool near(Vec3 a, Vec3 b, Fx range) { return distXZ(a, b) <= range; }

void tickFlags(MatchState& m, const MatchSetup& s, Ev ev) {
    const int flags = s.rules.teamCount < kMaxFlags ? s.rules.teamCount : kMaxFlags;
    const Fx reach = Fx::ratio(15, 10);
    for (int t = 0; t < flags; ++t) {
        FlagState& flag = m.objective.flags[t];
        const Vec3 home = teamBase(s, t);
        if (flag.carrier >= 0) {
            const FighterState& c = m.f[flag.carrier];
            if (c.out) {
                emit(ev, m, ImpactKind::FlagDropped, flag.carrier, ImpactClass::Medium, flag.pos, {}, Fx{}, static_cast<int16_t>(t));
                flag.carrier = -1;
                flag.returnTimer = 900;
                continue;
            }
            flag.pos = c.pos + Vec3{Fx{}, Fx::ratio(15, 10), Fx{}};
            // Score by bringing it to your own base while your flag is safe at home
            const int team = c.team;
            if (team < flags && m.objective.flags[team].home && near(c.pos, teamBase(s, team), reach + reach)) {
                ++m.objective.score[team];
                emit(ev, m, ImpactKind::FlagCaptured, flag.carrier, ImpactClass::Crushing, c.pos, {}, Fx{}, static_cast<int16_t>(t));
                flag = FlagState{};
                flag.pos = home;
            }
            continue;
        }
        if (!flag.home && --flag.returnTimer <= 0) {
            flag = FlagState{};
            flag.pos = home;
            emit(ev, m, ImpactKind::FlagReturned, t, ImpactClass::Light, home, {}, Fx{}, static_cast<int16_t>(t));
            continue;
        }
        for (int i = 0; i < m.count; ++i) {
            const FighterState& f = m.f[i];
            if (f.out || !near(f.pos, flag.pos, reach)) continue;
            if (f.team != t && !carriesObjective(m, i)) {
                flag.carrier = static_cast<int8_t>(i);
                flag.home = false;
                emit(ev, m, ImpactKind::FlagTaken, i, ImpactClass::Medium, flag.pos, {}, Fx{}, static_cast<int16_t>(t));
                break;
            }
            if (f.team == t && !flag.home) {
                flag = FlagState{};
                flag.pos = home;
                emit(ev, m, ImpactKind::FlagReturned, i, ImpactClass::Light, home, {}, Fx{}, static_cast<int16_t>(t));
                break;
            }
        }
    }
}

void tickOddball(MatchState& m, Ev ev) {
    ObjectiveState& o = m.objective;
    if (o.ballCarrier >= 0) {
        const FighterState& c = m.f[o.ballCarrier];
        if (c.out) {
            emit(ev, m, ImpactKind::BallDropped, o.ballCarrier, ImpactClass::Medium, o.ballPos, {}, Fx{});
            o.ballCarrier = -1;
            o.ballPos.y = Fx{};
            return;
        }
        o.ballPos = c.pos + Vec3{Fx{}, Fx::ratio(15, 10), Fx{}};
        if (++o.ballHeld >= 60) { o.ballHeld = 0; ++o.score[c.team]; }
        return;
    }
    for (int i = 0; i < m.count; ++i) {
        if (m.f[i].out || carriesObjective(m, i) || !near(m.f[i].pos, o.ballPos, Fx::ratio(15, 10))) continue;
        o.ballCarrier = static_cast<int8_t>(i);
        o.ballHeld = 0;
        emit(ev, m, ImpactKind::BallTaken, i, ImpactClass::Medium, o.ballPos, {}, Fx{});
        break;
    }
}

void tickZones(MatchState& m, const MatchSetup& s, Ev ev) {
    const MapDef& map = mapOf(s);
    const int zones = static_cast<int>(map.zones.size()) < kMaxZones ? static_cast<int>(map.zones.size()) : kMaxZones;
    for (int z = 0; z < zones; ++z) {
        ZoneState& zone = m.objective.zones[z];
        int team = -1;
        bool contested = false;
        for (int i = 0; i < m.count; ++i) {
            if (m.f[i].out || !near(m.f[i].pos, map.zones[z], Fx::fromInt(3))) continue;
            if (team < 0) team = m.f[i].team;
            else if (team != m.f[i].team) contested = true;
        }
        if (team >= 0 && !contested && team != zone.owner) {
            if (zone.capturing != team) { zone.capturing = static_cast<int8_t>(team); zone.progress = 0; }
            if (++zone.progress >= 240) {
                zone.owner = static_cast<int8_t>(team);
                zone.capturing = -1;
                zone.progress = 0;
                emit(ev, m, ImpactKind::ZoneCaptured, team, ImpactClass::Heavy, map.zones[z], {}, Fx{}, static_cast<int16_t>(z));
            }
        } else if (team < 0 && zone.progress > 0) {
            --zone.progress;
        }
        if (zone.owner >= 0 && m.frame % 60 == 0) ++m.objective.score[zone.owner];
    }
}

void tickObjective(MatchState& m, const MatchSetup& s, Ev ev) {
    switch (s.rules.objective) {
        case Objective::CaptureTheFlag: tickFlags(m, s, ev); break;
        case Objective::Oddball: tickOddball(m, ev); break;
        case Objective::Checkpoints: tickZones(m, s, ev); break;
        default: break;
    }
}

void checkWinner(MatchState& m, const MatchSetup& s) {
    const ModeRules& rules = s.rules;
    if (rules.objective == Objective::None) return; // open brawl never ends

    if (rules.objective == Objective::Elimination) {
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

    // Score based: FFA deathmatch counts personal KOs
    auto scoreOf = [&](int team) -> int {
        if (rules.teamCount == 0 && rules.objective == Objective::Deathmatch) return m.f[team].kos;
        return m.objective.score[team];
    };
    const int teams = rules.teamCount > 0 ? rules.teamCount : m.count;
    int best = 0;
    bool tie = false;
    for (int t = 1; t < teams; ++t) {
        if (scoreOf(t) > scoreOf(best)) { best = t; tie = false; }
        else if (scoreOf(t) == scoreOf(best)) tie = true;
    }
    if (rules.scoreLimit > 0 && scoreOf(best) >= rules.scoreLimit) m.winner = static_cast<uint8_t>(best);
    else if (rules.timeLimitFrames > 0 && m.frame + 1 >= rules.timeLimitFrames) m.winner = tie ? kDraw : static_cast<uint8_t>(best);
}

} // namespace detail

using namespace detail;

int bondLevel(const FighterState& f) { return 1 + (f.bond >= 3) + (f.bond >= 6); }

MatchState createMatch(const MatchSetup& s) {
    MatchState m{};
    m.count = static_cast<uint8_t>(s.count);
    for (int i = 0; i < s.count; ++i) resetFighter(m, s, i, true);
    for (int i = 0; i < s.count; ++i) m.f[i].target = static_cast<int8_t>(nearestEnemy(m, i));
    const MapDef& map = mapOf(s);
    for (int k = 0; k < static_cast<int>(map.obstacles.size()) && k < kMaxObstacles; ++k) m.obstacles[k].hp = map.obstacles[k].hp;
    for (int k = static_cast<int>(map.obstacles.size()); k < kMaxObstacles; ++k) m.obstacles[k].broken = true;
    for (int t = 0; t < kMaxFlags; ++t) m.objective.flags[t].pos = teamBase(s, t);
    return m;
}

void step(MatchState& m, const MatchSetup& s, const FrameInput inputs[], EventSink* ev) {
    if (m.winner != kNoWinner) return;
    const int n = m.count;
    for (int i = 0; i < n; ++i) {
        FrameInput in = s.loadout[i].bot ? botInput(m, s, i) : inputs[i];
        if (in.hasWorldMove()) in.dir = numpadFrom(m.f[i].facing, worldMove(in));
        m.f[i].input.push(in);
    }

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

    // collect first, then apply, so simultaneous hits trade
    MeleeCheck hits[kMaxFighters];
    for (int i = 0; i < n; ++i) if (!frozen[i]) hits[i] = checkMelee(m, s, i);
    for (int i = 0; i < n; ++i) {
        if (hits[i].hit.valid) {
            m.f[i].moveHasHit = true;
            applyHit(m, s, i, hits[i].hit, ev);
        } else if (hits[i].active &&
                   meleeHitsCompanion(m, s, i, moveOf(s, i, m.f[i].moveId), hits[i].center, hits[i].radius, ev)) {
            m.f[i].moveHasHit = true;
        }
    }
    for (int i = 0; i < n; ++i) if (!m.f[i].out) companionAttacks(m, s, i, ev);

    updateProjectiles(m, s, ev);
    tickRealms(m, s, ev);
    handleKnockouts(m, s, ev);
    tickObjective(m, s, ev);
    checkWinner(m, s);
    ++m.frame;
}

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
    for (const FrameInput& in : hist_)
        x.add(static_cast<uint64_t>(in.dir | (in.buttons << 8) | (static_cast<uint64_t>(in.consumed) << 24) |
                                    (static_cast<uint64_t>(static_cast<uint8_t>(in.moveX)) << 40) |
                                    (static_cast<uint64_t>(static_cast<uint8_t>(in.moveZ)) << 48)));
    return x.h;
}

uint64_t hashState(const MatchState& m) {
    Hasher x;
    x.addInt(m.frame); x.addInt(m.count); x.addInt(m.winner);
    for (int i = 0; i < m.count; ++i) {
        const FighterState& f = m.f[i];
        x.add(f.pos); x.add(f.vel); x.add(f.facing);
        x.addInt(f.ce); x.addInt(f.pressFrame); x.addInt(f.linkFrame); x.addInt(f.guardPressFrame); x.addInt(f.oathTimer);
        x.addInt(f.health); x.addInt(f.realmMeter); x.addInt(f.stateFrame); x.addInt(f.moveId); x.addInt(f.moveFrame);
        x.addInt(f.stun); x.addInt(f.hitstop); x.addInt(f.comboTaken); x.addInt(f.burnout); x.addInt(f.lock);
        x.addInt(f.healed);
        for (int16_t c : f.cooldown) x.addInt(c);
        x.addInt(static_cast<int>(f.state)); x.addInt(f.armorLeft);
        x.addInt(static_cast<int>(f.moveHasHit) | (f.grounded << 1) | (f.launched << 2) | (f.reinforce << 3) | (f.maxOutputUsed << 4) |
                 (f.manualTarget << 5) | (f.out << 6) | (f.lockOn << 7));
        x.addInt(f.spawnFrame);
        x.h = f.input.hash(x.h);
        for (int32_t t : f.tech) x.addInt(t);
        x.addInt(f.lostMask); x.addInt(f.bond);
        x.addInt(f.team); x.addInt(f.target); x.addInt(f.respawn); x.addInt(f.kos); x.addInt(f.deaths); x.addInt(f.lastHitBy);
        x.addInt(f.realm.timer); x.addInt(f.realm.open); x.add(f.realm.center); x.addInt(f.realm.clashWith);
        x.addInt(f.realm.clashTimer); x.addInt(f.realm.clashCount);
        for (const CompanionState& c : f.comp) {
            x.addInt(c.roster); x.addInt(c.action); x.addInt(static_cast<int>(c.link) | (c.hasHit << 1)); x.addInt(c.hp);
            x.addInt(c.actionFrame); x.addInt(c.cooldown); x.addInt(c.down); x.addInt(c.lock); x.addInt(c.expires);
            x.addInt(c.since); x.addInt(c.adapted); x.add(c.pos); x.add(c.facing);
        }
    }
    for (const Projectile& p : m.proj) {
        x.addInt(p.alive);
        if (!p.alive) continue;
        x.addInt(p.owner); x.addInt(p.source); x.addInt(p.moveIndex); x.addInt(p.life); x.addInt(p.armDelay);
        x.add(p.pos); x.add(p.vel); x.add(p.radius); x.addInt(p.beam); x.add(p.length);
    }
    for (const ObstacleState& o : m.obstacles) { x.addInt(o.hp); x.addInt(o.broken); }
    const ObjectiveState& o = m.objective;
    for (int16_t sc : o.score) x.addInt(sc);
    for (const FlagState& fl : o.flags) { x.add(fl.pos); x.addInt(fl.carrier); x.addInt(fl.home); x.addInt(fl.returnTimer); }
    for (const ZoneState& z : o.zones) { x.addInt(z.owner); x.addInt(z.capturing); x.addInt(z.progress); }
    x.add(o.ballPos); x.addInt(o.ballCarrier); x.addInt(o.ballHeld);
    return x.h;
}

} // namespace fc
