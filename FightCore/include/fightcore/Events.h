#pragma once
// Events = Brücke von der deterministischen Simulation zur Präsentation.
// Zerstörung, Krater, Staub, Feuer, Kamera-Shake reagieren NUR auf diese Events
// und laufen selbst NICHT im Rollback (kosmetisch, nicht deterministisch nötig).
#include <cstdint>
#include <vector>

#include "fightcore/Fixed.h"
#include "fightcore/Types.h"

namespace fc {

enum class ImpactKind : uint8_t {
    Hit,           // Treffer am Körper
    Block,
    GroundSlam,    // Aufschlag auf dem Boden -> Krater
    Landing,       // Landung nach Sprung -> Staubring
    DashDust,      // Dash/Sprung-Absprung -> Staubwolke
    WallSplat,     // Gegen Wand geknallt -> Risse
    WallBreak,     // Wand zerbricht (gameplay-relevant)
    ProjectileBurst,
    TechniqueFx,   // charakter-spezifischer Effekt (subtype in `tag`)
    RealmOpen,
    Ignite,        // Feuer-Quelle (z. B. Feuer-Technik) -> Holz fängt Feuer
};

struct ImpactEvent {
    int32_t frame = 0;
    ImpactKind kind = ImpactKind::Hit;
    uint8_t source = 0; // Spieler 0/1
    ImpactClass strength = ImpactClass::Light;
    int16_t tag = 0;
    Vec3 pos{}, dir{};
    Fx force{};
};

// Die Präsentation dedupliziert über (frame, kind, source, tag), da Rollback Frames neu simuliert.
using EventSink = std::vector<ImpactEvent>;

} // namespace fc
