// Runs a match with bots and dumps one frame as JSON, used for quick previews outside the engine.
// usage: fightcore_snapshot <mode 0-3> <frames> [seed]
#include <cstdio>
#include <cstdlib>

#include "fightcore/Simulation.h"

using namespace fc;

static void vec(const char* name, Vec3 v) { std::printf("\"%s\":[%.3f,%.3f,%.3f]", name, v.x.toFloat(), v.y.toFloat(), v.z.toFloat()); }

int main(int argc, char** argv) {
    const GameMode mode = static_cast<GameMode>(argc > 1 ? std::atoi(argv[1]) : 2);
    const int frames = argc > 2 ? std::atoi(argv[2]) : 900;
    const uint32_t seed = argc > 3 ? static_cast<uint32_t>(std::atoi(argv[3])) : 7;

    MatchSetup s = buildMatch(mode, nullptr, 0, 1, seed);
    if (argc > 4) {
        const MapDef* maps[] = {&maps::shrineWard(), &maps::harborDistrict(), &maps::oldCapital(), &maps::templeSteps(),
                                &maps::shatteredDowntown(), &maps::frozenLake()};
        ModeRules r = rulesFor(GameMode::Brawl);
        r.map = maps[std::atoi(argv[4]) % 6];
        s = buildMatch(GameMode::Brawl, r, nullptr, 0, 1, seed);
    }
    MatchState m = createMatch(s);
    FrameInput none[kMaxFighters] = {};
    for (int f = 0; f < frames && m.winner == kNoWinner; ++f) step(m, s, none);

    const MapDef& map = mapOf(s);
    std::printf("{\"map\":\"%s\",\"radius\":%.2f,\"frame\":%d,\"objective\":%d,\"teams\":%d,", map.name, map.radius.toFloat(), m.frame,
                static_cast<int>(s.rules.objective), s.rules.teamCount);
    std::printf("\"obstacles\":[");
    for (size_t k = 0; k < map.obstacles.size(); ++k) {
        const ObstacleDef& o = map.obstacles[k];
        std::printf("%s{", k ? "," : "");
        vec("c", o.center);
        std::printf(",\"hx\":%.2f,\"hz\":%.2f,\"h\":%.2f,\"broken\":%d}", o.halfX.toFloat(), o.halfZ.toFloat(), o.height.toFloat(),
                    m.obstacles[k].broken ? 1 : 0);
    }
    std::printf("],\"fighters\":[");
    for (int i = 0; i < m.count; ++i) {
        const FighterState& f = m.f[i];
        std::printf("%s{", i ? "," : "");
        vec("p", f.pos);
        std::printf(",");
        vec("d", f.facing);
        std::printf(",\"team\":%d,\"state\":%d,\"out\":%d,\"hp\":%.2f,\"target\":%d,\"comp\":[", f.team, static_cast<int>(f.state), f.out ? 1 : 0,
                    static_cast<float>(f.health) / s.stats[i].maxHealth, f.target);
        bool first = true;
        for (const CompanionState& c : f.comp) {
            if (!c.onField()) continue;
            std::printf("%s[%.2f,%.2f,%.2f]", first ? "" : ",", c.pos.x.toFloat(), c.pos.y.toFloat(), c.pos.z.toFloat());
            first = false;
        }
        std::printf("],\"realm\":%d,\"realmOpen\":%d,", f.realm.active() ? 1 : 0, f.realm.open ? 1 : 0);
        vec("rc", f.realm.open ? f.pos : f.realm.center);
        std::printf("}");
    }
    std::printf("],\"projectiles\":[");
    bool first = true;
    for (const Projectile& p : m.proj) {
        if (!p.alive) continue;
        std::printf("%s{\"p\":[%.2f,%.2f,%.2f],\"r\":%.2f,\"owner\":%d}", first ? "" : ",", p.pos.x.toFloat(), p.pos.y.toFloat(),
                    p.pos.z.toFloat(), p.radius.toFloat(), p.owner);
        first = false;
    }
    std::printf("]}\n");
    return 0;
}
