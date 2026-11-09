#pragma once
// Gesamter Spielzustand. MUSS trivially copyable bleiben -> Rollback = memcpy.
#include <array>
#include <cstdint>
#include <type_traits>

#include "fightcore/Fixed.h"
#include "fightcore/Input.h"
#include "fightcore/Types.h"

namespace fc {

constexpr int16_t kEssenceMax = 1000;

enum class FState : uint8_t { Idle, Walk, Dash, Air, Attack, Hitstun, Blockstun, Knockdown, Guard };

struct FighterState {
    Vec3 pos{}, vel{};
    Vec3 facing{Fx{}, Fx{}, Fx::fromInt(1)};
    int16_t health = 0;
    int16_t essence = 0;
    FState state = FState::Idle;
    int16_t stateFrame = 0;
    int16_t moveId = -1;
    int16_t moveFrame = 0;
    int16_t stun = 0;
    int16_t hitstop = 0;
    int16_t comboTaken = 0; // Treffer in Folge, die DIESER Kämpfer kassiert (Damage-Scaling)
    bool moveHasHit = false;
    bool grounded = true;
    bool launched = false;
    InputBuffer input;
    // Freie Variablen für die Technik des Charakters (Timer, Positionen, Stacks...).
    // Techniken speichern ihren Zustand NUR hier -> automatisch rollback-fähig.
    std::array<int32_t, 8> tech{};
};

struct Projectile {
    bool alive = false;
    uint8_t owner = 0;
    int16_t moveId = -1; // HitProps kommen aus diesem Move des Besitzers
    int16_t life = 0;
    Vec3 pos{}, vel{};
    Fx radius{};
};

struct WallState {
    int16_t hp = 0;
    bool broken = false;
};

// Realm = domain-artige Ultimate-Zone. Nur einer gleichzeitig.
struct RealmState {
    int8_t owner = -1;
    int16_t timer = 0;
};

struct MatchState {
    int32_t frame = 0;
    std::array<FighterState, 2> f{};
    std::array<Projectile, 16> proj{};
    std::array<WallState, 4> walls{}; // +X, -X, +Z, -Z
    RealmState realm{};
    uint8_t winner = 255; // 255 = läuft, 0/1 = Sieger, 2 = Unentschieden
};

static_assert(std::is_trivially_copyable_v<MatchState>, "MatchState muss memcpy-bar sein (Rollback)");

uint64_t hashState(const MatchState& m);

} // namespace fc
