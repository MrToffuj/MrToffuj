#include "fightcore/Cosmetics.h"

namespace fc {

int32_t unlockProgress(const OutfitDef& item, const PlayerRecord& r) {
    switch (item.unlock.kind) {
        case UnlockKind::Default: return 0;
        case UnlockKind::Wins: return r.wins;
        case UnlockKind::KOs: return r.kos;
        case UnlockKind::MatchesPlayed: return r.matches;
        case UnlockKind::BlackSparks: return r.blackSparks;
        case UnlockKind::RealmsOpened: return r.realmsOpened;
        case UnlockKind::MatchesWithTechnique: {
            const auto it = r.matchesWithTechnique.find(item.unlock.technique);
            return it == r.matchesWithTechnique.end() ? 0 : it->second;
        }
    }
    return 0;
}

bool isUnlocked(const OutfitDef& item, const PlayerRecord& r) {
    return item.unlock.kind == UnlockKind::Default || unlockProgress(item, r) >= item.unlock.amount;
}

namespace outfits {
namespace {

using S = OutfitSlot;
using U = UnlockKind;

// Mesh paths follow /Game/Outfits/<id>/<id>_M and _F
#define OUTFIT(id, name, slot, kind, amount, tech) \
    {id, name, slot, "/Game/Outfits/" id "/" id "_M", "/Game/Outfits/" id "/" id "_F", {kind, amount, tech}}

const OutfitDef kItems[] = {
    // school uniform basics everyone starts with
    OUTFIT("academy_jacket", "Academy Jacket", S::Top, U::Default, 0, ""),
    OUTFIT("academy_pants", "Academy Pants", S::Bottom, U::Default, 0, ""),
    OUTFIT("academy_skirt", "Academy Pleated Skirt", S::Bottom, U::Default, 0, ""),
    OUTFIT("school_loafers", "School Loafers", S::Shoes, U::Default, 0, ""),
    // earned
    OUTFIT("hooded_academy_jacket", "Hooded Academy Jacket", S::Top, U::Wins, 5, ""),
    OUTFIT("silk_blindfold", "Silk Blindfold", S::Face, U::RealmsOpened, 10, ""),
    OUTFIT("round_shades", "Round Shades", S::Face, U::KOs, 50, ""),
    OUTFIT("high_collar_coat", "High Collar Long Coat", S::Top, U::Wins, 25, ""),
    OUTFIT("five_color_kesa", "Five Color Kesa", S::Top, U::MatchesWithTechnique, 10, "spirit_hoard"),
    OUTFIT("overtime_suit", "Overtime Suit", S::Top, U::MatchesPlayed, 50, ""),
    OUTFIT("patterned_tie", "Patterned Tie", S::Accessory, U::BlackSparks, 20, ""),
    OUTFIT("shrine_kimono", "Shrine Kimono", S::Top, U::Wins, 100, ""),
    OUTFIT("compression_shirt", "Black Compression Shirt", S::Top, U::MatchesWithTechnique, 15, "tool_arsenal"),
    OUTFIT("cloud_staff_strap", "Staff Strap", S::Accessory, U::MatchesWithTechnique, 25, "tool_arsenal"),
    OUTFIT("pachinko_jacket", "Pachinko Parlor Jacket", S::Top, U::MatchesWithTechnique, 10, "jackpot"),
    OUTFIT("nail_pouch", "Hammer and Nail Pouch", S::Accessory, U::MatchesWithTechnique, 10, "straw_effigy"),
    OUTFIT("storm_jacket", "Storm Jacket", S::Top, U::MatchesWithTechnique, 10, "thunder_vessel"),
    OUTFIT("feather_mantle", "Feather Mantle", S::Top, U::MatchesWithTechnique, 10, "sky_weave"),
    OUTFIT("bandage_wrap", "Bandage Wrap", S::Face, U::MatchesWithTechnique, 10, "puppet_pilot"),
    OUTFIT("granite_vest", "Quarry Vest", S::Top, U::MatchesWithTechnique, 10, "granite_cannon"),
    OUTFIT("star_scarf", "Star Scarf", S::Accessory, U::MatchesWithTechnique, 10, "star_mass"),
    OUTFIT("iai_hakama", "Iai Hakama", S::Bottom, U::MatchesWithTechnique, 10, "blossom_draw"),
    OUTFIT("shadow_hoodie", "Shadow Hoodie", S::Top, U::MatchesWithTechnique, 10, "shadow_puppets"),
    OUTFIT("ember_haori", "Ember Haori", S::Top, U::MatchesWithTechnique, 10, "calamity_flames"),
    OUTFIT("void_coat", "Void Coat", S::Top, U::MatchesWithTechnique, 10, "boundless"),
    OUTFIT("light_robe", "Light Robe", S::Top, U::MatchesWithTechnique, 10, "light_malice"),
    OUTFIT("curtain_charm", "Curtain Charm", S::Accessory, U::Default, 0, ""),
    OUTFIT("geta_sandals", "Geta Sandals", S::Shoes, U::KOs, 100, ""),
};

#undef OUTFIT

} // namespace

const std::vector<const OutfitDef*>& builtins() {
    static const std::vector<const OutfitDef*> list = [] {
        std::vector<const OutfitDef*> v;
        for (const OutfitDef& o : kItems) v.push_back(&o);
        return v;
    }();
    return list;
}

} // namespace outfits
} // namespace fc
