#pragma once
// The whole match state. Keep it trivially copyable, rollback is a plain copy.
#include <array>
#include <cstdint>
#include <type_traits>

#include "fightcore/Fixed.h"
#include "fightcore/Input.h"

namespace fc {

constexpr int16_t kRealmMeterMax = 1000;
constexpr int kMaxFighters = 20;
constexpr int kMaxProjectiles = 64;
constexpr uint8_t kNoWinner = 255;
constexpr uint8_t kDraw = 254;
constexpr int kMaxTeams = kMaxFighters;
constexpr int kMaxObstacles = 96;
constexpr int kMaxFlags = 4;
constexpr int kMaxZones = 3;

enum class FState : uint8_t {
    Idle, Walk, Dash, Dodge, Air, Attack, Hitstun, Blockstun, Knockdown, Guard,
    Heal,  // Reverse Flow
    SimpleRealm,
    Locked,  // frozen by a Lock realm
    Clash,  // realm clash mash-off
};

struct CompanionState {
    int8_t roster = -1;  // index into MatchSetup::roster[owner], -1 = empty slot
    int8_t action = -1;  // AssistRole, -1 = following the owner
    bool link = false;
    bool hasHit = false;
    int16_t hp = 0;
    int16_t actionFrame = 0;
    int16_t cooldown = 0;
    int16_t down = 0;  // > 0: down, counting to revive
    int16_t lock = 0;
    int32_t expires = -1;  // temporary companions only
    int32_t since = 0;  // summon frame, the oldest gets replaced
    uint8_t adapted = 0;  // MoveCategory bits The Adaptor has adapted to
    Vec3 pos{};
    Vec3 facing{Fx{}, Fx{}, Fx::fromInt(1)};

    bool onField() const { return roster >= 0 && down == 0; }
};

// Realm Expansion. Everyone can have one up at the same time, each only works inside its radius.
struct RealmState {
    int16_t timer = 0;
    bool open = false;  // open realm: follows the owner, smaller radius
    Vec3 center{};  // closed realm stays where it was cast
    int8_t clashWith = -1;
    int16_t clashTimer = 0;
    int16_t clashCount = 0;

    bool active() const { return timer > 0 && clashTimer == 0; }
};

struct FighterState {
    Vec3 pos{}, vel{};
    Vec3 facing{Fx{}, Fx{}, Fx::fromInt(1)};
    int32_t ce = 0;  // CE in 1/100
    int32_t pressFrame = 0;  // frame the button for the current move was pressed
    int32_t linkFrame = -100;  // Black Spark timing: hitstop end of the last hit
    int32_t guardPressFrame = -100;
    int32_t oathTimer = -1;
    int16_t health = 0;
    int16_t realmMeter = 0;
    int16_t stateFrame = 0;
    int16_t moveId = -1;  // < 100 base move, >= 100 technique move (index + 100)
    int16_t moveFrame = 0;
    int16_t stun = 0;
    int16_t hitstop = 0;
    int16_t comboTaken = 0;
    int16_t burnout = 0;
    int16_t lock = 0;
    int16_t healed = 0;
    std::array<int16_t, 3> cooldown{};
    FState state = FState::Idle;
    uint8_t armorLeft = 0;
    bool moveHasHit = false;
    bool grounded = true;
    bool launched = false;
    bool reinforce = false;
    bool maxOutputUsed = false;
    InputBuffer input;
    std::array<CompanionState, 3> comp{};
    uint8_t lostMask = 0;  // roster entries gone for the round
    int16_t bond = 0;
    uint8_t team = 0;
    int8_t target = -1;  // lock-on
    bool manualTarget = false;  // picked by hand, don't auto switch
    bool lockOn = true;         // facing and movement follow the target
    int32_t spawnFrame = 0;
    bool out = false;
    int16_t respawn = 0;  // frames until respawn
    int16_t kos = 0;
    int16_t deaths = 0;
    int8_t lastHitBy = -1;
    RealmState realm{};
    std::array<int32_t, 8> tech{};  // free slots for the technique, keeps it rollback safe
};

struct Projectile {
    bool alive = false;
    uint8_t owner = 0;
    int8_t source = 0;  // 0 = technique, otherwise roster index + 1
    int16_t moveIndex = -1;
    int16_t life = 0;
    int16_t armDelay = 0;  // inactive for this many frames (telegraphed attacks)
    Vec3 pos{}, vel{};
    Fx radius{};
    bool beam = false;     // pos is the start, vel the direction, length how far it reaches
    Fx length{};
};

struct ObstacleState {
    int16_t hp = 0;
    bool broken = false;
};

struct FlagState {
    Vec3 pos{};
    int8_t carrier = -1;
    bool home = true;
    int16_t returnTimer = 0;
};

struct ZoneState {
    int8_t owner = -1;
    int8_t capturing = -1;
    int16_t progress = 0;
};

struct ObjectiveState {
    std::array<int16_t, kMaxTeams> score{};
    std::array<FlagState, kMaxFlags> flags{};
    std::array<ZoneState, kMaxZones> zones{};
    Vec3 ballPos{};
    int8_t ballCarrier = -1;
    int16_t ballHeld = 0;
};

struct MatchState {
    int32_t frame = 0;
    uint8_t count = 2;
    uint8_t winner = kNoWinner;  // winning team, kDraw or kNoWinner
    std::array<FighterState, kMaxFighters> f{};
    std::array<Projectile, kMaxProjectiles> proj{};
    std::array<ObstacleState, kMaxObstacles> obstacles{};
    ObjectiveState objective{};
};

static_assert(std::is_trivially_copyable_v<MatchState>, "MatchState muss kopierbar sein (Rollback)");

uint64_t hashState(const MatchState& m);

} // namespace fc
