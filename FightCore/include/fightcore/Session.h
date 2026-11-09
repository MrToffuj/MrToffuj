#pragma once
// GGPO style rollback.
// Missing remote inputs are predicted by repeating the last known one. When the real input
// shows up and differs, we load that frame and resimulate up to now.
// Events from resimulated frames are dropped since they were already shown.
#include <array>
#include <climits>
#include <cstdint>

#include "fightcore/Simulation.h"

namespace fc {

class Session {
public:
    static constexpr int kHistory = 32;  // max rollback window in frames

    explicit Session(const MatchSetup& setup);

    const MatchSetup& setup() const { return setup_; }
    const MatchState& state() const { return state_; }
    int32_t frame() const { return state_.frame; }
    int rollbackCount() const { return rollbacks_; }
    int lastRollbackDepth() const { return lastDepth_; }

    // Confirmed input for (player, frame). Late remote inputs are fine as long as they're
    // within the history window.
    bool setInput(int player, int32_t frame, FrameInput in);

    void applyRollback();

    void advance(EventSink* events = nullptr);

    // State at the start of a past frame, or nullptr if it fell out of the history.
    const MatchState* stateAt(int32_t frame) const;

    // Last frame up to which every input of this player is known (-1 = none yet).
    int32_t confirmedFrame(int player) const;
    // Last frame for which every human input is known, i.e. nothing before it can change anymore.
    int32_t confirmedAll() const;
    bool inputAt(int32_t frame, int player, FrameInput& out) const;
    // A bot plays this slot from `frame` on (player left). All peers must call this with the same frame.
    void takeOverByBot(int player, int32_t frame);
    bool isBotControlled(int player, int32_t frame) const;

private:
    struct Slot {
        int32_t frame = -1;
        MatchState state{};
        FrameInput input[kMaxFighters]{};
        bool confirmed[kMaxFighters] = {};
    };

    Slot& slot(int32_t f) { return history_[static_cast<size_t>(f % kHistory)]; }
    const Slot& slot(int32_t f) const { return history_[static_cast<size_t>(f % kHistory)]; }
    void touch(int32_t f);
    FrameInput predict(int player, int32_t f) const;
    void simulate(int32_t f, EventSink* events);

    MatchSetup setup_;
    MatchState state_;
    std::array<Slot, kHistory> history_{};
    void bumpConfirmed(int player);

    int32_t rollbackFrom_ = INT_MAX;
    std::array<int32_t, kMaxFighters> confirmedUpTo_{};
    std::array<int32_t, kMaxFighters> botFrom_{};
    int rollbacks_ = 0;
    int lastDepth_ = 0;
};

} // namespace fc
