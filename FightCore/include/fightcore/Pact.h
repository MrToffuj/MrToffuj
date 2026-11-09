#pragma once
// Pakte (Schwur-System): verstecktes Layer. Spieler wählt Kategorie + Größe,
// das Spiel bewertet unsichtbar und gibt Belohnung oder Backlash.
#include <cstdint>

namespace fc {

struct BodyDef;
struct TechniqueDef;

enum class PactCategory : uint8_t { Body, Resource, Technique, Rule, Time };
enum class PactSize : uint8_t { Small = 1, Medium = 2, Large = 4 };
enum class PactTier : uint8_t { None, TooCheap, Fair, High, Overpaid };

// detail je Kategorie:
//  Body:      0 Max-HP, 1 Speed, 2 Block-Stärke
//  Resource:  0 Essenz-Pool, 1 Essenz-Regen, 2 Rückfluss-Heilung
//  Technique: 0..2 Skill sperren, 3 Realm verbieten, 4 Apex verbieten
//  Rule:      0 "nie blocken", 1 "nur Nahkampf" (alle Skills gesperrt)
//  Time:      Pakt gilt nur 60 s, dafür stärker (detail ignoriert)
struct PactChoice {
    bool active = false;
    PactCategory category = PactCategory::Body;
    PactSize size = PactSize::Small;
    uint8_t detail = 0;
};

// Playstyle-Daten der letzten Matches (0..100). Muss vor dem Match zwischen Clients ausgetauscht werden.
struct PlaystyleProfile {
    uint8_t skillUsage[3] = {33, 33, 33};
    uint8_t blockRate = 50;
    uint8_t realmUsage = 50;
    uint8_t apexUsage = 50;
    uint8_t healUsage = 30;
};

struct PactModifiers {
    // Restriktionen (Opfer)
    int16_t maxHealthPct = 100, speedPct = 100, blockPct = 100;
    int16_t cePoolPct = 100, ceRegenPct = 100, healPct = 100;
    uint8_t lockedSkills = 0; // Bitmaske
    bool noRealm = false, noApex = false, noBlock = false;
    // Belohnungen / Backlash
    int16_t damagePct = 100, realmGainPct = 100, cooldownPct = 100;
    int32_t ceLeak = 0;           // pro Frame, 1/100 Essenz
    int16_t extraZeroSpark = 0;   // zusätzliche Nullfunke-Frames
    bool openRealm = false;       // Realm ohne Barriere
    bool apexNoStartup = false;
    int32_t durationFrames = -1;  // -1 = ganzes Match
};

struct PactResult {
    PactTier tier = PactTier::None;
    int16_t hiddenValue = 0; // nur intern / Debug
    PactModifiers mods;
};

PactResult evaluatePact(const PactChoice& choice, const BodyDef& body, const TechniqueDef* tech,
                        const PlaystyleProfile& profile);

} // namespace fc
