#pragma once
#include "fightcore/Body.h"
#include "fightcore/Companion.h"
#include "fightcore/Events.h"
#include "fightcore/Oath.h"
#include "fightcore/State.h"
#include "fightcore/Technique.h"
#include "fightcore/Types.h"

namespace fc {

enum class GameMode : uint8_t {
    Duel,   // 1v1, elimination
    Teams,  // 2v2, elimination
    Brawl,  // open colony, up to 20, endless respawns, no score
    Custom, // up to 20, custom teams and objective
};

enum class Objective : uint8_t {
    None,           // free fight, never ends
    Elimination,    // last team standing, no respawns
    Deathmatch,     // team scores per KO
    CaptureTheFlag,
    Oddball,        // hold the ball to score
    Checkpoints,    // capture and hold zones
};

struct ModeRules {
    int8_t fighters = 2;
    uint8_t teamCount = 0;        // 0 = everyone for themselves
    bool friendlyFire = false;
    int16_t respawnFrames = -1;   // -1 = no respawn
    int32_t timeLimitFrames = -1; // -1 = no time limit
    int16_t scoreLimit = 0;       // 0 = none
    Objective objective = Objective::Elimination;
    const MapDef* map = nullptr;
};
ModeRules rulesFor(GameMode mode);

namespace maps {
const MapDef& shrineWard();     // small colony, 1v1
const MapDef& harborDistrict(); // medium colony, 2v2
const MapDef& oldCapital();     // large colony, Brawl / Custom
} // namespace maps

// technique is null for Heaven's Bind
struct Loadout {
    const BodyDef* body = nullptr;
    const TechniqueDef* technique = nullptr;
    OathChoice oath;
    PlaystyleProfile profile;
    const CompanionDef* assist = nullptr;  // summoner only, defaults to Blade Wolf
    uint8_t team = 0;
    bool bot = false;
    uint8_t botLevel = 1;  // 0 easy, 1 normal, 2 hard
};

struct MatchSetup {
    GameMode mode = GameMode::Duel;
    ModeRules rules;
    int8_t count = 2;
    Loadout loadout[kMaxFighters];

    // filled in by finalize()
    BodyStats stats[kMaxFighters];
    OathResult oath[kMaxFighters];
    // companion roster: [summoner assist][technique companions...][oath companion]
    const CompanionDef* roster[kMaxFighters][kRosterMax] = {};
    int8_t rosterCount[kMaxFighters] = {};
    int8_t assistRoster[kMaxFighters] = {};
    int8_t techRosterStart[kMaxFighters] = {};
    int8_t oathRoster[kMaxFighters] = {};
    void finalize();
};

// Humans take the first slots, bots fill the rest (picked from the seed so every peer builds the same lobby).
// With teams, humans keep their team and bots join whichever team is smallest.
MatchSetup buildMatch(GameMode mode, const Loadout* humans, int humanCount, uint8_t botLevel, uint32_t seed);
MatchSetup buildMatch(GameMode mode, const ModeRules& rules, const Loadout* humans, int humanCount, uint8_t botLevel,
                      uint32_t seed);

const MapDef& mapOf(const MatchSetup& s);
bool carriesObjective(const MatchState& m, int fighter); // flag or ball carriers can't use their technique

constexpr int kBufferWindow = 8;
constexpr int16_t kRealmCastFrames = 60;
constexpr int16_t kRealmFrames = 600;
constexpr int16_t kClashFrames = 90;
constexpr int16_t kBurnoutFrames = 180;
constexpr int16_t kOpenRealmRadius = 10;
constexpr int16_t kRealmRadius = 18;

enum BaseMove : int16_t { kLight, kHeavy, kLauncher, kAirLight, kGrab, kBurst, kRealmCast, kBaseMoveCount };
constexpr int16_t kTechMoveOffset = 100;

const MoveDef& baseMove(int16_t id);
const MoveDef& moveOf(const MatchSetup& s, int fighter, int16_t moveId);

MatchState createMatch(const MatchSetup& setup);

int bondLevel(const FighterState& f);
bool isEnemy(const MatchState& m, int a, int b);
bool canHurt(const MatchState& m, const MatchSetup& s, int attacker, int victim);
const CompanionDef& companionDef(const MatchSetup& s, int owner, const CompanionState& c);
// One input per fighter. Bot slots are ignored, bots think inside the simulation.
void step(MatchState& m, const MatchSetup& setup, const FrameInput inputs[], EventSink* events = nullptr);

// Only reads the match state, so bots are deterministic and never need to be sent over the network.
FrameInput botInput(const MatchState& m, const MatchSetup& setup, int fighter);

} // namespace fc
