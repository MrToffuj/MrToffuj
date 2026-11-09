#pragma once
// Das "Skelett": Datenstrukturen, aus denen JEDER Charakter gebaut wird.
// Ein Charakter = CharacterDef (Daten) + Technique (stateless Logik).
#include <cstdint>
#include <vector>

#include "fightcore/Fixed.h"

namespace fc {

class Technique;

enum class MoveCategory : uint8_t { Normal = 0, Special = 1, Technique = 2, Ultimate = 3 };
constexpr uint8_t catBit(MoveCategory c) { return static_cast<uint8_t>(1u << static_cast<uint8_t>(c)); }

// Wie stark ein Treffer die Welt erschüttert -> steuert Zerstörung, Krater, Staub, Kamera-Shake.
enum class ImpactClass : uint8_t { Light, Medium, Heavy, Crushing };

struct HitProps {
    int16_t damage = 0;
    int16_t hitstun = 0;
    int16_t blockstun = 0;
    int16_t hitstop = 0;
    Fx knockback{};   // horizontal entlang der Blickrichtung des Angreifers
    Fx launch{};      // vertikal (> 0 = Launcher)
    int16_t essenceGainAttacker = 0;
    int16_t essenceGainDefender = 0;
    ImpactClass impact = ImpactClass::Light;
    bool unblockable = false;
};

// Kugel-Hitbox in Kämpfer-lokalen Koordinaten (x = rechts, y = oben, z = vorne).
// Aktiv während des Active-Fensters des Moves.
struct HitboxDef {
    Vec3 offset;
    Fx radius;
};

struct HurtboxDef {
    Vec3 offset;
    Fx radius;
};

struct MoveDef {
    const char* name = "";
    MoveCategory category = MoveCategory::Normal;
    // Frame-Daten @ 60 Hz. startup = Frames vor dem ersten aktiven Frame.
    int16_t startup = 1;
    int16_t active = 0;
    int16_t recovery = 0;
    std::vector<HitboxDef> hitboxes;
    HitProps hit;
    int16_t essenceCost = 0;
    // Vorwärtsbewegung während [moveFirst, moveLast]
    Fx forwardSpeed{};
    int16_t moveFirst = 0;
    int16_t moveLast = -1;
    // Auf welche Kategorien darf nach Treffer/Block gecancelt werden?
    uint8_t cancelMask = 0;
    // Frame, an dem Technique::onMoveEvent feuert (-1 = nie). Bei Ultimates öffnet sich hier der Realm.
    int16_t eventFrame = -1;
    // Unverwundbar in [invulnFirst, invulnLast]
    int16_t invulnFirst = 0;
    int16_t invulnLast = -1;

    int16_t total() const { return static_cast<int16_t>(startup + active + recovery); }
    bool isActiveFrame(int16_t f) const { return f >= startup && f < startup + active; }
};

// Eingabe -> Move. Wird in Reihenfolge geprüft, erster Treffer gewinnt.
struct CommandDef {
    uint8_t dir = 0;    // 0 = egal, sonst Numpad-Richtung
    uint8_t button = 0; // genau EIN Button aus fc::Button
    bool air = false;
    int16_t moveId = -1;
};

struct CharacterDef {
    const char* name = "";
    int16_t maxHealth = 1000;
    Fx walkSpeed, backSpeed, sideSpeed;
    Fx dashSpeed;
    int16_t dashFrames = 12;
    Fx jumpVelocity, gravity;
    Fx pushRadius;
    std::vector<HurtboxDef> hurtboxes;
    std::vector<MoveDef> moves;
    std::vector<CommandDef> commands;
    const Technique* technique = nullptr;
};

struct StageDef {
    Fx halfExtent = Fx::fromInt(12);  // Arena: [-h, h] auf X und Z
    int16_t wallHp = 300;             // Wände sind gameplay-relevant zerstörbar
    Fx wallImpactSpeed = Fx::ratio(1, 5); // Mindestgeschwindigkeit für Wand-Schaden
};

} // namespace fc
