#pragma once
// Companions: one player, more bodies on the field, one CE pool.
// They only act when called with the assist button, there's no autopilot.
#include <array>
#include <cstdint>
#include <vector>

#include "fightcore/Types.h"

namespace fc {

enum class CompanionType : uint8_t { Striker, Zoner, Support, Guard };
// Picked by the stick when pressing assist: neutral attack, forward pressure, sideways anti-air, back protect
enum class AssistRole : uint8_t { Attack = 0, Pressure = 1, AntiAir = 2, Protect = 3 };
enum class AssistKind : uint8_t {
    Rush,  // run at the target and hit
    Shoot,  // fire a projectile
    Rise,  // rise up in front of the owner
    Pull,  // grab the target and drag it to the owner
    Shield,  // stand in front of the owner and eat hits
};

struct AssistAction {
    AssistKind kind = AssistKind::Rush;
    int16_t startup = 10, active = 4, recovery = 12;
    int16_t move = 0;  // index into CompanionDef::moves
    Fx speed = Fx::ratio(25, 100);
    int16_t total() const { return static_cast<int16_t>(startup + active + recovery); }
};

struct CompanionDef {
    const char* name = "";
    CompanionType type = CompanionType::Striker;
    int16_t maxHp = 200;
    int16_t upkeep = 4;  // CE drained per frame (1/100) while on the field
    int16_t cooldown = 180;
    int16_t reviveFrames = 900;  // summoner assist only
    int16_t reviveCost = 150;
    bool adaptive = false;  // takes less damage from attack types it has seen
    std::vector<MoveDef> moves;
    std::array<AssistAction, 4> actions;
};

constexpr int kRosterMax = 6;
constexpr int kCompanionSlots = 3;  // summoner assist + two technique companions
constexpr int kTechCompanionMax = 2;
constexpr int kCompanionDamagePct = 60;  // companion hits deal 60% of a comparable skill
constexpr int16_t kBondMax = 6;  // bond level 2 at 3 landed assists, level 3 at 6

namespace companions {
// summoner assists
const CompanionDef& bladeWolf();
const CompanionDef& sparkOwl();
const CompanionDef& vineToad();
const CompanionDef& stoneWarden();
// oath rewards
const CompanionDef& oathSpirit();
const CompanionDef& adaptor();
} // namespace companions

} // namespace fc
