#pragma once
// Peer to peer netcode on top of the rollback session. No servers: every peer simulates the whole
// match and only inputs travel over the wire.
//
//  - Transport is whatever moves bytes (UDP sockets, EOS P2P, a fake network in tests).
//  - LobbyConfig describes a match by ids only, so every peer builds the identical MatchSetup.
//  - LobbyHost / LobbyClient do the handshake (version and mod check, slot assignment).
//  - PeerSession runs the match: input delay, redundant input packets with acks, time sync,
//    desync detection through state hashes, and bots taking over players that drop.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "fightcore/Session.h"

namespace fc::net {

constexpr uint32_t kProtocolVersion = 1;
constexpr uint8_t kNoPeer = 255;
constexpr int kMaxPeers = kMaxFighters;

class Transport {
public:
    virtual ~Transport() = default;
    virtual void send(uint8_t peer, const std::vector<uint8_t>& packet) = 0;
    // Returns false when nothing is waiting.
    virtual bool receive(uint8_t& peer, std::vector<uint8_t>& packet) = 0;
};

// ---------------------------------------------------------------- lobby description

struct SlotConfig {
    uint8_t owner = kNoPeer; // peer that plays this slot, kNoPeer = bot
    std::string body, technique, assist;
    uint8_t team = 0;
    uint8_t botLevel = 1;
    bool lockOnAtStart = false;
    OathChoice oath;
    PlaystyleProfile profile;
    std::vector<std::string> cursed;
};

struct LobbyConfig {
    uint32_t protocol = kProtocolVersion;
    uint64_t contentHash = 0; // Registry::contentHash(), everyone needs the same mods
    uint8_t mode = 0;         // GameMode
    // rules, sent in full so custom lobbies work
    uint8_t fighters = 2, teamCount = 0, objective = 1, cursedObjectSlots = 0;
    bool friendlyFire = false;
    int16_t respawnFrames = -1, scoreLimit = 0;
    int32_t timeLimitFrames = -1;
    std::string map;
    uint32_t seed = 0;
    uint8_t inputDelay = 2;
    uint8_t peerCount = 1;
    std::vector<std::string> peerAddresses; // filled by the transport layer of the host, index = peer id
    std::vector<SlotConfig> slots;
};

std::vector<uint8_t> encode(const LobbyConfig& lobby);
bool decode(const std::vector<uint8_t>& data, size_t offset, LobbyConfig& out);

// Describes an existing setup (e.g. from buildMatch) by ids. owners[i] = peer playing slot i.
LobbyConfig describe(const MatchSetup& setup, const std::vector<uint8_t>& owners, uint8_t peerCount, uint32_t seed,
                     uint8_t inputDelay);
bool buildSetup(const LobbyConfig& lobby, MatchSetup& out, std::string* error = nullptr);

// ---------------------------------------------------------------- handshake

struct PlayerRequest { // what a player wants to play, sent in Hello
    std::string body, technique, assist;
    uint8_t team = 0;
    OathChoice oath;
    PlaystyleProfile profile; // from the player's save, drives adaptive oaths
    std::vector<std::string> cursed;
};

class LobbyHost {
public:
    LobbyHost(Transport& transport, std::vector<PlayerRequest> localPlayers);
    // Call regularly, also after start(): late Hellos get the Start packet again.
    void poll();
    // Peers that joined so far, host included.
    int peerCount() const;
    // Fixes the lobby and tells everyone. Bots fill the free slots.
    LobbyConfig start(GameMode mode, const ModeRules& rules, uint8_t botLevel, uint32_t seed, uint8_t inputDelay,
                      const std::vector<std::string>& peerAddresses = {});
    // Once the match runs, the PeerSession reads the socket and late Hellos go unanswered.
    // Call this now and then during the first seconds instead.
    void resendStart();
    const std::string& lastRejection() const { return rejection_; }

private:
    Transport& transport_;
    std::vector<std::vector<PlayerRequest>> peers_; // index = peer id
    std::vector<uint8_t> startBody_;                // encoded lobby once started
    std::string rejection_;
    void sendStart(uint8_t peer);
};

class LobbyClient {
public:
    LobbyClient(Transport& transport, std::vector<PlayerRequest> localPlayers);
    // Sends Hello until the host answers with Start. Returns true once started.
    bool poll();
    const LobbyConfig& config() const { return config_; }
    uint8_t localPeer() const { return localPeer_; }

private:
    Transport& transport_;
    std::vector<PlayerRequest> players_;
    LobbyConfig config_;
    uint8_t localPeer_ = kNoPeer;
    bool started_ = false;
};

// ---------------------------------------------------------------- the match

class PeerSession {
public:
    // round counts rematches in the same lobby, packets from another round are ignored
    PeerSession(const LobbyConfig& lobby, const MatchSetup& setup, uint8_t localPeer, Transport& transport, uint8_t round = 0);

    // Input of one of this peer's own players (index among them) for this tick.
    void setLocalInput(int localIndex, const FrameInput& in);
    // Receive, maybe advance one frame, send. Call once per 60 Hz tick.
    void tick(EventSink* events = nullptr);

    const Session& session() const { return session_; }
    // Newest state built only from confirmed inputs: the same on every peer, never rolled back.
    // Use it for anything that must not flicker, like ending the match.
    const MatchState* confirmedState();
    uint8_t round() const { return round_; }
    int32_t frame() const { return session_.frame(); }
    bool stalled() const { return stalled_; }
    bool desynced() const { return desynced_; }
    int localPlayerCount() const { return static_cast<int>(localSlots_.size()); }
    const std::vector<int>& localSlots() const { return localSlots_; }
    bool peerDropped(uint8_t peer) const { return peer < remotes_.size() && remotes_[peer].dropped; }

    static constexpr int kTimeoutTicks = 180; // 3 s of silence and the host hands the slot to a bot
    static constexpr int kHashInterval = 60;

private:
    struct Remote {
        std::vector<int> slots;
        int32_t lastHeardTick = 0;
        int32_t latestFrame = -1;  // highest frame index we got from them
        int32_t ackOfMine = -1;    // highest frame of ours they have
        int32_t theirAdvantage = 0;
        bool dropped = false;
    };

    void receive();
    void sendInputs();
    void checkHashes();
    void checkTimeouts();
    void dropPeer(uint8_t peer, int32_t fromFrame, bool announce);
    int32_t confirmedOf(const std::vector<int>& slots) const;
    int advantageOver(uint8_t peer) const;

    Session session_;
    Transport& transport_;
    uint8_t localPeer_;
    uint8_t round_;
    int inputDelay_;
    std::vector<int> localSlots_;
    std::vector<FrameInput> pendingLocal_;
    std::vector<Remote> remotes_;
    int32_t ticks_ = 0;
    int32_t scheduled_ = -1; // last frame our own inputs are known for
    bool stalled_ = false;
    bool desynced_ = false;
    int32_t lastHashSent_ = 0;
    std::vector<std::pair<int32_t, uint64_t>> ownHashes_;
    std::vector<std::pair<int32_t, uint64_t>> remoteHashes_;
};

} // namespace fc::net
