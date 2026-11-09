#include "fightcore/Registry.h"

#include <deque>

#include "Common.h"
#include "Techniques.h"
#include "fightcore/Json.h"

namespace fc {

// ---------------------------------------------------------------- data driven techniques
namespace moddata {

enum class ActionKind : uint8_t { Melee, Projectile, Beam, Zone, Pull, Teleport, Buff, Counter, Summon, Heal };
enum class CoreKind : uint8_t { None, StacksOnHit, BurnOnHit, GuardProjectiles };

struct Action {
    ActionKind kind = ActionKind::Melee;
    Fx range = Fx::ratio(12, 10), radius = Fx::ratio(5, 10), speed = Fx::ratio(4, 10), length = Fx::fromInt(10);
    int16_t life = 60, delay = 0, frames = 0;
    int16_t damagePct = 0, speedPct = 0, takenPct = 0, heal = 0, selfDamage = 0;
    int16_t companion = -1;
    int16_t counterMove = -1;
};

struct Core {
    CoreKind kind = CoreKind::None;
    int16_t max = 5, perStack = 5, damage = 2, frames = 180, ceCost = 30;
};

enum : int { kStacks, kBuffTimer, kBuffAction, kCounterTimer, kCounterAction, kBurnVictim, kBurnTimer };

struct DataTechnique;

class DataLogic final : public TechniqueLogic {
public:
    explicit DataLogic(const DataTechnique* t) : t_(t) {}
    void onMoveEvent(const TechContext& c, int16_t move) const override;
    void onTick(const TechContext& c) const override;
    void onHitLanded(const TechContext& c, int16_t move, bool projectile, int victim) const override;
    bool nullifiesIncoming(const TechContext& c, const MoveDef& mv, bool projectile, int attacker) const override;
    int damageDealtPct(const TechContext& c, int16_t move) const override;
    int damageTakenPct(const TechContext& c) const override;
    int speedPct(const TechContext& c) const override;

private:
    const DataTechnique* t_;
};

struct DataTechnique {
    TechniqueDef def;
    std::array<Action, 4> actions; // three skills, then the Max Output; move index == action index
    Core core;
    std::vector<std::unique_ptr<CompanionDef>> companions;
    std::unique_ptr<DataLogic> logic;
};

void DataLogic::onMoveEvent(const TechContext& c, int16_t move) const {
    if (move < 0 || move > 3) return;
    using namespace techniques;
    const Action& a = t_->actions[move];
    FighterState& me = c.me();
    switch (a.kind) {
        case ActionKind::Melee: break;
        case ActionKind::Projectile:
            c.spawnProjectile(c.localToWorld({Fx{}, tenths(12), tenths(8)}), me.facing * a.speed, a.radius, a.life, move);
            break;
        case ActionKind::Beam:
            c.spawnBeam(c.localToWorld({Fx{}, tenths(12), tenths(6)}), me.facing, a.length, a.radius, 4, move);
            break;
        case ActionKind::Zone:
            if (c.hasTarget()) c.spawnProjectile(c.opp().pos + Vec3{Fx{}, Fx::fromInt(1), Fx{}}, {}, a.radius, 20, move, a.delay);
            break;
        case ActionKind::Pull:
            if (c.hasTarget() && flatDistance(me.pos, c.opp().pos) <= a.range) c.strike(me.target, move);
            break;
        case ActionKind::Teleport:
            me.pos += me.facing * (me.input.current().dir <= 3 ? -a.range : a.range);
            break;
        case ActionKind::Buff:
            me.tech[kBuffTimer] = a.frames;
            me.tech[kBuffAction] = move;
            break;
        case ActionKind::Counter:
            me.tech[kCounterTimer] = a.frames;
            me.tech[kCounterAction] = move;
            break;
        case ActionKind::Summon: {
            const int slot = c.companionSlot(a.companion);
            if (slot >= 0) c.command(slot, AssistRole::Attack);
            else c.summon(a.companion);
            break;
        }
        case ActionKind::Heal: {
            const int hp = me.health + a.heal;
            me.health = static_cast<int16_t>(hp > c.stats().maxHealth ? c.stats().maxHealth : hp);
            break;
        }
    }
    if (a.selfDamage > 0) recoil(c, a.selfDamage);
}

void DataLogic::onTick(const TechContext& c) const {
    int32_t* t = c.me().tech.data();
    if (t[kBuffTimer] > 0) --t[kBuffTimer];
    if (t[kCounterTimer] > 0) --t[kCounterTimer];
    if (t[kBurnTimer] > 0) {
        --t[kBurnTimer];
        if (c.m.frame % 20 == 0 && !c.m.f[t[kBurnVictim]].out) c.damage(t[kBurnVictim], t_->core.damage);
    }
}

void DataLogic::onHitLanded(const TechContext& c, int16_t move, bool, int victim) const {
    int32_t* t = c.me().tech.data();
    if (t_->core.kind == CoreKind::StacksOnHit) t[kStacks] = t[kStacks] + 1 > t_->core.max ? t_->core.max : t[kStacks] + 1;
    if (t_->core.kind == CoreKind::BurnOnHit && move >= 0) {
        t[kBurnVictim] = victim;
        t[kBurnTimer] = t_->core.frames;
    }
}

bool DataLogic::nullifiesIncoming(const TechContext& c, const MoveDef&, bool projectile, int attacker) const {
    FighterState& me = c.me();
    if (me.tech[kCounterTimer] > 0 && !projectile) {
        me.tech[kCounterTimer] = 0;
        c.strike(attacker, t_->actions[me.tech[kCounterAction] & 3].counterMove);
        return true;
    }
    const int32_t cost = t_->core.ceCost * 100;
    if (t_->core.kind == CoreKind::GuardProjectiles && projectile && me.burnout == 0 && me.ce >= cost) {
        me.ce -= cost;
        return true;
    }
    return false;
}

int DataLogic::damageDealtPct(const TechContext& c, int16_t) const {
    const int32_t* t = c.me().tech.data();
    int p = 100;
    if (t_->core.kind == CoreKind::StacksOnHit) p += t[kStacks] * t_->core.perStack;
    if (t[kBuffTimer] > 0) p += t_->actions[t[kBuffAction] & 3].damagePct;
    return p < 10 ? 10 : p;
}

int DataLogic::damageTakenPct(const TechContext& c) const {
    const int32_t* t = c.me().tech.data();
    return t[kBuffTimer] > 0 ? 100 + t_->actions[t[kBuffAction] & 3].takenPct : 100;
}

int DataLogic::speedPct(const TechContext& c) const {
    const int32_t* t = c.me().tech.data();
    return t[kBuffTimer] > 0 ? 100 + t_->actions[t[kBuffAction] & 3].speedPct : 100;
}

} // namespace moddata

struct Registry::ModContent {
    std::deque<std::string> strings;
    std::vector<std::unique_ptr<moddata::DataTechnique>> techniques;
    std::vector<std::unique_ptr<MapDef>> maps;
    std::vector<std::unique_ptr<CursedObjectDef>> cursed;
    std::vector<std::unique_ptr<OutfitDef>> outfits;

    const char* keep(std::string_view s) {
        strings.emplace_back(s);
        return strings.back().c_str();
    }
};

// ---------------------------------------------------------------- parsing helpers
namespace {

using json::Value;
using moddata::Action;
using moddata::ActionKind;
using moddata::CoreKind;

template <typename T>
T clampTo(int32_t v, int32_t lo, int32_t hi) { return static_cast<T>(v < lo ? lo : (v > hi ? hi : v)); }
Fx clampFx(Fx v, int lo, int hi) {
    if (v < Fx::fromInt(lo)) return Fx::fromInt(lo);
    if (v > Fx::fromInt(hi)) return Fx::fromInt(hi);
    return v;
}

struct Builder {
    Registry::ModContent& mod;
    std::string& error;

    bool fail(const std::string& what) {
        if (error.empty()) error = what;
        return false;
    }

    bool pick(std::string_view name, std::initializer_list<std::string_view> options, int& out, const char* what) {
        int i = 0;
        for (std::string_view o : options) {
            if (o == name) { out = i; return true; }
            ++i;
        }
        return fail(std::string("unknown ") + what + " '" + std::string(name) + "'");
    }

    HitProps hitProps(const Value& j) {
        int impact = 1;
        pick(j.stringOr("impact", "medium"), {"light", "medium", "heavy", "crushing"}, impact, "impact");
        return techniques::hit(clampTo<int16_t>(j.intOr("damage", 60), 0, 400), clampTo<int16_t>(j.intOr("hitstun", 20), 0, 90),
                               clampFx(j.fxOr("knockback", Fx::ratio(2, 10)), -1, 1),
                               clampFx(j.fxOr("launch", Fx{}), 0, 1), static_cast<ImpactClass>(impact), j.boolOr("unblockable", false));
    }

    bool companion(const Value& j, moddata::DataTechnique& t, int16_t& index) {
        auto c = std::make_unique<CompanionDef>();
        c->name = mod.keep(j.stringOr("name", "Spirit"));
        int type = 0;
        if (!pick(j.stringOr("type", "striker"), {"striker", "zoner", "support", "guard"}, type, "companion type")) return false;
        c->type = static_cast<CompanionType>(type);
        c->maxHp = clampTo<int16_t>(j.intOr("hp", 200), 50, 600);
        c->upkeep = clampTo<int16_t>(j.intOr("upkeep", 4), 0, 30);
        c->cooldown = clampTo<int16_t>(j.intOr("cooldown", 180), 30, 900);
        MoveDef strike = techniques::move("Strike", MoveCategory::Skill, 1, 0, 1);
        strike.hit = hitProps(j);
        MoveDef lift = strike;
        lift.hit.launch = Fx::ratio(3, 10);
        c->moves = {strike, lift};
        const int sp = clampTo<int>(j.intOr("speedPct", 35), 10, 60);
        const AssistKind main = c->type == CompanionType::Zoner ? AssistKind::Shoot
                              : c->type == CompanionType::Support ? AssistKind::Pull : AssistKind::Rush;
        c->actions = {techniques::act(main, 12, 5, 12, 0, sp), techniques::act(main, 10, 5, 10, 0, sp + 5),
                      techniques::act(AssistKind::Rise, 12, 5, 12, 1, sp), techniques::act(AssistKind::Shield, 8, 60, 8, 0, sp)};
        index = static_cast<int16_t>(t.companions.size());
        t.companions.push_back(std::move(c));
        return true;
    }

    bool action(const Value& j, int index, moddata::DataTechnique& t) {
        const bool isMax = index == 3;
        Action& a = t.actions[index];
        int kind = 0;
        if (!pick(j.stringOr("effect", "melee"),
                  {"melee", "projectile", "beam", "zone", "pull", "teleport", "buff", "counter", "summon", "heal"}, kind, "effect"))
            return false;
        a.kind = static_cast<ActionKind>(kind);
        a.range = clampFx(j.fxOr("range", a.range), 0, 30);
        a.radius = clampFx(j.fxOr("radius", a.radius), 0, 6);
        a.speed = clampFx(j.fxOr("speed", a.speed), 0, 2);
        a.length = clampFx(j.fxOr("length", a.length), 1, 40);
        a.life = clampTo<int16_t>(j.intOr("life", 60), 1, 600);
        a.delay = clampTo<int16_t>(j.intOr("delay", 0), 0, 120);
        a.frames = clampTo<int16_t>(j.intOr("frames", 120), 1, 1200);
        a.damagePct = clampTo<int16_t>(j.intOr("damagePct", 0), -50, 100);
        a.speedPct = clampTo<int16_t>(j.intOr("speedPct", 0), -50, 100);
        a.takenPct = clampTo<int16_t>(j.intOr("takenPct", 0), -60, 100);
        a.heal = clampTo<int16_t>(j.intOr("heal", 0), 0, 400);
        a.selfDamage = clampTo<int16_t>(j.intOr("selfDamage", 0), 0, 200);

        const std::string_view name = j.stringOr("name", "");
        if (name.empty()) return fail("every skill needs a name");
        const int16_t startup = clampTo<int16_t>(j.intOr("startup", 12), 1, 120);
        MoveDef& m = t.def.moves[index];
        m = techniques::move(mod.keep(name), isMax ? MoveCategory::MaxOutput : MoveCategory::Skill, startup, 0,
                             clampTo<int16_t>(j.intOr("recovery", 18), 0, 120), startup);
        m.hit = hitProps(j);
        if (a.kind == ActionKind::Melee) {
            m.active = clampTo<int16_t>(j.intOr("active", 4), 1, 30);
            m.hitboxes = {{{Fx{}, Fx::ratio(12, 10), a.range}, a.radius}};
            m.eventFrame = -1;
        }
        if (a.kind == ActionKind::Pull) m.hit.knockback = -abs(m.hit.knockback.raw ? m.hit.knockback : Fx::ratio(35, 100));
        if (a.kind == ActionKind::Counter) {
            MoveDef counter = techniques::move(mod.keep(std::string(name) + " Counter"), MoveCategory::Skill, 1, 0, 1);
            counter.hit = m.hit;
            counter.isCounter = true;
            a.counterMove = static_cast<int16_t>(t.def.moves.size());
            t.def.moves.push_back(counter);
        }
        if (a.kind == ActionKind::Summon) {
            const Value* cj = j.get("companion");
            if (!cj) return fail("summon needs a companion");
            if (!companion(*cj, t, a.companion)) return false;
        }
        if (!isMax) {
            t.def.skills[index] = {static_cast<int16_t>(index), clampTo<int16_t>(j.intOr("ce", 60), 0, 500),
                                   clampTo<int16_t>(j.intOr("cooldown", 120), 20, 1800)};
        } else {
            int type = 0;
            if (!pick(j.stringOr("type", "burst"), {"burst", "zone", "transform"}, type, "max output type")) return false;
            t.def.maxOutputType = static_cast<MaxOutputType>(type);
        }
        return true;
    }

    bool technique(const Value& j, moddata::DataTechnique& t) {
        const std::string_view id = j.stringOr("id", "");
        if (id.empty()) return fail("technique without an id");
        t.def.id = mod.keep(id);
        t.def.name = mod.keep(j.stringOr("name", id));
        t.def.coreRule = mod.keep(j.stringOr("coreRule", ""));
        t.def.weakness = mod.keep(j.stringOr("weakness", ""));

        if (const Value* realm = j.get("realm")) {
            t.def.realmName = mod.keep(realm->stringOr("name", "Realm"));
            int cls = 0;
            if (!pick(realm->stringOr("class", "damage"), {"damage", "lock", "debuff", "buffField"}, cls, "realm class")) return false;
            t.def.realmClass = static_cast<RealmClass>(cls);
        }
        if (const Value* core = j.get("core")) {
            int kind = 0;
            if (!pick(core->stringOr("type", "none"), {"none", "stacksOnHit", "burnOnHit", "guardProjectiles"}, kind, "core rule"))
                return false;
            t.core.kind = static_cast<CoreKind>(kind);
            t.core.max = clampTo<int16_t>(core->intOr("max", 5), 1, 10);
            t.core.perStack = clampTo<int16_t>(core->intOr("damagePerStack", 5), 0, 20);
            t.core.damage = clampTo<int16_t>(core->intOr("damage", 2), 0, 20);
            t.core.frames = clampTo<int16_t>(core->intOr("frames", 180), 1, 900);
            t.core.ceCost = clampTo<int16_t>(core->intOr("ceCost", 30), 5, 200);
        }

        const Value* skills = j.get("skills");
        const Value* maxOutput = j.get("maxOutput");
        if (!skills || skills->items.size() != 3) return fail(std::string(id) + ": needs exactly three skills");
        if (!maxOutput) return fail(std::string(id) + ": needs a maxOutput");
        t.def.moves.resize(4);
        for (int k = 0; k < 3; ++k)
            if (!action(skills->items[k], k, t)) return false;
        if (!action(*maxOutput, 3, t)) return false;
        t.def.maxOutputMove = 3;
        for (const auto& c : t.companions) t.def.companions.push_back(c.get());
        t.logic = std::make_unique<moddata::DataLogic>(&t);
        t.def.logic = t.logic.get();
        return true;
    }

    bool colony(const Value& j, MapDef& m) {
        const std::string_view id = j.stringOr("id", "");
        if (id.empty()) return fail("map without an id");
        m.id = mod.keep(id);
        m.name = mod.keep(j.stringOr("name", id));
        m.radius = clampFx(j.fxOr("radius", Fx::fromInt(60)), 10, 250);
        if (const Value* obstacles = j.get("obstacles")) {
            if (obstacles->items.size() > static_cast<size_t>(kMaxObstacles))
                return fail(std::string(id) + ": at most " + std::to_string(kMaxObstacles) + " obstacles");
            for (const Value& o : obstacles->items) {
                const Fx half = Fx::ratio(1, 2);
                m.obstacles.push_back({{o.fxOr("x", Fx{}), Fx{}, o.fxOr("z", Fx{})},
                                       clampFx(o.fxOr("width", Fx::fromInt(4)), 1, 60) * half,
                                       clampFx(o.fxOr("depth", Fx::fromInt(4)), 1, 60) * half,
                                       clampFx(o.fxOr("height", Fx::fromInt(5)), 1, 80), clampTo<int16_t>(o.intOr("hp", 400), 50, 5000)});
            }
        }
        if (const Value* zones = j.get("zones"))
            for (const Value& z : zones->items)
                if (z.items.size() == 2 && m.zones.size() < static_cast<size_t>(kMaxZones))
                    m.zones.push_back({z.items[0].asFx(), Fx{}, z.items[1].asFx()});
        if (m.zones.empty()) m.zones.push_back({});
        return true;
    }

    bool cursedObject(const Value& j, CursedObjectDef& item) {
        const std::string_view id = j.stringOr("id", "");
        if (id.empty()) return fail("cursed object without an id");
        item.id = mod.keep(id);
        item.name = mod.keep(j.stringOr("name", id));
        item.description = mod.keep(j.stringOr("description", ""));
        static const char* const kNames[] = {"damagePct",       "damageTakenPct",   "normalDamagePct",      "skillDamagePct",
                                             "companionDamagePct", "speedPct",      "ceRegenPct",           "ceMaxPct",
                                             "maxHealthPct",    "cooldownPct",      "realmGainPct",         "startRealmMeterPct",
                                             "blackSparkWindow", "parryWindow",     "lifestealPct",         "healOnKo",
                                             "guaranteedHitTakenPct", "overtimeDamagePct", "critChancePct", "thornsPct"};
        static_assert(sizeof(kNames) / sizeof(kNames[0]) == static_cast<size_t>(Effect::Count));
        const Value* effects = j.get("effects");
        if (!effects || effects->members.empty()) return fail(std::string(id) + ": needs at least one effect");
        for (const auto& [key, value] : effects->members) {
            int e = -1;
            for (int k = 0; k < static_cast<int>(Effect::Count); ++k)
                if (key == kNames[k]) e = k;
            if (e < 0) return fail(std::string(id) + ": unknown effect '" + key + "'");
            item.effects.push_back({static_cast<Effect>(e), clampTo<int16_t>(value.asInt(), -90, 200)});
        }
        return true;
    }

    bool outfit(const Value& j, OutfitDef& o) {
        const std::string_view id = j.stringOr("id", "");
        if (id.empty()) return fail("outfit without an id");
        o.id = mod.keep(id);
        o.name = mod.keep(j.stringOr("name", id));
        int slot = 0;
        if (!pick(j.stringOr("slot", "top"), {"head", "face", "top", "bottom", "shoes", "accessory"}, slot, "outfit slot")) return false;
        o.slot = static_cast<OutfitSlot>(slot);
        o.meshMasculine = mod.keep(j.stringOr("meshMasculine", ""));
        o.meshFeminine = mod.keep(j.stringOr("meshFeminine", ""));
        if (std::string_view(o.meshMasculine).empty() || std::string_view(o.meshFeminine).empty())
            return fail(std::string(id) + ": every outfit needs a masculine and a feminine mesh");
        if (const Value* u = j.get("unlock")) {
            int kind = 0;
            if (!pick(u->stringOr("kind", "default"),
                      {"default", "wins", "kos", "matches", "blackSparks", "realmsOpened", "matchesWithTechnique"}, kind, "unlock kind"))
                return false;
            o.unlock = {static_cast<UnlockKind>(kind), clampTo<int32_t>(u->intOr("amount", 0), 0, 100000),
                        mod.keep(u->stringOr("technique", ""))};
        }
        return true;
    }
};

uint64_t fnv(std::string_view s) {
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
    return h;
}

template <typename T, typename F>
const T* findById(const std::vector<const T*>& list, std::string_view id, F idOf) {
    for (const T* t : list)
        if (idOf(t) == id) return t;
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------- registry
Registry& Registry::instance() {
    static Registry r;
    return r;
}

Registry::Registry() { addBuiltins(); }

void Registry::addBuiltins() {
    using namespace techniques;
    techniques_ = {&boundless(),   &calamityFlames(), &shadowPuppets(), &graniteCannon(), &skyWeave(),
                   &thunderVessel(), &toolArsenal(),  &starMass(),      &blossomDraw(),   &spiritHoard(),
                   &jackpot(),     &strawEffigy(),    &puppetPilot(),   &lightMalice()};
    maps_ = {&maps::shrineWard(), &maps::harborDistrict(), &maps::oldCapital(),
             &maps::templeSteps(), &maps::shatteredDowntown(), &maps::frozenLake()};
    cursed_ = cursed::builtins();
    outfits_ = outfits::builtins();
}

const TechniqueDef* Registry::technique(std::string_view id) const {
    return findById(techniques_, id, [](const TechniqueDef* t) { return std::string_view(t->id); });
}
const MapDef* Registry::map(std::string_view id) const {
    return findById(maps_, id, [](const MapDef* t) { return std::string_view(t->id); });
}
const CursedObjectDef* Registry::cursedObject(std::string_view id) const {
    return findById(cursed_, id, [](const CursedObjectDef* t) { return std::string_view(t->id); });
}
const OutfitDef* Registry::outfit(std::string_view id) const {
    return findById(outfits_, id, [](const OutfitDef* t) { return std::string_view(t->id); });
}

bool Registry::loadMod(std::string_view text, std::string* errorOut) {
    std::string error;
    auto fail = [&](const std::string& what) {
        if (errorOut) *errorOut = what;
        return false;
    };
    json::Value root;
    if (!json::parse(text, root, &error)) return fail("mod.json: " + error);
    ModInfo info;
    info.id = std::string(root.stringOr("id", ""));
    if (info.id.empty()) return fail("mod.json needs an id");
    for (const ModInfo& m : modInfos_)
        if (m.id == info.id) return fail("mod '" + info.id + "' is already loaded");
    info.name = std::string(root.stringOr("name", info.id));
    info.version = std::string(root.stringOr("version", "1"));
    info.author = std::string(root.stringOr("author", ""));
    info.hash = fnv(text);

    auto content = std::make_unique<ModContent>();
    Builder b{*content, error};
    if (const Value* list = root.get("techniques"))
        for (const Value& j : list->items) {
            auto t = std::make_unique<moddata::DataTechnique>();
            if (!b.technique(j, *t)) return fail(error);
            if (technique(t->def.id)) return fail(std::string("technique id '") + t->def.id + "' is taken");
            content->techniques.push_back(std::move(t));
        }
    if (const Value* list = root.get("maps"))
        for (const Value& j : list->items) {
            auto m = std::make_unique<MapDef>();
            if (!b.colony(j, *m)) return fail(error);
            if (map(m->id)) return fail(std::string("map id '") + m->id + "' is taken");
            content->maps.push_back(std::move(m));
        }
    if (const Value* list = root.get("cursedObjects"))
        for (const Value& j : list->items) {
            auto c = std::make_unique<CursedObjectDef>();
            if (!b.cursedObject(j, *c)) return fail(error);
            if (cursedObject(c->id)) return fail(std::string("cursed object id '") + c->id + "' is taken");
            content->cursed.push_back(std::move(c));
        }
    if (const Value* list = root.get("outfits"))
        for (const Value& j : list->items) {
            auto o = std::make_unique<OutfitDef>();
            if (!b.outfit(j, *o)) return fail(error);
            if (outfit(o->id)) return fail(std::string("outfit id '") + o->id + "' is taken");
            content->outfits.push_back(std::move(o));
        }

    // everything checked out, publish it
    for (const auto& t : content->techniques) techniques_.push_back(&t->def);
    for (const auto& m : content->maps) maps_.push_back(m.get());
    for (const auto& c : content->cursed) cursed_.push_back(c.get());
    for (const auto& o : content->outfits) outfits_.push_back(o.get());
    info.techniques = static_cast<int>(content->techniques.size());
    info.maps = static_cast<int>(content->maps.size());
    info.cursedObjects = static_cast<int>(content->cursed.size());
    info.outfits = static_cast<int>(content->outfits.size());
    modInfos_.push_back(std::move(info));
    content_.push_back(std::move(content));
    return true;
}

void Registry::unloadMods() {
    modInfos_.clear();
    addBuiltins();
    content_.clear();
}

uint64_t Registry::contentHash() const {
    uint64_t h = 14695981039346656037ull;
    for (const ModInfo& m : modInfos_) { h ^= m.hash; h *= 1099511628211ull; }
    return h;
}

} // namespace fc
