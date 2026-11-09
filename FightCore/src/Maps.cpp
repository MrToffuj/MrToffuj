// Carnage Game colonies. Each map is a circle sealed by the barrier.
#include "fightcore/Simulation.h"

namespace fc::maps {
namespace {

ObstacleDef box(int x10, int z10, int halfX10, int halfZ10, int height10, int16_t hp) {
    return {{Fx::ratio(x10, 10), Fx{}, Fx::ratio(z10, 10)}, Fx::ratio(halfX10, 10), Fx::ratio(halfZ10, 10),
            Fx::ratio(height10, 10), hp};
}

} // namespace

const MapDef& shrineWard() {
    static const MapDef map = [] {
        MapDef m;
        m.name = "Shrine Ward";
        m.radius = Fx::fromInt(14);
        // four stone pillars around the middle
        m.obstacles = {box(70, 0, 8, 8, 40, 250), box(-70, 0, 8, 8, 40, 250), box(0, 70, 8, 8, 40, 250),
                       box(0, -70, 8, 8, 40, 250)};
        m.zones = {{Fx{}, Fx{}, Fx{}}};
        return m;
    }();
    return map;
}

const MapDef& harborDistrict() {
    static const MapDef map = [] {
        MapDef m;
        m.name = "Harbor District";
        m.radius = Fx::fromInt(20);
        m.obstacles = {box(90, 0, 20, 12, 60, 400), box(-90, 0, 20, 12, 60, 400), box(0, 110, 12, 20, 50, 350),
                       box(0, -110, 12, 20, 50, 350), box(60, 60, 8, 8, 30, 200), box(-60, -60, 8, 8, 30, 200)};
        m.zones = {{Fx{}, Fx{}, Fx{}}, {Fx::fromInt(11), Fx{}, Fx::fromInt(11)}, {Fx::fromInt(-11), Fx{}, Fx::fromInt(-11)}};
        return m;
    }();
    return map;
}

const MapDef& oldCapital() {
    static const MapDef map = [] {
        MapDef m;
        m.name = "Old Capital";
        m.radius = Fx::fromInt(70);
        // inner ring of houses, outer ring of bigger blocks, plenty of stuff to wreck
        for (int k = 0; k < 8; ++k) {
            const int x = (k % 4 - 1) * 150 - 75, z = k < 4 ? -150 : 150;
            m.obstacles.push_back(box(x, z, 25, 20, 70, 450));
        }
        for (int k = 0; k < 8; ++k) {
            const int x = (k % 4 - 1) * 280 - 140, z = k < 4 ? -380 : 380;
            m.obstacles.push_back(box(x, z, 45, 35, 120, 700));
        }
        m.zones = {{Fx{}, Fx{}, Fx{}}, {Fx::fromInt(35), Fx{}, Fx{}}, {Fx::fromInt(-35), Fx{}, Fx{}}};
        return m;
    }();
    return map;
}

const MapDef& templeSteps() {
    static const MapDef map = [] {
        MapDef m;
        m.name = "Temple Steps";
        m.radius = Fx::fromInt(26);
        // a ring of twelve shrine pillars around a raised hall in the middle
        const int ring[12][2] = {{150, 0}, {130, 75}, {75, 130}, {0, 150}, {-75, 130}, {-130, 75},
                                 {-150, 0}, {-130, -75}, {-75, -130}, {0, -150}, {75, -130}, {130, -75}};
        for (const auto& p : ring) m.obstacles.push_back(box(p[0], p[1], 7, 7, 50, 220));
        m.obstacles.push_back(box(0, 0, 35, 25, 45, 900)); // main hall
        m.obstacles.push_back(box(0, 210, 30, 10, 25, 300)); // gate
        m.zones = {{Fx{}, Fx{}, Fx::fromInt(8)}, {Fx::fromInt(19), Fx{}, Fx{}}, {Fx::fromInt(-19), Fx{}, Fx{}}};
        return m;
    }();
    return map;
}

const MapDef& shatteredDowntown() {
    static const MapDef map = [] {
        MapDef m;
        m.name = "Shattered Downtown";
        m.radius = Fx::fromInt(55);
        // city blocks on a grid with streets in between, towers get taller toward the middle
        for (int gx = -3; gx <= 3; ++gx)
            for (int gz = -3; gz <= 3; ++gz) {
                if ((gx == 0 && gz == 0) || gx * gx + gz * gz > 10) continue;
                const int height = 160 - (gx * gx + gz * gz) * 10;
                if (static_cast<int>(m.obstacles.size()) < kMaxObstacles)
                    m.obstacles.push_back(box(gx * 130, gz * 130, 40, 40, height, static_cast<int16_t>(400 + height * 2)));
            }
        m.zones = {{Fx{}, Fx{}, Fx{}}, {Fx::fromInt(26), Fx{}, Fx::fromInt(-26)}, {Fx::fromInt(-26), Fx{}, Fx::fromInt(26)}};
        return m;
    }();
    return map;
}

const MapDef& frozenLake() {
    static const MapDef map = [] {
        MapDef m;
        m.name = "Frozen Lake";
        m.radius = Fx::fromInt(32);
        // wide open ice with a few frozen boat wrecks, a zoner's paradise
        m.obstacles = {box(100, 40, 30, 10, 20, 250), box(-120, -60, 25, 9, 18, 250), box(20, -160, 10, 28, 22, 250),
                       box(-40, 170, 12, 26, 20, 250), box(190, -120, 8, 8, 35, 180)};
        m.zones = {{Fx{}, Fx{}, Fx{}}, {Fx::fromInt(20), Fx{}, Fx::fromInt(12)}, {Fx::fromInt(-20), Fx{}, Fx::fromInt(-12)}};
        return m;
    }();
    return map;
}

} // namespace fc::maps
