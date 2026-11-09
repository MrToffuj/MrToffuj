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

} // namespace fc::maps
