#pragma once
// Interne Helfer, die Simulation.cpp und Companion.cpp teilen. Nicht Teil der öffentlichen API.
#include "fightcore/Simulation.h"

namespace fc::detail {

using Ev = EventSink*;

void emit(Ev ev, const MatchState& m, ImpactKind k, int src, ImpactClass s, Vec3 pos, Vec3 dir, Fx force, int16_t tag = 0);
const PactModifiers& mods(const MatchState& m, const MatchSetup& s, int i);
const TechniqueLogic* logicOf(const MatchSetup& s, int i);
bool overlap(Vec3 a, Fx ra, Vec3 b, Fx rb);
Fx distXZ(Vec3 a, Vec3 b);
bool realmAffects(const MatchState& m, const MatchSetup& s, int target);
void triggerBurnout(MatchState& m, int i, Ev ev);
void addRealmMeter(FighterState& f, int amount);
bool hitsFighter(const MatchState& m, int target, Vec3 center, Fx radius);
void moveToward(Vec3& p, Vec3 target, Fx speed);
bool isInvulnerable(const MatchState& m, const MatchSetup& s, int i);

struct PendingHit {
    bool valid = false;
    const MoveDef* move = nullptr;
    int16_t moveId = -1;       // wie FighterState::moveId (Projektil: Index + 100), -1 bei Begleitern
    bool projectile = false;
    Vec3 point{}, dir{};
    int8_t companion = -1;     // Roster-Index, wenn ein Begleiter der Angreifer ist
    bool link = false;         // Link-Attack (Bond-Stufe 2)
    Vec3 source{};             // Position des Angreifers (Begleiter)
};
void applyHit(MatchState& m, const MatchSetup& s, int atk, const PendingHit& h, Ev ev);

// ---- Companion.cpp
void buildRoster(MatchSetup& s);
void initCompanions(MatchState& m, const MatchSetup& s);
int32_t companionUpkeep(const MatchState& m, const MatchSetup& s, int owner);
void tryAssistCommand(MatchState& m, const MatchSetup& s, int owner, Ev ev);
void tickCompanions(MatchState& m, const MatchSetup& s, int owner, Ev ev);
// Kampf-Interaktion
void companionAttacks(MatchState& m, const MatchSetup& s, int owner, Ev ev);
bool meleeHitsCompanion(MatchState& m, const MatchSetup& s, int atk, const MoveDef& mv, Vec3 center, Fx radius, Ev ev);
bool projectileHitsCompanion(MatchState& m, const MatchSetup& s, Projectile& p, const MoveDef& mv, Ev ev);
int shieldingCompanion(const MatchState& m, const MatchSetup& s, int defender, Vec3 source);
void damageCompanion(MatchState& m, const MatchSetup& s, int owner, int slot, int amount, MoveCategory cat, Ev ev);
void leash(MatchState& m, const MatchSetup& s, int owner);
void realmHitsCompanions(MatchState& m, const MatchSetup& s, int target, RealmClass cls, int ownerBond, Ev ev);
const MoveDef& companionMove(const MatchSetup& s, int owner, int roster, int16_t move);

} // namespace fc::detail
