#pragma once
#include "fightcore/Events.h"
#include "fightcore/State.h"
#include "fightcore/Technique.h"
#include "fightcore/Types.h"

namespace fc {

struct MatchSetup {
    const CharacterDef* chars[2] = {nullptr, nullptr};
    StageDef stage;
};

constexpr int kBufferWindow = 8;    // Input-Buffer in Frames (großzügig für Handhelds)
constexpr int16_t kRealmFrames = 600; // 10 s

MatchState createMatch(const MatchSetup& setup);

// Ein fester 60-Hz-Tick. Rein deterministisch: gleicher State + gleiche Inputs = gleiches Ergebnis.
void step(MatchState& m, const MatchSetup& setup, const FrameInput inputs[2], EventSink* events = nullptr);

} // namespace fc
