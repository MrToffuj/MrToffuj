#pragma once
// Gesamter Spielzustand. MUSS trivially copyable bleiben -> Rollback = Kopie.
#include <array>
#include <cstdint>
#include <type_traits>

#include "fightcore/Fixed.h"
#include "fightcore/Input.h"

namespace fc {

constexpr int16_t kRealmMeterMax = 1000;

enum class FState : uint8_t {
    Idle, Walk, Dash, Dodge, Air, Attack, Hitstun, Blockstun, Knockdown, Guard,
    Heal,   // Rückfluss
    Ward,   // Schutzkreis
    Locked, // vom Realm festgesetzt
    Clash,  // Realm-Clash (Input-Duell)
};

struct FighterState {
    Vec3 pos{}, vel{};
    Vec3 facing{Fx{}, Fx{}, Fx::fromInt(1)};
    int32_t ce = 0;              // Essenz in 1/100
    int32_t pressFrame = 0;      // Frame des Tastendrucks, der den aktuellen Move gestartet hat
    int32_t linkFrame = -100;    // perfekter Frame für Nullfunke (Ende des Hitstops des letzten Treffers)
    int32_t guardPressFrame = -100;
    int32_t pactTimer = -1;
    int16_t health = 0;
    int16_t realmMeter = 0;
    int16_t stateFrame = 0;
    int16_t moveId = -1;         // < 100 Basis-Move, >= 100 Technik-Move (Index + 100)
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
    bool apexUsed = false;
    InputBuffer input;
    std::array<int32_t, 8> tech{}; // Technik-Zustand
};

struct Projectile {
    bool alive = false;
    uint8_t owner = 0;
    int16_t moveIndex = -1; // Technik-Move-Index (HitProps)
    int16_t life = 0;
    int16_t armDelay = 0;   // Frames bis aktiv (z. B. Säule aus dem Boden)
    Vec3 pos{}, vel{};
    Fx radius{};
};

struct WallState {
    int16_t hp = 0;
    bool broken = false;
};

struct RealmState {
    int8_t owner = -1;
    int8_t challenger = -1;
    bool open = false;     // offener Realm (Pakt-Unlock)
    int16_t timer = 0;
    int16_t clashTimer = 0;
    std::array<int16_t, 2> clashCount{};
};

struct MatchState {
    int32_t frame = 0;
    std::array<FighterState, 2> f{};
    std::array<Projectile, 16> proj{};
    std::array<WallState, 4> walls{};
    RealmState realm{};
    uint8_t winner = 255; // 255 läuft, 0/1 Sieger, 2 Unentschieden
};

static_assert(std::is_trivially_copyable_v<MatchState>, "MatchState muss kopierbar sein (Rollback)");

uint64_t hashState(const MatchState& m);

} // namespace fc
