#pragma once
// Basis-Klasse für Charakter-Techniken (die "Fluch-Techniken" unseres Spiels).
// REGEL: Techniken sind STATELESS (alle Methoden const). Zustand -> FighterState::tech.
#include "fightcore/Events.h"
#include "fightcore/State.h"

namespace fc {

struct MatchSetup;

struct TechContext {
    MatchState& m;
    const MatchSetup& setup;
    int self;
    EventSink* events;

    FighterState& me() const { return m.f[self]; }
    FighterState& opp() const { return m.f[1 - self]; }
    const CharacterDef& def() const;
    bool realmIsMine() const { return m.realm.owner == self; }

    int spawnProjectile(Vec3 pos, Vec3 vel, Fx radius, int16_t life, int16_t moveId) const;
    void emit(ImpactKind kind, Vec3 pos, Vec3 dir, Fx force, ImpactClass strength, int16_t tag = 0) const;
    Vec3 localToWorld(Vec3 offset) const;
};

class Technique {
public:
    virtual ~Technique() = default;
    // Feuert bei MoveDef::eventFrame
    virtual void onMoveEvent(const TechContext&, int16_t /*moveId*/) const {}
    // Jeden Frame (nach Bewegung, vor Trefferprüfung)
    virtual void onTick(const TechContext&) const {}
    // Wenn der eigene Realm aufgeht
    virtual void onRealmOpen(const TechContext&) const {}
    // Modifikator für ausgeteilten Schaden (z. B. Realm-Buffs). 100 = normal.
    virtual int damagePercent(const TechContext&) const { return 100; }
};

} // namespace fc
