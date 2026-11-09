#pragma once
// Technik-Template (GDD Abschnitt 4): JEDE Technik hat exakt
//   1 Core Rule, 3 Skills (Offense / Utility / Defense-Movement),
//   1 Apex-Technik (Burst / Zone / Transform), 1 Realm (Damage / Lock / Debuff / BuffField), 1 Schwäche.
// Logik ist STATELESS (const). Zustand -> FighterState::tech[].
#include <array>
#include <cstdint>
#include <vector>

#include "fightcore/Events.h"
#include "fightcore/Types.h"

namespace fc {

struct MatchState;
struct MatchSetup;
struct FighterState;
struct BodyStats;

enum class ApexType : uint8_t { Burst, Zone, Transform };
enum class RealmClass : uint8_t { Damage, Lock, Debuff, BuffField };
enum class SkillRole : uint8_t { Offense = 0, Utility = 1, Defense = 2 };

struct SkillSlot {
    int16_t move = -1;     // Index in TechniqueDef::moves
    int16_t ceCost = 0;    // Essenz (ganze Punkte)
    int16_t cooldown = 0;  // Frames
};

class TechniqueLogic;

struct TechniqueDef {
    const char* name = "";
    const char* coreRule = "";
    const char* weakness = "";
    std::vector<MoveDef> moves;      // Skills, Apex, Projektil-Props usw.
    std::array<SkillSlot, 3> skills; // Index = SkillRole
    int16_t apexMove = -1;
    ApexType apexType = ApexType::Burst;
    RealmClass realmClass = RealmClass::Damage;
    const TechniqueLogic* logic = nullptr;
};

struct TechContext {
    MatchState& m;
    const MatchSetup& setup;
    int self;
    EventSink* events;

    FighterState& me() const;
    FighterState& opp() const;
    const BodyStats& stats() const;
    bool realmIsMine() const;
    Vec3 localToWorld(Vec3 offset) const;
    // moveIndex = Index in TechniqueDef::moves (liefert die HitProps)
    int spawnProjectile(Vec3 pos, Vec3 vel, Fx radius, int16_t life, int16_t moveIndex, int16_t armDelay = 0) const;
    void emit(ImpactKind kind, Vec3 pos, Vec3 dir, Fx force, ImpactClass strength, int16_t tag = 0) const;
    void damage(int target, int16_t amount) const; // direkter Schaden (DoT, Selbstschaden)
};

class TechniqueLogic {
public:
    virtual ~TechniqueLogic() = default;
    virtual void onMoveEvent(const TechContext&, int16_t /*moveIndex*/) const {}
    virtual void onTick(const TechContext&) const {}
    virtual void onRealmOpen(const TechContext&) const {}
    virtual void onRealmTick(const TechContext&) const {}
    // Treffer gelandet (Melee oder Projektil). moveIndex >= 0 nur bei Technik-Moves.
    virtual void onHitLanded(const TechContext&, int16_t /*moveIndex*/, bool /*projectile*/) const {}
    // Core Rules wie ein Auto-Block-Feld: true = Treffer wird neutralisiert.
    virtual bool nullifiesIncoming(const TechContext&, const MoveDef&, bool /*projectile*/) const { return false; }
    virtual void onParry(const TechContext&, const MoveDef&) const {}
    virtual int damageDealtPct(const TechContext&, int16_t /*moveIndex*/) const { return 100; }
    virtual int damageTakenPct(const TechContext&) const { return 100; }
};

} // namespace fc
