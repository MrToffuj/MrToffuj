// Begleiter-System: Roster, Befehle, Verhalten, Schaden, Leash, Realm-Interaktion.
#include "Internal.h"

namespace fc {

const CompanionDef& companionDef(const MatchSetup& s, int owner, const CompanionState& c) {
    return *s.roster[owner][c.roster];
}

namespace detail {
namespace {

const PactModifiers kNoPact{};
const PactModifiers& setupMods(const MatchSetup& s, int i) {
    return s.pact[i].tier == PactTier::None ? kNoPact : s.pact[i].mods;
}

int16_t companionMaxHp(const MatchSetup& s, int owner, const CompanionDef& d) {
    return static_cast<int16_t>(d.maxHp * setupMods(s, owner).companionHpPct / 100);
}

Vec3 homePos(const FighterState& f, int slot) {
    Vec3 right{f.facing.z, Fx{}, -f.facing.x};
    Fx side = slot == 0 ? Fx::ratio(9, 10) : slot == 1 ? Fx::ratio(-9, 10) : Fx{};
    Vec3 p = f.pos - f.facing + right * side;
    p.y = Fx::ratio(6, 10);
    return p;
}

void place(MatchState& m, const MatchSetup& s, int owner, int slot, int roster, Vec3 pos) {
    CompanionState& c = m.f[owner].comp[slot];
    c = CompanionState{};
    c.roster = static_cast<int8_t>(roster);
    c.hp = companionMaxHp(s, owner, *s.roster[owner][roster]);
    c.since = m.frame;
    c.pos = pos;
    c.facing = m.f[owner].facing;
}

void abortAction(CompanionState& c, const CompanionDef& d) {
    if (c.action < 0) return;
    c.action = -1;
    c.link = false;
    c.cooldown = d.cooldown;
}

const CompanionType kPreference[4][4] = {
    {CompanionType::Striker, CompanionType::Zoner, CompanionType::Support, CompanionType::Guard},   // Attack
    {CompanionType::Striker, CompanionType::Support, CompanionType::Zoner, CompanionType::Guard},   // Pressure
    {CompanionType::Zoner, CompanionType::Striker, CompanionType::Support, CompanionType::Guard},   // AntiAir
    {CompanionType::Guard, CompanionType::Support, CompanionType::Striker, CompanionType::Zoner},   // Protect
};

bool ready(const CompanionState& c) { return c.onField() && c.action < 0 && c.lock == 0; }

int pickSlot(const MatchState& m, const MatchSetup& s, int owner, AssistRole role) {
    const FighterState& f = m.f[owner];
    for (CompanionType t : kPreference[static_cast<int>(role)])
        for (int i = 0; i < kCompanionSlots; ++i) {
            const CompanionState& c = f.comp[i];
            if (ready(c) && c.cooldown == 0 && companionDef(s, owner, c).type == t) return i;
        }
    return -1;
}

void startAction(MatchState& m, const MatchSetup& s, int owner, int slot, AssistRole role, bool link, Ev ev) {
    CompanionState& c = m.f[owner].comp[slot];
    const AssistAction& a = companionDef(s, owner, c).actions[static_cast<int>(role)];
    c.action = static_cast<int8_t>(role);
    c.actionFrame = 0;
    c.hasHit = false;
    c.link = link && (a.kind == AssistKind::Rush || a.kind == AssistKind::Pull || a.kind == AssistKind::Rise);
    if (c.link) {
        // Link-Attack: Begleiter taucht direkt am Gegner auf und schlägt sofort zu
        const FighterState& o = m.f[1 - owner];
        Vec3 dir = normalizeXZ(o.pos - m.f[owner].pos, m.f[owner].facing);
        c.pos = o.pos - dir * Fx::ratio(8, 10);
        c.pos.y = Fx::ratio(6, 10);
        c.facing = dir;
        c.actionFrame = a.startup;
    }
    emit(ev, m, ImpactKind::CompanionCall, owner, ImpactClass::Light, c.pos, c.facing, Fx{},
         static_cast<int16_t>(static_cast<int>(role) + (c.link ? 10 : 0)));
}

Vec3 normalize3(Vec3 v, Vec3 fallback) {
    Fx len = length(v);
    if (len.raw < 16) return fallback;
    return {v.x / len, v.y / len, v.z / len};
}

} // namespace

const MoveDef& companionMove(const MatchSetup& s, int owner, int roster, int16_t move) {
    return s.roster[owner][roster]->moves[move];
}

void buildRoster(MatchSetup& s) {
    for (int i = 0; i < 2; ++i) {
        int n = 0;
        s.assistRoster[i] = s.techRosterStart[i] = s.pactRoster[i] = -1;
        if (s.loadout[i].body->has(PassiveAssist)) {
            s.roster[i][n] = s.loadout[i].assist ? s.loadout[i].assist : &companions::bladeWolf();
            s.assistRoster[i] = static_cast<int8_t>(n++);
        }
        const TechniqueDef* t = s.loadout[i].technique;
        if (t && !t->companions.empty()) {
            s.techRosterStart[i] = static_cast<int8_t>(n);
            for (const CompanionDef* c : t->companions)
                if (n < kRosterMax - 1) s.roster[i][n++] = c;
        }
        const uint8_t bonus = setupMods(s, i).bonusCompanion;
        if (bonus) {
            s.roster[i][n] = bonus == 2 ? &companions::shifter() : &companions::pactSpirit();
            s.pactRoster[i] = static_cast<int8_t>(n++);
        }
        s.rosterCount[i] = static_cast<int8_t>(n);
    }
}

void initCompanions(MatchState& m, const MatchSetup& s) {
    for (int i = 0; i < 2; ++i) {
        FighterState& f = m.f[i];
        const PactModifiers& pm = setupMods(s, i);
        const bool techComps = s.techRosterStart[i] >= 0;
        int slot = 0;
        if (s.assistRoster[i] >= 0 && !(pm.lockedCompanionSlots > 0 && !techComps)) {
            place(m, s, i, slot++, s.assistRoster[i], homePos(f, 0));
            if (pm.companionLost && !techComps) f.lostMask |= static_cast<uint8_t>(1u << s.assistRoster[i]); // kein Revive
        }
        if (pm.companionLost && techComps) f.lostMask |= static_cast<uint8_t>(1u << s.techRosterStart[i]);
        if (s.pactRoster[i] >= 0) {
            place(m, s, i, slot, s.pactRoster[i], homePos(f, slot));
            f.comp[slot].expires = pm.durationFrames > 0 ? pm.durationFrames : 1800;
        }
    }
}

int32_t companionUpkeep(const MatchState& m, const MatchSetup& s, int owner) {
    int32_t sum = 0;
    for (const CompanionState& c : m.f[owner].comp)
        if (c.onField()) sum += companionDef(s, owner, c).upkeep;
    return sum;
}

void tryAssistCommand(MatchState& m, const MatchSetup& s, int owner, Ev ev) {
    FighterState& f = m.f[owner];
    if (f.burnout > 0) return;
    const int a = f.input.pressAge(BtnAssist, kBufferWindow);
    if (a < 0) return;
    const uint8_t dir = f.input.ago(a).dir;
    const AssistRole role = dir == 6 ? AssistRole::Pressure
                          : dir == 8 ? AssistRole::AntiAir
                          : dir == 2 ? AssistRole::Protect : AssistRole::Attack;
    const int slot = pickSlot(m, s, owner, role);
    if (slot < 0) return; // bleibt gebuffert
    f.input.consume(BtnAssist, kBufferWindow);
    const bool link = bondLevel(f) >= 2 && f.state == FState::Attack && f.moveHasHit;
    startAction(m, s, owner, slot, role, link, ev);
}

void tickCompanions(MatchState& m, const MatchSetup& s, int owner, Ev ev) {
    FighterState& f = m.f[owner];
    const FighterState& o = m.f[1 - owner];
    for (int slot = 0; slot < kCompanionSlots; ++slot) {
        CompanionState& c = f.comp[slot];
        if (c.roster < 0) continue;
        const CompanionDef& d = companionDef(s, owner, c);

        if (c.expires > 0 && --c.expires == 0) {
            emit(ev, m, ImpactKind::CompanionGone, owner, ImpactClass::Light, c.pos, {}, Fx{});
            c = CompanionState{};
            continue;
        }
        if (c.down > 0) {
            if (--c.down == 0) {
                const int32_t cost = d.reviveCost * 100;
                if (f.ce >= cost) {
                    f.ce -= cost;
                    c.hp = companionMaxHp(s, owner, d);
                    c.pos = homePos(f, slot);
                    emit(ev, m, ImpactKind::CompanionRevive, owner, ImpactClass::Medium, c.pos, {}, Fx{});
                } else {
                    c.down = 1; // wartet auf genug Essenz
                }
            }
            continue;
        }
        if (c.cooldown > 0) --c.cooldown;
        if (m.realm.clashTimer > 0) continue;
        if (c.lock > 0) { --c.lock; continue; }

        if (c.action < 0) {
            c.facing = normalizeXZ(o.pos - c.pos, c.facing);
            moveToward(c.pos, homePos(f, slot), Fx::ratio(15, 100));
            continue;
        }

        const AssistAction& a = d.actions[c.action];
        ++c.actionFrame;
        const bool startup = c.actionFrame <= a.startup;
        switch (a.kind) {
            case AssistKind::Rush:
            case AssistKind::Pull:
                if (startup) {
                    c.facing = normalizeXZ(o.pos - c.pos, c.facing);
                    Vec3 t = o.pos - c.facing * Fx::ratio(8, 10);
                    t.y = Fx::ratio(6, 10);
                    moveToward(c.pos, t, a.speed);
                }
                break;
            case AssistKind::Rise:
                if (startup) moveToward(c.pos, f.pos + f.facing + Vec3{Fx{}, Fx::fromInt(2), Fx{}}, a.speed);
                break;
            case AssistKind::Shoot:
                c.facing = normalizeXZ(o.pos - c.pos, c.facing);
                if (c.actionFrame == a.startup) {
                    Vec3 from = c.pos + Vec3{Fx{}, Fx::ratio(5, 10), Fx{}};
                    Vec3 dir = normalize3(o.pos + Vec3{Fx{}, Fx::ratio(12, 10), Fx{}} - from, c.facing);
                    for (Projectile& p : m.proj) {
                        if (p.alive) continue;
                        p = Projectile{true, static_cast<uint8_t>(owner), static_cast<int8_t>(c.roster + 1), a.move, 90, 0,
                                       from, dir * a.speed, Fx::ratio(45, 100)};
                        break;
                    }
                }
                break;
            case AssistKind::Shield: {
                Vec3 t = f.pos + f.facing * Fx::ratio(9, 10);
                t.y = Fx::ratio(6, 10);
                moveToward(c.pos, t, a.speed);
                c.facing = f.facing;
                break;
            }
        }
        if (c.actionFrame >= a.total()) abortAction(c, d);
    }
}

void companionAttacks(MatchState& m, const MatchSetup& s, int owner, Ev ev) {
    FighterState& f = m.f[owner];
    const int target = 1 - owner;
    for (CompanionState& c : f.comp) {
        if (!c.onField() || c.action < 0 || c.hasHit || c.lock > 0) continue;
        const CompanionDef& d = companionDef(s, owner, c);
        const AssistAction& a = d.actions[c.action];
        if (a.kind == AssistKind::Shoot || a.kind == AssistKind::Shield) continue;
        if (c.actionFrame <= a.startup || c.actionFrame > a.startup + a.active) continue;
        Vec3 center = a.kind == AssistKind::Rise ? c.pos : c.pos + c.facing * Fx::ratio(6, 10);
        Fx radius = a.kind == AssistKind::Rise ? Fx::fromInt(1) : a.kind == AssistKind::Pull ? Fx::ratio(7, 10) : Fx::ratio(6, 10);
        if (isInvulnerable(m, s, target) || !hitsFighter(m, target, center, radius)) continue;
        c.hasHit = true;
        PendingHit h{true, &d.moves[a.move], -1, false, center, c.facing};
        if (a.kind == AssistKind::Pull) h.dir = normalizeXZ(f.pos - m.f[target].pos, c.facing); // zum Spieler ziehen
        h.companion = c.roster;
        h.link = c.link;
        h.source = c.pos;
        applyHit(m, s, owner, h, ev);
    }
}

bool meleeHitsCompanion(MatchState& m, const MatchSetup& s, int atk, const MoveDef& mv, Vec3 center, Fx radius, Ev ev) {
    const int def = 1 - atk;
    for (int slot = 0; slot < kCompanionSlots; ++slot) {
        CompanionState& c = m.f[def].comp[slot];
        if (!c.onField() || !overlap(center, radius, c.pos, Fx::ratio(5, 10))) continue;
        m.f[atk].hitstop = 4;
        emit(ev, m, ImpactKind::Hit, atk, mv.hit.impact, c.pos, m.f[atk].facing, mv.hit.knockback, 4);
        damageCompanion(m, s, def, slot, mv.hit.damage, mv.category, ev);
        return true;
    }
    return false;
}

bool projectileHitsCompanion(MatchState& m, const MatchSetup& s, Projectile& p, const MoveDef& mv, Ev ev) {
    const int def = 1 - p.owner;
    for (int slot = 0; slot < kCompanionSlots; ++slot) {
        CompanionState& c = m.f[def].comp[slot];
        if (!c.onField() || !overlap(p.pos, p.radius, c.pos, Fx::ratio(5, 10))) continue;
        const bool shielding = c.action >= 0 && companionDef(s, def, c).actions[c.action].kind == AssistKind::Shield;
        if (shielding && m.f[def].bond < kBondMax) ++m.f[def].bond;
        emit(ev, m, ImpactKind::ProjectileBurst, p.owner, mv.hit.impact, c.pos, {}, Fx{}, 4);
        damageCompanion(m, s, def, slot, mv.hit.damage, mv.category, ev);
        return true;
    }
    return false;
}

int shieldingCompanion(const MatchState& m, const MatchSetup& s, int defender, Vec3 source) {
    const FighterState& f = m.f[defender];
    if (dot(f.facing, source - f.pos) <= Fx{}) return -1; // nur Treffer von vorne
    for (int slot = 0; slot < kCompanionSlots; ++slot) {
        const CompanionState& c = f.comp[slot];
        if (!c.onField() || c.action < 0 || c.lock > 0) continue;
        const AssistAction& a = companionDef(s, defender, c).actions[c.action];
        if (a.kind == AssistKind::Shield && c.actionFrame > a.startup && c.actionFrame <= a.startup + a.active)
            return slot;
    }
    return -1;
}

void damageCompanion(MatchState& m, const MatchSetup& s, int owner, int slot, int amount, MoveCategory cat, Ev ev) {
    FighterState& f = m.f[owner];
    CompanionState& c = f.comp[slot];
    const CompanionDef& d = companionDef(s, owner, c);
    if (d.adaptive) {
        // Adaption: dieselbe Angriffs-Kategorie macht beim nächsten Mal nur halben Schaden
        const uint8_t bit = catBit(cat);
        if (c.adapted & bit) amount /= 2;
        c.adapted = static_cast<uint8_t>(c.adapted | bit);
    }
    c.hp = static_cast<int16_t>(c.hp - amount);
    if (c.action >= 0 && d.actions[c.action].kind != AssistKind::Shield) abortAction(c, d);
    if (c.hp > 0) return;

    emit(ev, m, ImpactKind::CompanionDown, owner, ImpactClass::Heavy, c.pos, {}, Fx{});
    f.bond = 0; // Bond-Fortschritt weg
    const uint8_t bit = static_cast<uint8_t>(1u << c.roster);
    if (c.roster == s.assistRoster[owner] && !(f.lostMask & bit)) {
        c.hp = 0;
        c.action = -1;
        c.link = false;
        c.down = d.reviveFrames; // Summoner-Assist kommt zurück
    } else {
        f.lostMask = static_cast<uint8_t>(f.lostMask | bit); // für die Runde weg
        c = CompanionState{};
    }
}

void leash(MatchState& m, const MatchSetup& s, int owner) {
    for (CompanionState& c : m.f[owner].comp)
        if (c.onField() && c.action >= 0) abortAction(c, companionDef(s, owner, c));
}

void realmHitsCompanions(MatchState& m, const MatchSetup& s, int target, RealmClass cls, int ownerBond, Ev ev) {
    const RealmState& r = m.realm;
    const int elapsed = kRealmFrames - r.timer;
    const bool bond3 = ownerBond >= 3;
    for (int slot = 0; slot < kCompanionSlots; ++slot) {
        CompanionState& c = m.f[target].comp[slot];
        if (!c.onField()) continue;
        if (r.open && distXZ(m.f[r.owner].pos, c.pos) > Fx::fromInt(kOpenRealmRadius)) continue;
        if (cls == RealmClass::Damage && elapsed % 30 == 0) {
            emit(ev, m, ImpactKind::SureHit, r.owner, ImpactClass::Medium, c.pos, {}, Fx{}, 4);
            damageCompanion(m, s, target, slot, bond3 ? 25 : 20, MoveCategory::Realm, ev);
        } else if (cls == RealmClass::Lock && elapsed % 200 == 0) {
            abortAction(c, companionDef(s, target, c));
            c.lock = static_cast<int16_t>(bond3 ? 120 : 90);
        }
    }
}

} // namespace detail

// ---------------------------------------------------------------- TechContext-Anbindung
int TechContext::companionSlot(int16_t techCompanion) const {
    const int r = setup.techRosterStart[self] + techCompanion;
    for (int i = 0; i < kCompanionSlots; ++i)
        if (me().comp[i].roster == r) return i;
    return -1;
}

bool TechContext::companionLost(int16_t techCompanion) const {
    if (setup.techRosterStart[self] < 0) return true;
    return (me().lostMask & (1u << (setup.techRosterStart[self] + techCompanion))) != 0;
}

int TechContext::summon(int16_t techCompanion) const {
    using namespace detail;
    if (setup.techRosterStart[self] < 0 || companionLost(techCompanion)) return -1;
    int existing = companionSlot(techCompanion);
    if (existing >= 0) return existing;

    FighterState& f = me();
    const int maxTech = kTechCompanionMax - mods(m, setup, self).lockedCompanionSlots;
    if (maxTech <= 0) return -1;
    const int start = setup.techRosterStart[self];
    const int end = start + static_cast<int>(setup.loadout[self].technique->companions.size());
    int count = 0, oldest = -1, empty = -1;
    for (int i = 0; i < kCompanionSlots; ++i) {
        const CompanionState& c = f.comp[i];
        if (c.roster < 0) { if (empty < 0) empty = i; continue; }
        if (c.roster >= start && c.roster < end) {
            ++count;
            if (oldest < 0 || c.since < f.comp[oldest].since) oldest = i;
        }
    }
    int slot = count >= maxTech ? oldest : empty;
    if (slot < 0) return -1;
    if (count >= maxTech) detail::emit(events, m, ImpactKind::CompanionGone, self, ImpactClass::Light, f.comp[slot].pos, {}, Fx{});
    place(m, setup, self, slot, start + techCompanion, f.pos + f.facing + Vec3{Fx{}, Fx::ratio(6, 10), Fx{}});
    detail::emit(events, m, ImpactKind::CompanionSummon, self, ImpactClass::Medium, f.comp[slot].pos, {}, Fx{}, techCompanion);
    return slot;
}

bool TechContext::command(int slot, AssistRole role) const {
    if (slot < 0 || slot >= kCompanionSlots || !detail::ready(me().comp[slot])) return false;
    detail::startAction(m, setup, self, slot, role, false, events);
    return true;
}

} // namespace fc
