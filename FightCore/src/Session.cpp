#include "fightcore/Session.h"

namespace fc {

namespace {
bool sameInput(const FrameInput& a, const FrameInput& b) { return a.dir == b.dir && a.buttons == b.buttons; }
} // namespace

Session::Session(const MatchSetup& setup) : setup_(setup), state_(createMatch(setup_)) {}

void Session::touch(int32_t f) {
    Slot& s = slot(f);
    if (s.frame == f) return;
    s = Slot{};
    s.frame = f;
}

bool Session::setInput(int player, int32_t f, FrameInput in) {
    if (player < 0 || player >= setup_.count) return false;
    if (f < 0 || f <= state_.frame - kHistory || f >= state_.frame + kHistory) return false;
    in.consumed = 0;
    if (f < state_.frame) {
        Slot& s = slot(f);
        if (s.frame != f) return false;
        if (!s.confirmed[player] && !sameInput(s.input[player], in) && f < rollbackFrom_) rollbackFrom_ = f;
        s.input[player] = in;
        s.confirmed[player] = true;
        return true;
    }
    touch(f);
    slot(f).input[player] = in;
    slot(f).confirmed[player] = true;
    return true;
}

FrameInput Session::predict(int player, int32_t f) const {
    for (int32_t g = f - 1; g >= 0 && g > f - kHistory; --g) {
        const Slot& s = slot(g);
        if (s.frame == g && s.confirmed[player]) return s.input[player];
    }
    return FrameInput{};
}

void Session::simulate(int32_t f, EventSink* events) {
    Slot& s = slot(f);
    s.state = state_;
    for (int p = 0; p < setup_.count; ++p)
        if (!s.confirmed[p] && !setup_.loadout[p].bot) s.input[p] = predict(p, f);
    step(state_, setup_, s.input, events);
}

void Session::applyRollback() {
    if (rollbackFrom_ == INT_MAX) return;
    const int32_t from = rollbackFrom_;
    const int32_t to = state_.frame;
    rollbackFrom_ = INT_MAX;
    state_ = slot(from).state;
    for (int32_t f = from; f < to; ++f) simulate(f, nullptr); // Events schon gezeigt -> verwerfen
    ++rollbacks_;
    lastDepth_ = to - from;
}

void Session::advance(EventSink* events) {
    applyRollback();
    touch(state_.frame);
    simulate(state_.frame, events);
}

const MatchState* Session::stateAt(int32_t f) const {
    const Slot& s = slot(f);
    return (s.frame == f && f < state_.frame) ? &s.state : nullptr;
}

} // namespace fc
