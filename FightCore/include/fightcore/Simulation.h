#pragma once
#include "fightcore/Body.h"
#include "fightcore/Events.h"
#include "fightcore/Pact.h"
#include "fightcore/State.h"
#include "fightcore/Technique.h"
#include "fightcore/Types.h"

namespace fc {

// Ein Build = Body + Technik (+ optionaler Pakt). Technik = nullptr nur beim Nullgeborenen.
struct Loadout {
    const BodyDef* body = nullptr;
    const TechniqueDef* technique = nullptr;
    PactChoice pact;
    PlaystyleProfile profile;
};

struct MatchSetup {
    Loadout loadout[2];
    StageDef stage;

    // Von finalize() berechnet:
    BodyStats stats[2];
    PactResult pact[2];
    void finalize();
};

constexpr int kBufferWindow = 8;
constexpr int16_t kRealmCastFrames = 60;  // für alle gleich
constexpr int16_t kRealmFrames = 600;     // 10 s, für alle gleich
constexpr int16_t kClashFrames = 90;
constexpr int16_t kBurnoutFrames = 180;   // 3 s
constexpr int16_t kOpenRealmRadius = 10;

// Basis-Moves, die JEDER Build hat
enum BaseMove : int16_t { kLight, kHeavy, kLauncher, kAirLight, kGrab, kBurst, kRealmCast, kBaseMoveCount };
constexpr int16_t kTechMoveOffset = 100;

const MoveDef& baseMove(int16_t id);
const MoveDef& moveOf(const MatchSetup& s, int fighter, int16_t moveId);

MatchState createMatch(const MatchSetup& setup);
void step(MatchState& m, const MatchSetup& setup, const FrameInput inputs[2], EventSink* events = nullptr);

} // namespace fc
