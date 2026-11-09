#pragma once
// Every technique follows the same template: a core rule, three skills (offense, utility,
// defense/movement), a Max Output, a realm and a weakness.
// Logic is stateless; anything that needs to persist goes into FighterState::tech.
#include <array>
#include <cstdint>
#include <vector>

#include "fightcore/Companion.h"
#include "fightcore/Events.h"
#include "fightcore/Types.h"

namespace fc {

struct MatchState;
struct MatchSetup;
struct FighterState;
struct BodyStats;

enum class MaxOutputType : uint8_t { Burst, Zone, Transform };
enum class RealmClass : uint8_t { Damage, Lock, Debuff, BuffField };
enum class SkillRole : uint8_t { Offense = 0, Utility = 1, Defense = 2 };

struct SkillSlot {
    int16_t move = -1;  // index into TechniqueDef::moves
    int16_t ceCost = 0;  // CE
    int16_t cooldown = 0;
};

class TechniqueLogic;

struct TechniqueDef {
    const char* name = "";
    const char* coreRule = "";
    const char* weakness = "";
    std::vector<MoveDef> moves;
    std::array<SkillSlot, 3> skills;
    int16_t maxOutputMove = -1;
    MaxOutputType maxOutputType = MaxOutputType::Burst;
    RealmClass realmClass = RealmClass::Damage;
    std::vector<const CompanionDef*> companions;
    const TechniqueLogic* logic = nullptr;
};

struct TechContext {
    MatchState& m;
    const MatchSetup& setup;
    int self;
    EventSink* events;

    FighterState& me() const;
    FighterState& opp() const;  // lock-on target, or yourself if there is none
    bool hasTarget() const;
    const BodyStats& stats() const;
    bool realmIsMine() const;
    Vec3 localToWorld(Vec3 offset) const;
    int spawnProjectile(Vec3 pos, Vec3 vel, Fx radius, int16_t life, int16_t moveIndex, int16_t armDelay = 0) const;
    void emit(ImpactKind kind, Vec3 pos, Vec3 dir, Fx force, ImpactClass strength, int16_t tag = 0) const;
    void damage(int target, int16_t amount) const;  // raw damage that skips all modifiers (burn, recoil)
    // Summon one of TechniqueDef::companions. Returns the slot or -1.
    int summon(int16_t techCompanion) const;
    int companionSlot(int16_t techCompanion) const;
    bool companionLost(int16_t techCompanion) const;
    bool command(int slot, AssistRole role) const;
};

class TechniqueLogic {
public:
    virtual ~TechniqueLogic() = default;
    virtual void onMoveEvent(const TechContext&, int16_t /*moveIndex*/) const {}
    virtual bool canUseSkill(const TechContext&, int /*slot*/) const { return true; }
    virtual void onTick(const TechContext&) const {}
    virtual void onRealmOpen(const TechContext&) const {}
    virtual void onRealmTick(const TechContext&) const {}
    // moveIndex is -1 for base moves
    virtual void onHitLanded(const TechContext&, int16_t /*moveIndex*/, bool /*projectile*/, int /*victim*/) const {}
    // return true to cancel the hit entirely
    virtual bool nullifiesIncoming(const TechContext&, const MoveDef&, bool /*projectile*/) const { return false; }
    virtual void onParry(const TechContext&, const MoveDef&) const {}
    virtual int damageDealtPct(const TechContext&, int16_t /*moveIndex*/) const { return 100; }
    virtual int damageTakenPct(const TechContext&) const { return 100; }
};

} // namespace fc
