#pragma once
// Outfits. Purely cosmetic, nothing here touches the simulation.
// Two base bodies; every clothing item comes in a masculine and a feminine cut, and either cut
// fits either body, so players mix freely.
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace fc {

enum class BodyType : uint8_t { Male, Female };
enum class Cut : uint8_t { Masculine, Feminine };
enum class OutfitSlot : uint8_t { Head, Face, Top, Bottom, Shoes, Accessory, Count };

enum class UnlockKind : uint8_t {
    Default,             // owned from the start
    Wins,
    KOs,
    MatchesPlayed,
    BlackSparks,
    RealmsOpened,
    MatchesWithTechnique // needs `technique`
};

struct UnlockRule {
    UnlockKind kind = UnlockKind::Default;
    int32_t amount = 0;
    const char* technique = ""; // technique id for MatchesWithTechnique
};

struct OutfitDef {
    const char* id = "";
    const char* name = "";
    OutfitSlot slot = OutfitSlot::Top;
    // asset paths, one per cut; the engine fits whichever cut onto whichever body
    const char* meshMasculine = "";
    const char* meshFeminine = "";
    UnlockRule unlock;
};

// Lifetime numbers the unlocks are checked against. Lives in the player's profile.
struct PlayerRecord {
    int32_t wins = 0;
    int32_t kos = 0;
    int32_t matches = 0;
    int32_t blackSparks = 0;
    int32_t realmsOpened = 0;
    std::unordered_map<std::string, int32_t> matchesWithTechnique;
};

struct OutfitPiece {
    const OutfitDef* item = nullptr;
    Cut cut = Cut::Masculine;
};

struct Appearance {
    BodyType body = BodyType::Male;
    std::array<OutfitPiece, static_cast<size_t>(OutfitSlot::Count)> pieces{};
};

bool isUnlocked(const OutfitDef& item, const PlayerRecord& record);
int32_t unlockProgress(const OutfitDef& item, const PlayerRecord& record); // current count toward the rule

namespace outfits {
const std::vector<const OutfitDef*>& builtins();
} // namespace outfits

} // namespace fc
