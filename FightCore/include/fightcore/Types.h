#pragma once
// Move-/Treffer-Daten. Gemeinsam für Basis-Moves (alle Bodies) und Technik-Moves.
#include <cstdint>
#include <vector>

#include "fightcore/Fixed.h"

namespace fc {

enum class MoveCategory : uint8_t { Normal = 0, Grab = 1, Skill = 2, Apex = 3, Realm = 4, Burst = 5 };
constexpr uint8_t catBit(MoveCategory c) { return static_cast<uint8_t>(1u << static_cast<uint8_t>(c)); }

enum class ImpactClass : uint8_t { Light, Medium, Heavy, Crushing };

struct HitProps {
    int16_t damage = 0;
    int16_t hitstun = 0;
    int16_t blockstun = 0;
    int16_t hitstop = 0;
    Fx knockback{};
    Fx launch{};
    ImpactClass impact = ImpactClass::Light;
    bool unblockable = false;
};

struct HitboxDef {
    Vec3 offset; // lokal: x rechts, y oben, z vorne
    Fx radius;
};

struct MoveDef {
    const char* name = "";
    MoveCategory category = MoveCategory::Normal;
    int16_t startup = 1; // Frames vor dem ersten aktiven Frame
    int16_t active = 0;
    int16_t recovery = 0;
    std::vector<HitboxDef> hitboxes;
    HitProps hit;
    Fx forwardSpeed{};
    int16_t moveFirst = 0;
    int16_t moveLast = -1;
    uint8_t cancelMask = 0;   // erlaubte Cancel-Kategorien nach Treffer/Block
    int16_t eventFrame = -1;  // Technik-Event / Realm-Öffnung
    int16_t invulnFirst = 0;
    int16_t invulnLast = -1;
    bool armored = false;     // Super Armor (nur wenn Body-Passiv es erlaubt)
    bool usableInStun = false;// z. B. Essenz-Burst

    int16_t total() const { return static_cast<int16_t>(startup + active + recovery); }
    bool isActiveFrame(int16_t f) const { return f >= startup && f < startup + active; }
};

struct StageDef {
    Fx halfExtent = Fx::fromInt(12);
    int16_t wallHp = 300;
    Fx wallImpactSpeed = Fx::ratio(1, 5);
};

} // namespace fc
