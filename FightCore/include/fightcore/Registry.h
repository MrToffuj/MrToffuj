#pragma once
// Everything a match can be built from: techniques, colonies, cursed objects and outfits.
// Built-ins are always there; mods add more from mod.json files (see docs/MODDING.md).
//
// Mods are data, not code. That keeps every peer's simulation identical and means a mod can't
// run anything on your machine. Peers compare contentHash() in the lobby and refuse to start if
// their mod sets differ.
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "fightcore/Cosmetics.h"
#include "fightcore/CursedObject.h"
#include "fightcore/Technique.h"
#include "fightcore/Types.h"

namespace fc {

struct ModInfo {
    std::string id, name, version, author;
    uint64_t hash = 0;
    int techniques = 0, maps = 0, cursedObjects = 0, outfits = 0;
};

class Registry {
public:
    static Registry& instance();

    const std::vector<const TechniqueDef*>& techniques() const { return techniques_; }
    const std::vector<const MapDef*>& maps() const { return maps_; }
    const std::vector<const CursedObjectDef*>& cursedObjects() const { return cursed_; }
    const std::vector<const OutfitDef*>& outfits() const { return outfits_; }
    const std::vector<ModInfo>& mods() const { return modInfos_; }

    const TechniqueDef* technique(std::string_view id) const;
    const MapDef* map(std::string_view id) const;
    const CursedObjectDef* cursedObject(std::string_view id) const;
    const OutfitDef* outfit(std::string_view id) const;

    // Parses one mod.json. On any error nothing from the mod is added.
    bool loadMod(std::string_view json, std::string* error = nullptr);
    // Drops all mods, keeps the built-ins.
    void unloadMods();
    // Hash over every loaded mod, in load order. Equal hashes = identical content.
    uint64_t contentHash() const;

    struct ModContent;

private:
    Registry();
    void addBuiltins();

    std::vector<const TechniqueDef*> techniques_;
    std::vector<const MapDef*> maps_;
    std::vector<const CursedObjectDef*> cursed_;
    std::vector<const OutfitDef*> outfits_;
    std::vector<ModInfo> modInfos_;
    std::vector<std::unique_ptr<ModContent>> content_;
};

} // namespace fc
