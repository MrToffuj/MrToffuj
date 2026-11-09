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

struct CompanionState {
    int8_t roster = -1;      // Index in MatchSetup::roster[owner], -1 = Slot leer
    int8_t action = -1;      // AssistRole oder -1 (folgt dem Spieler)
    bool link = false;       // Link-Attack (Bond-Stufe 2)
    bool hasHit = false;
    int16_t hp = 0;
    int16_t actionFrame = 0;
    int16_t cooldown = 0;
    int16_t down = 0;        // > 0: ausgefallen, Revive-Countdown
    int16_t lock = 0;
    int32_t expires = -1;    // temporäre Begleiter: Frames bis sie verschwinden
    int32_t since = 0;       // Beschwörungs-Frame ("ältester wird ersetzt")
    uint8_t adapted = 0;     // Adaption: Bitmaske der MoveCategories
    Vec3 pos{};
    Vec3 facing{Fx{}, Fx{}, Fx::fromInt(1)};

    bool onField() const { return roster >= 0 && down == 0; }
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
    std::array<CompanionState, 3> comp{};
    uint8_t lostMask = 0;        // Roster-Einträge, die für die Runde weg sind
    int16_t bond = 0;            // erfolgreiche Assists (Bond-Leiste)
    std::array<int32_t, 8> tech{}; // Technik-Zustand
};

struct Projectile {
    bool alive = false;
    uint8_t owner = 0;
    int8_t source = 0;      // 0 = Technik, sonst Roster-Index + 1 (Begleiter)
    int16_t moveIndex = -1; // Move-Index in Technik bzw. Begleiter (HitProps)
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
