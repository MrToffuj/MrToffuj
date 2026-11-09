#include "fightcore/Session.h"

namespace fc {

namespace {
bool sameInput(const FrameInput& a, const FrameInput& b) {
    return a.dir == b.dir && a.buttons == b.buttons && a.moveX == b.moveX && a.moveZ == b.moveZ;
}
} // namespace

Session::Session(const MatchSetup& setup) : setup_(setup), state_(createMatch(setup_)) {
    confirmedUpTo_.fill(-1);
    botFrom_.fill(INT_MAX);
}

bool Session::isBotControlled(int p, int32_t f) const { return setup_.loadout[p].bot || f >= botFrom_[p]; }

void Session::bumpConfirmed(int p) {
    while (true) {
        const int32_t g = confirmedUpTo_[p] + 1;
        const Slot& s = slot(g);
        if (s.frame != g || !s.confirmed[p]) return;
        confirmedUpTo_[p] = g;
    }
}

int32_t Session::confirmedFrame(int p) const { return confirmedUpTo_[p]; }

int32_t Session::confirmedAll() const {
    int32_t lowest = state_.frame - 1;
    for (int p = 0; p < setup_.count; ++p) {
        if (setup_.loadout[p].bot) continue;
        const int32_t c = botFrom_[p] != INT_MAX ? (confirmedUpTo_[p] >= botFrom_[p] - 1 ? INT_MAX : confirmedUpTo_[p]) : confirmedUpTo_[p];
        if (c < lowest) lowest = c;
    }
    return lowest;
}

bool Session::inputAt(int32_t f, int p, FrameInput& out) const {
    const Slot& s = slot(f);
    if (s.frame != f || !s.confirmed[p]) return false;
    out = s.input[p];
    return true;
}

void Session::takeOverByBot(int p, int32_t f) {
    if (f >= botFrom_[p]) return;
    botFrom_[p] = f;
    if (f < state_.frame && f < rollbackFrom_) rollbackFrom_ = f > state_.frame - kHistory + 1 ? f : state_.frame - kHistory + 1;
}

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
        bumpConfirmed(player);
        return true;
    }
    touch(f);
    slot(f).input[player] = in;
    slot(f).confirmed[player] = true;
    bumpConfirmed(player);
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
    FrameInput inputs[kMaxFighters];
    for (int p = 0; p < setup_.count; ++p) {
        if (f >= botFrom_[p]) {
            // a bot took over this slot: it locks on like every bot and thinks for itself
            state_.f[p].lockOn = true;
            inputs[p] = botInput(state_, setup_, p);
            continue;
        }
        if (!s.confirmed[p] && !setup_.loadout[p].bot) s.input[p] = predict(p, f);
        inputs[p] = s.input[p];
    }
    step(state_, setup_, inputs, events);
}

void Session::applyRollback() {
    if (rollbackFrom_ == INT_MAX) return;
    const int32_t from = rollbackFrom_;
    const int32_t to = state_.frame;
    rollbackFrom_ = INT_MAX;
    state_ = slot(from).state;
    for (int32_t f = from; f < to; ++f) simulate(f, nullptr);  // already shown
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
