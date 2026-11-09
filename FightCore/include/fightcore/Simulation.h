#pragma once
#include "fightcore/Body.h"
#include "fightcore/Companion.h"
#include "fightcore/Events.h"
#include "fightcore/Pact.h"
#include "fightcore/State.h"
#include "fightcore/Technique.h"
#include "fightcore/Types.h"

namespace fc {

// Spielmodi
//  Duel  : 1v1, Elimination
//  Teams : 2v2, Elimination, kein Friendly Fire
//  Brawl : bis zu 20 Spieler, Free-for-all auf großer Map, Respawn, Zeitlimit, meiste K.O.s gewinnen
enum class GameMode : uint8_t { Duel, Teams, Brawl };

struct ModeRules {
    int8_t fighters = 2;          // Lobby-Größe (wird mit Bots aufgefüllt)
    bool teams = false;           // false: jeder Kämpfer ist sein eigenes Team
    int16_t respawnFrames = -1;   // -1 = kein Respawn (Elimination)
    int32_t timeLimitFrames = -1; // -1 = kein Zeitlimit
    int16_t koLimit = 0;          // 0 = keins
    Fx arenaHalfExtent = Fx::fromInt(12);
};
ModeRules rulesFor(GameMode mode);

// Ein Build = Body + Technik (+ optionaler Pakt). Technik = nullptr nur beim Nullgeborenen.
struct Loadout {
    const BodyDef* body = nullptr;
    const TechniqueDef* technique = nullptr;
    PactChoice pact;
    PlaystyleProfile profile;
    const CompanionDef* assist = nullptr; // Summoner-Assist (nullptr -> Klingenwolf)
    uint8_t team = 0;                     // nur im Teams-Modus relevant
    bool bot = false;
    uint8_t botLevel = 1;                 // 0 leicht, 1 normal, 2 schwer
};

struct MatchSetup {
    GameMode mode = GameMode::Duel;
    int8_t count = 2;
    Loadout loadout[kMaxFighters];
    StageDef stage;

    // Von finalize() berechnet:
    BodyStats stats[kMaxFighters];
    PactResult pact[kMaxFighters];
    // Begleiter-Roster: [Summoner-Assist][Technik-Begleiter...][Pakt-Begleiter]
    const CompanionDef* roster[kMaxFighters][kRosterMax] = {};
    int8_t rosterCount[kMaxFighters] = {};
    int8_t assistRoster[kMaxFighters] = {};
    int8_t techRosterStart[kMaxFighters] = {};
    int8_t pactRoster[kMaxFighters] = {};
    void finalize();
};

// Lobby: Menschen übernehmen die ersten Slots, der Rest wird mit Bots aufgefüllt (deterministisch per Seed).
// Teams-Modus: Menschen behalten ihr Team, Bots füllen das kleinere Team auf.
MatchSetup buildMatch(GameMode mode, const Loadout* humans, int humanCount, uint8_t botLevel, uint32_t seed);

constexpr int kBufferWindow = 8;
constexpr int16_t kRealmCastFrames = 60;  // für alle gleich
constexpr int16_t kRealmFrames = 600;     // 10 s, für alle gleich
constexpr int16_t kClashFrames = 90;
constexpr int16_t kBurnoutFrames = 180;   // 3 s
constexpr int16_t kOpenRealmRadius = 10;
constexpr int16_t kRealmRadius = 18;      // geschlossener Realm (deckt die 1v1-Arena ab)

// Basis-Moves, die JEDER Build hat
enum BaseMove : int16_t { kLight, kHeavy, kLauncher, kAirLight, kGrab, kBurst, kRealmCast, kBaseMoveCount };
constexpr int16_t kTechMoveOffset = 100;

const MoveDef& baseMove(int16_t id);
const MoveDef& moveOf(const MatchSetup& s, int fighter, int16_t moveId);

MatchState createMatch(const MatchSetup& setup);

int bondLevel(const FighterState& f); // 1..3
bool isEnemy(const MatchState& m, int a, int b);
const CompanionDef& companionDef(const MatchSetup& s, int owner, const CompanionState& c);
// inputs: ein Eintrag pro Kämpfer (setup.count). Bot-Slots werden ignoriert, Bots denken in der Simulation.
void step(MatchState& m, const MatchSetup& setup, const FrameInput inputs[], EventSink* events = nullptr);

// Bot-KI: deterministisch aus dem Spielzustand -> rollback- und netzwerksicher.
FrameInput botInput(const MatchState& m, const MatchSetup& setup, int fighter);

} // namespace fc
