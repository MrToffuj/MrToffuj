// Carnage Game colonies. Each map is a circle sealed by the barrier.
// Layouts are generated with integer math only, so every peer builds the exact same map.
#include "fightcore/Simulation.h"

namespace fc::maps {
namespace {

// sizes in decimeters
ObstacleDef box(int x, int z, int halfX, int halfZ, int height, int hp) {
    return {{Fx::ratio(x, 10), Fx{}, Fx::ratio(z, 10)}, Fx::ratio(halfX, 10), Fx::ratio(halfZ, 10), Fx::ratio(height, 10),
            static_cast<int16_t>(hp)};
}

// unit circle in 1/1000, 16 directions
constexpr int kDirs[16][2] = {{1000, 0},    {924, 383},   {707, 707},   {383, 924},  {0, 1000},  {-383, 924},
                              {-707, 707},  {-924, 383},  {-1000, 0},   {-924, -383}, {-707, -707}, {-383, -924},
                              {0, -1000},   {383, -924},  {707, -707},  {924, -383}};

// count buildings evenly around a ring (count divides 16 or is 16), offset rotates the ring by one step
void ring(MapDef& m, int radius, int count, int offset, int halfX, int halfZ, int height, int hp) {
    const int step = 16 / count;
    for (int k = 0; k < count && static_cast<int>(m.obstacles.size()) < kMaxObstacles; ++k) {
        const auto& d = kDirs[(k * step + offset) % 16];
        m.obstacles.push_back(box(d[0] * radius / 1000, d[1] * radius / 1000, halfX, halfZ, height, hp));
    }
}

Vec3 at(int x, int z) { return {Fx::ratio(x, 10), Fx{}, Fx::ratio(z, 10)}; }

} // namespace

const MapDef& shrineWard() {
    static const MapDef map = [] {
        MapDef m;
        m.id = "shrine_ward";
        m.name = "Shrine Ward";
        m.radius = Fx::fromInt(30);
        ring(m, 120, 4, 0, 9, 9, 45, 260);   // inner pillars
        ring(m, 210, 8, 1, 12, 8, 30, 220);  // stone lanterns
        m.zones = {at(0, 0)};
        return m;
    }();
    return map;
}

const MapDef& harborDistrict() {
    static const MapDef map = [] {
        MapDef m;
        m.id = "harbor_district";
        m.name = "Harbor District";
        m.radius = Fx::fromInt(45);
        ring(m, 160, 4, 0, 35, 18, 70, 450);  // warehouses
        ring(m, 160, 4, 2, 10, 10, 30, 220);  // crate stacks
        ring(m, 320, 8, 1, 40, 22, 90, 600);  // outer warehouses
        ring(m, 320, 8, 0, 12, 12, 35, 240);
        m.zones = {at(0, 0), at(240, 240), at(-240, -240)};
        return m;
    }();
    return map;
}

const MapDef& oldCapital() {
    static const MapDef map = [] {
        MapDef m;
        m.id = "old_capital";
        m.name = "Old Capital";
        m.radius = Fx::fromInt(200);
        ring(m, 250, 8, 0, 30, 25, 80, 500);
        ring(m, 500, 16, 0, 35, 28, 90, 550);
        ring(m, 800, 16, 1, 45, 35, 120, 700);
        ring(m, 1150, 16, 0, 50, 40, 140, 800);
        ring(m, 1500, 16, 1, 60, 45, 160, 900);
        m.zones = {at(0, 0), at(900, 0), at(-900, 0)};
        return m;
    }();
    return map;
}

const MapDef& templeSteps() {
    static const MapDef map = [] {
        MapDef m;
        m.id = "temple_steps";
        m.name = "Temple Steps";
        m.radius = Fx::fromInt(70);
        m.obstacles.push_back(box(0, 0, 60, 45, 70, 1200)); // main hall
        ring(m, 200, 16, 0, 8, 8, 55, 230);                 // inner pillar ring
        ring(m, 380, 8, 1, 30, 18, 40, 400);                // side shrines
        ring(m, 550, 16, 0, 9, 9, 60, 240);                 // outer pillar ring
        m.obstacles.push_back(box(0, 620, 45, 12, 30, 350)); // gate
        m.zones = {at(0, 120), at(450, 0), at(-450, 0)};
        return m;
    }();
    return map;
}

const MapDef& shatteredDowntown() {
    static const MapDef map = [] {
        MapDef m;
        m.id = "shattered_downtown";
        m.name = "Shattered Downtown";
        m.radius = Fx::fromInt(140);
        // a grid of city blocks, towers get taller toward the middle, a plaza in the center
        const int spacing = 220, half = 70;
        for (int gx = -6; gx <= 6; ++gx)
            for (int gz = -6; gz <= 6; ++gz) {
                const int d2 = gx * gx + gz * gz;
                if (d2 <= 1 || d2 > 34 || static_cast<int>(m.obstacles.size()) >= kMaxObstacles) continue;
                const int height = 280 - d2 * 6;
                m.obstacles.push_back(box(gx * spacing, gz * spacing, half, half, height, 400 + height * 2));
            }
        m.zones = {at(0, 0), at(660, -440), at(-660, 440)};
        return m;
    }();
    return map;
}

const MapDef& frozenLake() {
    static const MapDef map = [] {
        MapDef m;
        m.id = "frozen_lake";
        m.name = "Frozen Lake";
        m.radius = Fx::fromInt(95);
        // wide open ice with a handful of frozen boat wrecks, a zoner's paradise
        ring(m, 280, 4, 1, 35, 12, 22, 260);
        ring(m, 560, 8, 0, 40, 14, 25, 280);
        ring(m, 780, 8, 1, 12, 12, 40, 200);
        m.zones = {at(0, 0), at(420, 260), at(-420, -260)};
        return m;
    }();
    return map;
}

} // namespace fc::maps
