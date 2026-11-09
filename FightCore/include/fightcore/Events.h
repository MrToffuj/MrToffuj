#pragma once
// Events = Brücke von der deterministischen Simulation zur Präsentation
// (Zerstörung, Krater, Staub, Feuer, Impact-Frames, Kamera). Kosmetik läuft NICHT im Rollback.
#include <cstdint>
#include <vector>

#include "fightcore/Fixed.h"
#include "fightcore/Types.h"

namespace fc {

enum class ImpactKind : uint8_t {
    Hit, Block, Parry, Nullified, BlackSpark, GuaranteedHit,
    GroundSlam, Landing, DashDust, WallSplat, WallBreak,
    ProjectileBurst, TechniqueFx, Burst,
    RealmOpen, RealmClash, RealmEnd,
    Ignite, Burnout,
    CompanionSummon, CompanionCall, CompanionDown, CompanionRevive, CompanionGone,
    KO, Respawn,
    BarrierHit, FlagTaken, FlagDropped, FlagReturned, FlagCaptured, BallTaken, BallDropped, ZoneCaptured,
};

struct ImpactEvent {
    int32_t frame = 0;
    ImpactKind kind = ImpactKind::Hit;
    uint8_t source = 0;
    ImpactClass strength = ImpactClass::Light;
    int16_t tag = 0;
    Vec3 pos{}, dir{};
    Fx force{};
};

// Präsentation dedupliziert über (frame, kind, source, tag), da Rollback Frames neu simuliert.
using EventSink = std::vector<ImpactEvent>;

} // namespace fc
