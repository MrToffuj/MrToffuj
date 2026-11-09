#pragma once
// Gesamter Spielzustand. MUSS trivially copyable bleiben -> Rollback = Kopie.
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
constexpr int kMaxObstacles = 24;
constexpr int kMaxFlags = 4;
constexpr int kMaxZones = 3;

enum class FState : uint8_t {
    Idle, Walk, Dash, Dodge, Air, Attack, Hitstun, Blockstun, Knockdown, Guard,
    Heal,   // Rückfluss
    SimpleRealm,   // Schutzkreis
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

// Realm (Domain-artige Ultimate). Jeder Kämpfer kann einen eigenen haben; Wirkung nur im Radius.
struct RealmState {
    int16_t timer = 0;      // > 0: aktiv
    bool open = false;      // offener Realm (Pakt-Unlock): folgt dem Besitzer, kleinerer Radius
    Vec3 center{};          // geschlossener Realm: fester Mittelpunkt
    int8_t clashWith = -1;  // Realm-Clash gegen diesen Kämpfer
    int16_t clashTimer = 0;
    int16_t clashCount = 0;

    bool active() const { return timer > 0 && clashTimer == 0; }
};

struct FighterState {
    Vec3 pos{}, vel{};
    Vec3 facing{Fx{}, Fx{}, Fx::fromInt(1)};
    int32_t ce = 0;              // Essenz in 1/100
    int32_t pressFrame = 0;      // Frame des Tastendrucks, der den aktuellen Move gestartet hat
    int32_t linkFrame = -100;    // perfekter Frame für Nullfunke (Ende des Hitstops des letzten Treffers)
    int32_t guardPressFrame = -100;
    int32_t oathTimer = -1;
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
    bool maxOutputUsed = false;
    InputBuffer input;
    std::array<CompanionState, 3> comp{};
    uint8_t lostMask = 0;        // Roster-Einträge, die für die Runde weg sind
    int16_t bond = 0;            // erfolgreiche Assists (Bond-Leiste)
    // Modus / Team / Ziel
    uint8_t team = 0;
    int8_t target = -1;          // Lock-on-Ziel
    bool manualTarget = false;   // Ziel per Taste gewählt -> kein Auto-Wechsel
    bool out = false;            // K.O.
    int16_t respawn = 0;         // Brawl: Frames bis Respawn
    int16_t kos = 0;
    int16_t deaths = 0;
    int8_t lastHitBy = -1;
    RealmState realm{};
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
    uint8_t count = 2; // aktive Kämpfer
    uint8_t winner = kNoWinner; // Team-ID des Siegers, kDraw, kNoWinner = läuft
    std::array<FighterState, kMaxFighters> f{};
    std::array<Projectile, kMaxProjectiles> proj{};
    std::array<ObstacleState, kMaxObstacles> obstacles{};
    ObjectiveState objective{};
};

static_assert(std::is_trivially_copyable_v<MatchState>, "MatchState muss kopierbar sein (Rollback)");

uint64_t hashState(const MatchState& m);

} // namespace fc
