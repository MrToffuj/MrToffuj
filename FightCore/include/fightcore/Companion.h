#pragma once
// Begleiter-System: ein Spieler, zwei Ziele, EIN Essenz-Pool.
// Begleiter sind Werkzeuge des Spielers, kein zweiter Char: sie handeln nur auf Befehl (Assist-Button).
#include <array>
#include <cstdint>
#include <vector>

#include "fightcore/Types.h"

namespace fc {

enum class CompanionType : uint8_t { Striker, Zoner, Support, Guard };
// Richtung beim Assist-Druck: neutral = Attack, vorwärts = Pressure, hoch (8) = AntiAir, runter (2) = Protect
enum class AssistRole : uint8_t { Attack = 0, Pressure = 1, AntiAir = 2, Protect = 3 };
// Generische Verhaltens-Bausteine, aus denen jede Begleiter-Aktion gebaut wird
enum class AssistKind : uint8_t {
    Rush,   // zum Gegner sprinten und zuschlagen
    Shoot,  // Projektil auf den Gegner
    Rise,   // vor dem Spieler hochsteigen (Anti-Air)
    Pull,   // Gegner packen und zum Spieler ziehen
    Shield, // sich vor den Spieler stellen und Treffer abfangen
};

struct AssistAction {
    AssistKind kind = AssistKind::Rush;
    int16_t startup = 10, active = 4, recovery = 12;
    int16_t move = 0; // Index in CompanionDef::moves (HitProps)
    Fx speed = Fx::ratio(25, 100);
    int16_t total() const { return static_cast<int16_t>(startup + active + recovery); }
};

struct CompanionDef {
    const char* name = "";
    CompanionType type = CompanionType::Striker;
    int16_t maxHp = 200;        // kleiner als Spieler-HP
    int16_t upkeep = 4;         // Essenz-Abzug pro Frame (1/100), solange auf dem Feld
    int16_t cooldown = 180;     // pro Begleiter, nach jeder Aktion
    int16_t reviveFrames = 900; // nur Summoner-Assist: kehrt nach 15 s zurück
    int16_t reviveCost = 150;   // Essenz
    bool adaptive = false;      // passt sich an wiederholte Angriffs-Kategorien an
    std::vector<MoveDef> moves;
    std::array<AssistAction, 4> actions; // Index = AssistRole
};

constexpr int kRosterMax = 6;
constexpr int kCompanionSlots = 3;    // Summoner-Assist + 2 Technik-Begleiter (oder Pakt-Begleiter)
constexpr int kTechCompanionMax = 2;  // max. gleichzeitige Technik-Begleiter
constexpr int kCompanionDamagePct = 60; // Begleiter-Schaden <= 60 % eines vergleichbaren Skills
constexpr int16_t kBondMax = 6;       // Stufe 2 ab 3, Stufe 3 ab 6 erfolgreichen Assists

namespace companions {
// Summoner-Assists (frei wählbar)
const CompanionDef& bladeWolf();   // Klingenwolf   – Striker
const CompanionDef& sparkOwl();    // Funkeneule    – Zoner
const CompanionDef& vineToad();    // Rankenkröte   – Support
const CompanionDef& stoneWarden(); // Steinwächter  – Guard
// Pakt-Belohnungen
const CompanionDef& pactSpirit();  // Paktgeist     – temporärer Zusatz-Begleiter
const CompanionDef& shifter();     // Wandler       – adaptive Sonderform
} // namespace companions

} // namespace fc
