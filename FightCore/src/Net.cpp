#include "fightcore/Net.h"

#include <algorithm>

#include "fightcore/Registry.h"

namespace fc::net {
namespace {

enum Msg : uint8_t { kHello = 1, kStart = 2, kInputs = 3, kHash = 4, kDrop = 5 };

struct Writer {
    std::vector<uint8_t> b;
    void u8(uint8_t v) { b.push_back(v); }
    void u16(uint16_t v) { u8(static_cast<uint8_t>(v)); u8(static_cast<uint8_t>(v >> 8)); }
    void u32(uint32_t v) { u16(static_cast<uint16_t>(v)); u16(static_cast<uint16_t>(v >> 16)); }
    void u64(uint64_t v) { u32(static_cast<uint32_t>(v)); u32(static_cast<uint32_t>(v >> 32)); }
    void str(const std::string& s) {
        const size_t n = s.size() < 255 ? s.size() : 255;
        u8(static_cast<uint8_t>(n));
        b.insert(b.end(), s.begin(), s.begin() + static_cast<long>(n));
    }
    void input(const FrameInput& in) {
        u8(in.dir);
        u16(in.buttons);
        u8(static_cast<uint8_t>(in.moveX));
        u8(static_cast<uint8_t>(in.moveZ));
    }
};

struct Reader {
    const std::vector<uint8_t>& b;
    size_t p;
    bool ok = true;
    uint8_t u8() {
        if (p >= b.size()) { ok = false; return 0; }
        return b[p++];
    }
    uint16_t u16() { const uint16_t lo = u8(); return static_cast<uint16_t>(lo | (u8() << 8)); }
    uint32_t u32() { const uint32_t lo = u16(); return lo | (static_cast<uint32_t>(u16()) << 16); }
    uint64_t u64() { const uint64_t lo = u32(); return lo | (static_cast<uint64_t>(u32()) << 32); }
    std::string str() {
        const size_t n = u8();
        if (p + n > b.size()) { ok = false; return {}; }
        std::string s(b.begin() + static_cast<long>(p), b.begin() + static_cast<long>(p + n));
        p += n;
        return s;
    }
    FrameInput input() {
        FrameInput in;
        in.dir = u8();
        in.buttons = u16();
        in.moveX = static_cast<int8_t>(u8());
        in.moveZ = static_cast<int8_t>(u8());
        return in;
    }
};

const BodyDef* bodyByName(const std::string& name) {
    for (const BodyDef* b : {&bodies::heavy(), &bodies::speedster(), &bodies::caster(), &bodies::brawler(), &bodies::technician(),
                             &bodies::summoner(), &bodies::heavensBind()})
        if (name == b->name) return b;
    return nullptr;
}

const CompanionDef* assistByName(const std::string& name) {
    for (const CompanionDef* c : {&companions::bladeWolf(), &companions::sparkOwl(), &companions::vineToad(), &companions::stoneWarden()})
        if (name == c->name) return c;
    return nullptr;
}

void writeOath(Writer& w, const OathChoice& o, const PlaystyleProfile& p) {
    w.u8(o.active ? 1 : 0);
    w.u8(static_cast<uint8_t>(o.category));
    w.u8(static_cast<uint8_t>(o.size));
    w.u8(o.detail);
    for (uint8_t u : p.skillUsage) w.u8(u);
    w.u8(p.blockRate);
    w.u8(p.realmUsage);
    w.u8(p.maxOutputUsage);
    w.u8(p.healUsage);
}

void readOath(Reader& r, OathChoice& o, PlaystyleProfile& p) {
    o.active = r.u8() != 0;
    o.category = static_cast<OathCategory>(r.u8());
    o.size = static_cast<OathSize>(r.u8());
    o.detail = r.u8();
    for (uint8_t& u : p.skillUsage) u = r.u8();
    p.blockRate = r.u8();
    p.realmUsage = r.u8();
    p.maxOutputUsage = r.u8();
    p.healUsage = r.u8();
}

void writeRequest(Writer& w, const PlayerRequest& r) {
    w.str(r.body);
    w.str(r.technique);
    w.str(r.assist);
    w.u8(r.team);
    writeOath(w, r.oath, r.profile);
    w.u8(static_cast<uint8_t>(r.cursed.size()));
    for (const std::string& c : r.cursed) w.str(c);
}

PlayerRequest readRequest(Reader& rd) {
    PlayerRequest r;
    r.body = rd.str();
    r.technique = rd.str();
    r.assist = rd.str();
    r.team = rd.u8();
    readOath(rd, r.oath, r.profile);
    const int n = rd.u8();
    for (int k = 0; k < n && rd.ok; ++k) r.cursed.push_back(rd.str());
    return r;
}

Loadout toLoadout(const PlayerRequest& r) {
    const Registry& reg = Registry::instance();
    Loadout l;
    l.body = bodyByName(r.body);
    if (!l.body) l.body = &bodies::brawler();
    l.technique = reg.technique(r.technique);
    if (l.body->has(PassiveHeavensBind)) l.technique = reg.technique("tool_arsenal");
    l.assist = assistByName(r.assist);
    l.team = r.team;
    l.oath = r.oath;
    l.profile = r.profile;
    l.lockOnAtStart = false;
    for (size_t k = 0; k < r.cursed.size() && k < static_cast<size_t>(kMaxCursedObjects); ++k) l.cursed[k] = reg.cursedObject(r.cursed[k]);
    return l;
}

void writeInputs(Writer& w, const Session& s, const std::vector<int>& slots, int32_t start, int32_t end) {
    const int32_t count = end >= start ? end - start + 1 : 0;
    w.u32(static_cast<uint32_t>(start));
    w.u8(static_cast<uint8_t>(count));
    w.u8(static_cast<uint8_t>(slots.size()));
    for (int32_t f = start; f < start + count; ++f)
        for (int slot : slots) {
            FrameInput in;
            s.inputAt(f, slot, in);
            w.input(in);
        }
}

} // namespace

// ---------------------------------------------------------------- lobby description

std::vector<uint8_t> encode(const LobbyConfig& l) {
    Writer w;
    w.u32(l.protocol);
    w.u64(l.contentHash);
    w.u8(l.mode);
    w.u8(l.fighters);
    w.u8(l.teamCount);
    w.u8(l.objective);
    w.u8(l.cursedObjectSlots);
    w.u8(l.friendlyFire ? 1 : 0);
    w.u16(static_cast<uint16_t>(l.respawnFrames));
    w.u16(static_cast<uint16_t>(l.scoreLimit));
    w.u32(static_cast<uint32_t>(l.timeLimitFrames));
    w.str(l.map);
    w.u32(l.seed);
    w.u8(l.inputDelay);
    w.u8(l.peerCount);
    w.u8(static_cast<uint8_t>(l.peerAddresses.size()));
    for (const std::string& a : l.peerAddresses) w.str(a);
    w.u8(static_cast<uint8_t>(l.slots.size()));
    for (const SlotConfig& s : l.slots) {
        w.u8(s.owner);
        w.str(s.body);
        w.str(s.technique);
        w.str(s.assist);
        w.u8(s.team);
        w.u8(s.botLevel);
        w.u8(s.lockOnAtStart ? 1 : 0);
        writeOath(w, s.oath, s.profile);
        w.u8(static_cast<uint8_t>(s.cursed.size()));
        for (const std::string& c : s.cursed) w.str(c);
    }
    return w.b;
}

bool decode(const std::vector<uint8_t>& data, size_t offset, LobbyConfig& l) {
    Reader r{data, offset};
    l = LobbyConfig{};
    l.protocol = r.u32();
    l.contentHash = r.u64();
    l.mode = r.u8();
    l.fighters = r.u8();
    l.teamCount = r.u8();
    l.objective = r.u8();
    l.cursedObjectSlots = r.u8();
    l.friendlyFire = r.u8() != 0;
    l.respawnFrames = static_cast<int16_t>(r.u16());
    l.scoreLimit = static_cast<int16_t>(r.u16());
    l.timeLimitFrames = static_cast<int32_t>(r.u32());
    l.map = r.str();
    l.seed = r.u32();
    l.inputDelay = r.u8();
    l.peerCount = r.u8();
    const int addresses = r.u8();
    for (int k = 0; k < addresses && r.ok; ++k) l.peerAddresses.push_back(r.str());
    const int slots = r.u8();
    for (int k = 0; k < slots && r.ok; ++k) {
        SlotConfig s;
        s.owner = r.u8();
        s.body = r.str();
        s.technique = r.str();
        s.assist = r.str();
        s.team = r.u8();
        s.botLevel = r.u8();
        s.lockOnAtStart = r.u8() != 0;
        readOath(r, s.oath, s.profile);
        const int cursed = r.u8();
        for (int c = 0; c < cursed && r.ok; ++c) s.cursed.push_back(r.str());
        l.slots.push_back(std::move(s));
    }
    return r.ok;
}

LobbyConfig describe(const MatchSetup& s, const std::vector<uint8_t>& owners, uint8_t peerCount, uint32_t seed, uint8_t inputDelay) {
    LobbyConfig l;
    l.contentHash = Registry::instance().contentHash();
    l.mode = static_cast<uint8_t>(s.mode);
    l.fighters = static_cast<uint8_t>(s.rules.fighters);
    l.teamCount = s.rules.teamCount;
    l.objective = static_cast<uint8_t>(s.rules.objective);
    l.cursedObjectSlots = s.rules.cursedObjectSlots;
    l.friendlyFire = s.rules.friendlyFire;
    l.respawnFrames = s.rules.respawnFrames;
    l.scoreLimit = s.rules.scoreLimit;
    l.timeLimitFrames = s.rules.timeLimitFrames;
    l.map = mapOf(s).id;
    l.seed = seed;
    l.inputDelay = inputDelay;
    l.peerCount = peerCount;
    for (int i = 0; i < s.count; ++i) {
        const Loadout& lo = s.loadout[i];
        SlotConfig slot;
        slot.owner = i < static_cast<int>(owners.size()) ? owners[i] : kNoPeer;
        slot.body = lo.body->name;
        slot.technique = lo.technique ? lo.technique->id : "";
        slot.assist = lo.assist ? lo.assist->name : "";
        slot.team = lo.team;
        slot.botLevel = lo.botLevel;
        slot.lockOnAtStart = lo.lockOnAtStart;
        slot.oath = lo.oath;
        slot.profile = lo.profile;
        for (const CursedObjectDef* c : lo.cursed)
            if (c) slot.cursed.emplace_back(c->id);
        l.slots.push_back(std::move(slot));
    }
    return l;
}

bool buildSetup(const LobbyConfig& l, MatchSetup& out, std::string* error) {
    auto fail = [&](const std::string& what) {
        if (error) *error = what;
        return false;
    };
    const Registry& reg = Registry::instance();
    if (l.protocol != kProtocolVersion) return fail("different game version");
    if (l.contentHash != reg.contentHash()) return fail("different mods installed");
    if (l.slots.empty() || l.slots.size() > static_cast<size_t>(kMaxFighters)) return fail("bad slot count");

    MatchSetup s;
    s.mode = static_cast<GameMode>(l.mode);
    s.rules.fighters = static_cast<int8_t>(l.fighters);
    s.rules.teamCount = l.teamCount;
    s.rules.objective = static_cast<Objective>(l.objective);
    s.rules.cursedObjectSlots = l.cursedObjectSlots;
    s.rules.friendlyFire = l.friendlyFire;
    s.rules.respawnFrames = l.respawnFrames;
    s.rules.scoreLimit = l.scoreLimit;
    s.rules.timeLimitFrames = l.timeLimitFrames;
    s.rules.map = reg.map(l.map);
    if (!s.rules.map) return fail("unknown map " + l.map);
    s.count = static_cast<int8_t>(l.slots.size());
    for (size_t i = 0; i < l.slots.size(); ++i) {
        const SlotConfig& slot = l.slots[i];
        Loadout& lo = s.loadout[i];
        lo.body = bodyByName(slot.body);
        if (!lo.body) return fail("unknown body " + slot.body);
        lo.technique = slot.technique.empty() ? nullptr : reg.technique(slot.technique);
        if (!slot.technique.empty() && !lo.technique) return fail("unknown technique " + slot.technique);
        lo.assist = slot.assist.empty() ? nullptr : assistByName(slot.assist);
        lo.team = slot.team;
        lo.bot = slot.owner == kNoPeer;
        lo.botLevel = slot.botLevel;
        lo.lockOnAtStart = slot.lockOnAtStart;
        lo.oath = slot.oath;
        lo.profile = slot.profile;
        for (size_t k = 0; k < slot.cursed.size() && k < static_cast<size_t>(kMaxCursedObjects); ++k)
            lo.cursed[k] = reg.cursedObject(slot.cursed[k]);
    }
    s.finalize();
    out = s;
    return true;
}

// ---------------------------------------------------------------- handshake

LobbyHost::LobbyHost(Transport& transport, std::vector<PlayerRequest> localPlayers) : transport_(transport) {
    peers_.push_back(std::move(localPlayers));
}

void LobbyHost::poll() {
    uint8_t from = 0;
    std::vector<uint8_t> packet;
    while (transport_.receive(from, packet)) {
        if (packet.empty() || packet[0] != kHello || from == 0) continue;
        Reader r{packet, 1};
        const uint32_t protocol = r.u32();
        const uint64_t hash = r.u64();
        if (protocol != kProtocolVersion) { rejection_ = "peer runs a different version"; continue; }
        if (hash != Registry::instance().contentHash()) { rejection_ = "peer has different mods"; continue; }
        std::vector<PlayerRequest> players;
        const int n = r.u8();
        for (int k = 0; k < n && r.ok; ++k) players.push_back(readRequest(r));
        if (!r.ok || players.empty()) continue;
        if (!startBody_.empty()) {
            // already running: this peer missed every Start copy
            if (from < peers_.size() && !peers_[from].empty()) sendStart(from);
            continue;
        }
        if (from >= peers_.size()) peers_.resize(from + 1);
        peers_[from] = std::move(players);
    }
}

int LobbyHost::peerCount() const {
    int n = 0;
    for (const auto& p : peers_) n += p.empty() ? 0 : 1;
    return n;
}

void LobbyHost::resendStart() {
    if (startBody_.empty()) return;
    for (size_t p = 1; p < peers_.size(); ++p)
        if (!peers_[p].empty()) sendStart(static_cast<uint8_t>(p));
}

void LobbyHost::sendStart(uint8_t peer) {
    std::vector<uint8_t> packet{kStart, peer};
    packet.insert(packet.end(), startBody_.begin(), startBody_.end());
    transport_.send(peer, packet);
}

LobbyConfig LobbyHost::start(GameMode mode, const ModeRules& rules, uint8_t botLevel, uint32_t seed, uint8_t inputDelay,
                             const std::vector<std::string>& peerAddresses) {
    std::vector<Loadout> humans;
    std::vector<uint8_t> owners;
    for (size_t p = 0; p < peers_.size(); ++p)
        for (const PlayerRequest& r : peers_[p]) {
            humans.push_back(toLoadout(r));
            owners.push_back(static_cast<uint8_t>(p));
        }
    const MatchSetup setup = buildMatch(mode, rules, humans.data(), static_cast<int>(humans.size()), botLevel, seed);
    owners.resize(static_cast<size_t>(setup.count), kNoPeer);
    LobbyConfig lobby = describe(setup, owners, static_cast<uint8_t>(peers_.size()), seed, inputDelay);
    lobby.peerAddresses = peerAddresses;
    startBody_ = encode(lobby);
    for (size_t p = 1; p < peers_.size(); ++p) {
        if (peers_[p].empty()) continue;
        // the start packet is important, send it a few times in case one gets lost
        for (int k = 0; k < 3; ++k) sendStart(static_cast<uint8_t>(p));
    }
    return lobby;
}

LobbyClient::LobbyClient(Transport& transport, std::vector<PlayerRequest> localPlayers)
    : transport_(transport), players_(std::move(localPlayers)) {}

bool LobbyClient::poll() {
    if (started_) return true;
    uint8_t from = 0;
    std::vector<uint8_t> packet;
    while (transport_.receive(from, packet)) {
        if (from != 0 || packet.size() < 2 || packet[0] != kStart) continue;
        LobbyConfig cfg;
        if (!decode(packet, 2, cfg)) continue;
        config_ = std::move(cfg);
        localPeer_ = packet[1];
        started_ = true;
        return true;
    }
    Writer w;
    w.u8(kHello);
    w.u32(kProtocolVersion);
    w.u64(Registry::instance().contentHash());
    w.u8(static_cast<uint8_t>(players_.size()));
    for (const PlayerRequest& r : players_) writeRequest(w, r);
    transport_.send(0, w.b);
    return false;
}

// ---------------------------------------------------------------- the match

PeerSession::PeerSession(const LobbyConfig& lobby, const MatchSetup& setup, uint8_t localPeer, Transport& transport, uint8_t round)
    : session_(setup), transport_(transport), localPeer_(localPeer), round_(round), inputDelay_(lobby.inputDelay) {
    remotes_.resize(lobby.peerCount);
    for (size_t i = 0; i < lobby.slots.size(); ++i) {
        const uint8_t owner = lobby.slots[i].owner;
        if (owner == kNoPeer) continue;
        if (owner == localPeer_) localSlots_.push_back(static_cast<int>(i));
        else if (owner < remotes_.size()) remotes_[owner].slots.push_back(static_cast<int>(i));
    }
    pendingLocal_.resize(localSlots_.size());
    // the first frames have no input yet, everyone agrees they're neutral
    for (int32_t f = 0; f < inputDelay_; ++f)
        for (int slot : localSlots_) session_.setInput(slot, f, FrameInput{});
    scheduled_ = inputDelay_ - 1;
}

void PeerSession::setLocalInput(int localIndex, const FrameInput& in) {
    if (localIndex >= 0 && localIndex < static_cast<int>(pendingLocal_.size())) pendingLocal_[static_cast<size_t>(localIndex)] = in;
}

const MatchState* PeerSession::confirmedState() {
    session_.applyRollback();
    return session_.stateAt(session_.confirmedAll());
}

int32_t PeerSession::confirmedOf(const std::vector<int>& slots) const {
    int32_t c = INT32_MAX;
    for (int slot : slots) c = std::min(c, session_.confirmedFrame(slot));
    return c == INT32_MAX ? -1 : c;
}

int PeerSession::advantageOver(uint8_t peer) const {
    return remotes_[peer].latestFrame < 0 ? 0 : frame() - (remotes_[peer].latestFrame - inputDelay_);
}

void PeerSession::tick(EventSink* events) {
    ++ticks_;
    receive();
    if (localPeer_ == 0) checkTimeouts();

    bool advance = frame() - session_.confirmedAll() < Session::kHistory - 4;
    // time sync: if we're clearly ahead of someone, wait a tick now and then so they catch up
    for (size_t r = 0; r < remotes_.size() && advance; ++r) {
        if (r == localPeer_ || remotes_[r].dropped || remotes_[r].slots.empty()) continue;
        const int skew = (advantageOver(static_cast<uint8_t>(r)) - remotes_[r].theirAdvantage) / 2;
        if (skew >= 2 && ticks_ % 3 == 0) advance = false;
    }
    stalled_ = !advance;
    if (advance) {
        const int32_t target = frame() + inputDelay_;
        if (target > scheduled_) {
            for (size_t k = 0; k < localSlots_.size(); ++k) session_.setInput(localSlots_[k], target, pendingLocal_[k]);
            scheduled_ = target;
        }
        session_.advance(events);
    }
    sendInputs();
    checkHashes();
}

void PeerSession::receive() {
    uint8_t from = 0;
    std::vector<uint8_t> packet;
    while (transport_.receive(from, packet)) {
        if (packet.size() < 2 || from >= remotes_.size() || from == localPeer_) continue;
        if (packet[1] != round_) continue; // left over from the previous round, or already the next one
        Remote& rm = remotes_[from];
        if (rm.dropped) continue;
        Reader r{packet, 2};
        switch (packet[0]) {
            case kInputs: {
                rm.lastHeardTick = ticks_;
                rm.ackOfMine = std::max(rm.ackOfMine, static_cast<int32_t>(r.u32()));
                const int32_t theirFrame = static_cast<int32_t>(r.u32());
                rm.theirAdvantage = static_cast<int16_t>(r.u16());
                const int32_t start = static_cast<int32_t>(r.u32());
                const int count = r.u8(), slots = r.u8();
                if (slots != static_cast<int>(rm.slots.size())) break;
                for (int k = 0; k < count && r.ok; ++k)
                    for (int s = 0; s < slots; ++s) {
                        const FrameInput in = r.input();
                        if (r.ok) session_.setInput(rm.slots[static_cast<size_t>(s)], start + k, in);
                    }
                if (r.ok) rm.latestFrame = std::max(rm.latestFrame, std::max(start + count - 1, theirFrame + inputDelay_ - 1));
                break;
            }
            case kHash: {
                rm.lastHeardTick = ticks_;
                const int32_t f = static_cast<int32_t>(r.u32());
                const uint64_t h = r.u64();
                if (!r.ok) break;
                bool compared = false;
                for (const auto& [frameNo, own] : ownHashes_)
                    if (frameNo == f) { compared = true; if (own != h) desynced_ = true; }
                if (!compared) remoteHashes_.emplace_back(f, h);
                break;
            }
            case kDrop: {
                const uint8_t peer = r.u8();
                const int32_t fromFrame = static_cast<int32_t>(r.u32());
                const int32_t start = static_cast<int32_t>(r.u32());
                const int count = r.u8(), slots = r.u8();
                if (!r.ok || peer >= remotes_.size() || peer == localPeer_) break;
                const std::vector<int>& dropped = remotes_[peer].slots;
                if (slots != static_cast<int>(dropped.size())) break;
                for (int k = 0; k < count && r.ok; ++k)
                    for (int s = 0; s < slots; ++s) {
                        const FrameInput in = r.input();
                        if (r.ok) session_.setInput(dropped[static_cast<size_t>(s)], start + k, in);
                    }
                dropPeer(peer, fromFrame, false);
                break;
            }
            default: break;
        }
    }
}

void PeerSession::sendInputs() {
    for (size_t p = 0; p < remotes_.size(); ++p) {
        const Remote& rm = remotes_[p];
        if (p == localPeer_ || rm.dropped || rm.slots.empty()) continue;
        Writer w;
        w.u8(kInputs);
        w.u8(round_);
        w.u32(static_cast<uint32_t>(confirmedOf(rm.slots)));
        w.u32(static_cast<uint32_t>(frame()));
        w.u16(static_cast<uint16_t>(advantageOver(static_cast<uint8_t>(p))));
        // everything they haven't acknowledged, so a lost packet costs nothing
        const int32_t start = std::max({rm.ackOfMine + 1, scheduled_ - (Session::kHistory - 8), 0});
        writeInputs(w, session_, localSlots_, start, scheduled_);
        transport_.send(static_cast<uint8_t>(p), w.b);
    }
}

void PeerSession::checkHashes() {
    const int32_t confirmed = session_.confirmedAll();
    const int32_t f = confirmed / kHashInterval * kHashInterval;
    if (f <= lastHashSent_ || f >= frame()) return;
    session_.applyRollback();
    const MatchState* s = session_.stateAt(f);
    if (!s) return;
    lastHashSent_ = f;
    const uint64_t h = hashState(*s);
    ownHashes_.emplace_back(f, h);
    if (ownHashes_.size() > 16) ownHashes_.erase(ownHashes_.begin());
    for (const auto& [frameNo, remote] : remoteHashes_)
        if (frameNo == f && remote != h) desynced_ = true;
    remoteHashes_.erase(std::remove_if(remoteHashes_.begin(), remoteHashes_.end(),
                                       [&](const auto& e) { return e.first <= f; }),
                        remoteHashes_.end());
    Writer w;
    w.u8(kHash);
    w.u8(round_);
    w.u32(static_cast<uint32_t>(f));
    w.u64(h);
    for (size_t p = 0; p < remotes_.size(); ++p)
        if (p != localPeer_ && !remotes_[p].dropped && !remotes_[p].slots.empty()) transport_.send(static_cast<uint8_t>(p), w.b);
}

void PeerSession::checkTimeouts() {
    for (size_t p = 1; p < remotes_.size(); ++p) {
        const Remote& rm = remotes_[p];
        if (rm.dropped || rm.slots.empty() || ticks_ - rm.lastHeardTick <= kTimeoutTicks) continue;
        dropPeer(static_cast<uint8_t>(p), confirmedOf(rm.slots) + 1, true);
    }
}

void PeerSession::dropPeer(uint8_t peer, int32_t fromFrame, bool announce) {
    Remote& rm = remotes_[peer];
    if (rm.dropped) return;
    rm.dropped = true;
    for (int slot : rm.slots) session_.takeOverByBot(slot, fromFrame);
    if (!announce) return;
    // everyone needs the leaver's last inputs up to the hand-over frame, send what we have
    Writer w;
    w.u8(kDrop);
    w.u8(round_);
    w.u8(peer);
    w.u32(static_cast<uint32_t>(fromFrame));
    const int32_t oldest = std::max({0, fromFrame - (Session::kHistory - 2), frame() - (Session::kHistory - 1)});
    writeInputs(w, session_, rm.slots, oldest, fromFrame - 1);
    for (size_t p = 0; p < remotes_.size(); ++p)
        if (p != localPeer_ && p != peer && !remotes_[p].dropped && !remotes_[p].slots.empty())
            for (int k = 0; k < 3; ++k) transport_.send(static_cast<uint8_t>(p), w.b);
}

} // namespace fc::net
