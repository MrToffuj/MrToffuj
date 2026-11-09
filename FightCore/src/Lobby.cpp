#include "Techniques.h"
#include "fightcore/Simulation.h"

namespace fc {
namespace {

uint32_t next(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state >> 8;
}

} // namespace

MatchSetup buildMatch(GameMode mode, const Loadout* humans, int humanCount, uint8_t botLevel, uint32_t seed) {
    return buildMatch(mode, rulesFor(mode), humans, humanCount, botLevel, seed);
}

MatchSetup buildMatch(GameMode mode, const ModeRules& rules, const Loadout* humans, int humanCount, uint8_t botLevel,
                      uint32_t seed) {
    MatchSetup s;
    s.mode = mode;
    s.rules = rules;
    if (!s.rules.map) s.rules.map = rulesFor(mode).map;
    s.count = s.rules.fighters > kMaxFighters ? static_cast<int8_t>(kMaxFighters) : s.rules.fighters;
    if (humanCount > s.count) humanCount = s.count;

    const BodyDef* bodyPool[] = {&bodies::heavy(), &bodies::speedster(), &bodies::caster(), &bodies::brawler(),
                                 &bodies::technician(), &bodies::summoner(), &bodies::heavensBind()};
    const TechniqueDef* techPool[] = {&techniques::boundless(),    &techniques::calamityFlames(), &techniques::shadowPuppets(),
                                      &techniques::graniteCannon(), &techniques::skyWeave(),       &techniques::thunderVessel(),
                                      &techniques::starMass(),      &techniques::blossomDraw(),    &techniques::spiritHoard(),
                                      &techniques::jackpot(),       &techniques::strawEffigy(),    &techniques::puppetPilot(),
                                      &techniques::lightMalice()};
    constexpr int kTechCount = static_cast<int>(sizeof(techPool) / sizeof(techPool[0]));
    const CompanionDef* assistPool[] = {&companions::bladeWolf(), &companions::sparkOwl(), &companions::vineToad(),
                                        &companions::stoneWarden()};

    const int teams = s.rules.teamCount;
    int teamSize[kMaxTeams] = {};
    for (int i = 0; i < humanCount; ++i)
        if (teams > 0) ++teamSize[humans[i].team % teams];

    uint32_t rng = seed;
    for (int i = 0; i < s.count; ++i) {
        Loadout& l = s.loadout[i];
        if (i < humanCount) {
            l = humans[i];
            l.bot = false;
            if (teams > 0) l.team = static_cast<uint8_t>(l.team % teams);
            continue;
        }
        l = Loadout{};
        l.bot = true;
        l.botLevel = botLevel;
        l.body = bodyPool[next(rng) % 7];
        l.technique = l.body->has(PassiveHeavensBind) ? &techniques::toolArsenal() : techPool[next(rng) % kTechCount];
        l.assist = assistPool[next(rng) % 4];
        if (teams > 0) {
            int smallest = 0;
            for (int t = 1; t < teams; ++t)
                if (teamSize[t] < teamSize[smallest]) smallest = t;
            l.team = static_cast<uint8_t>(smallest);
            ++teamSize[smallest];
        }
    }
    s.finalize();
    return s;
}

} // namespace fc
