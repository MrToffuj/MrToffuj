// Lobby: Menschen + Bots zu einem MatchSetup zusammenbauen.
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
    const ModeRules rules = rulesFor(mode);
    MatchSetup s;
    s.mode = mode;
    s.count = rules.fighters;
    if (humanCount > s.count) humanCount = s.count;

    const BodyDef* bodyPool[] = {&bodies::heavy(), &bodies::speedster(), &bodies::caster(), &bodies::brawler(),
                                 &bodies::technician(), &bodies::summoner(), &bodies::nullborn()};
    const TechniqueDef* techPool[] = {&techniques::nullraum(), &techniques::glutkern(), &techniques::schattenmenagerie()};
    const CompanionDef* assistPool[] = {&companions::bladeWolf(), &companions::sparkOwl(), &companions::vineToad(),
                                        &companions::stoneWarden()};

    int teamSize[2] = {0, 0};
    uint32_t rng = seed;
    for (int i = 0; i < s.count; ++i) {
        Loadout& l = s.loadout[i];
        if (i < humanCount) {
            l = humans[i];
            l.bot = false;
            if (rules.teams) { l.team = static_cast<uint8_t>(l.team > 1 ? 1 : l.team); ++teamSize[l.team]; }
            continue;
        }
        l = Loadout{};
        l.bot = true;
        l.botLevel = botLevel;
        l.body = bodyPool[next(rng) % 7];
        l.technique = l.body->has(PassiveNullborn) ? nullptr : techPool[next(rng) % 3];
        l.assist = assistPool[next(rng) % 4];
        if (rules.teams) {
            l.team = teamSize[0] <= teamSize[1] ? 0 : 1;
            ++teamSize[l.team];
        }
    }
    s.finalize();
    return s;
}

} // namespace fc
