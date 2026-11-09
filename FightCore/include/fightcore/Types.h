#pragma once
#include <cstdint>
#include <vector>

#include "fightcore/Fixed.h"

namespace fc {

enum class MoveCategory : uint8_t { Normal = 0, Grab = 1, Skill = 2, MaxOutput = 3, Realm = 4, Burst = 5 };
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
    Vec3 offset;  // local space: x right, y up, z forward
    Fx radius;
};

struct MoveDef {
    const char* name = "";
    MoveCategory category = MoveCategory::Normal;
    int16_t startup = 1;  // frames before the first active frame
    int16_t active = 0;
    int16_t recovery = 0;
    std::vector<HitboxDef> hitboxes;
    HitProps hit;
    Fx forwardSpeed{};
    int16_t moveFirst = 0;
    int16_t moveLast = -1;
    uint8_t cancelMask = 0;  // categories this move can cancel into on hit or block
    int16_t eventFrame = -1;  // fires TechniqueLogic::onMoveEvent (or opens the realm)
    int16_t invulnFirst = 0;
    int16_t invulnLast = -1;
    bool armored = false;  // only matters for bodies with super armor
    bool usableInStun = false;
    bool isCounter = false;  // counter hits can't be countered again

    int16_t total() const { return static_cast<int16_t>(startup + active + recovery); }
    bool isActiveFrame(int16_t f) const { return f >= startup && f < startup + active; }
};

// A colony: a circular area sealed off by the barrier. The barrier itself is indestructible,
// everything inside it (buildings, pillars) can be wrecked.
struct ObstacleDef {
    Vec3 center;
    Fx halfX, halfZ;
    Fx height;
    int16_t hp = 250;
};

struct MapDef {
    const char* name = "";
    Fx radius = Fx::fromInt(14);
    std::vector<ObstacleDef> obstacles;
    std::vector<Vec3> zones; // capture points for the Checkpoints objective
    Fx impactSpeed = Fx::ratio(1, 5); // min. speed for a body to damage scenery
};

} // namespace fc
