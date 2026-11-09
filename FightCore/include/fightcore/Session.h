#pragma once
// Rollback-Session (GGPO-Prinzip), engine-unabhängig.
//  - Lokale Inputs werden sofort bestätigt.
//  - Fehlende Remote-Inputs werden vorhergesagt (letzter bekannter Input wird wiederholt).
//  - Kommt ein Remote-Input an, der von der Vorhersage abweicht: Zustand des Frames laden und
//    bis zum aktuellen Frame neu simulieren.
// Events aus neu simulierten Frames werden verworfen (sie wurden schon gezeigt) -> Präsentation
// muss Fehlvorhersagen tolerieren (kosmetisch).
#include <array>
#include <climits>
#include <cstdint>

#include "fightcore/Simulation.h"

namespace fc {

class Session {
public:
    static constexpr int kHistory = 32; // max. Rollback-Fenster in Frames

    explicit Session(const MatchSetup& setup);

    const MatchSetup& setup() const { return setup_; }
    const MatchState& state() const { return state_; }
    int32_t frame() const { return state_.frame; }
    int rollbackCount() const { return rollbacks_; }
    int lastRollbackDepth() const { return lastDepth_; }

    // Bestätigter Input für (player, frame). Frame darf in der Vergangenheit liegen (Remote, verspätet),
    // aber höchstens kHistory - 1 Frames zurück.
    bool setInput(int player, int32_t frame, FrameInput in);

    // Offene Korrekturen sofort anwenden (ohne vorzurücken).
    void applyRollback();

    // Einen Frame simulieren. Fehlende Inputs werden vorhergesagt.
    void advance(EventSink* events = nullptr);

    // Zustand eines vergangenen Frames (vor dessen Simulation), nullptr falls nicht mehr im Verlauf.
    const MatchState* stateAt(int32_t frame) const;

private:
    struct Slot {
        int32_t frame = -1;
        MatchState state{};
        FrameInput input[2]{};
        bool confirmed[2] = {false, false};
    };

    Slot& slot(int32_t f) { return history_[static_cast<size_t>(f % kHistory)]; }
    const Slot& slot(int32_t f) const { return history_[static_cast<size_t>(f % kHistory)]; }
    void touch(int32_t f);
    FrameInput predict(int player, int32_t f) const;
    void simulate(int32_t f, EventSink* events);

    MatchSetup setup_;
    MatchState state_;
    std::array<Slot, kHistory> history_{};
    int32_t rollbackFrom_ = INT_MAX;
    int rollbacks_ = 0;
    int lastDepth_ = 0;
};

} // namespace fc
