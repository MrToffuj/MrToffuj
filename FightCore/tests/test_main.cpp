// Minimale Tests ohne Framework: Determinismus, Rollback, Treffer, Block, Wand, Essenz.
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "Kael.h"
#include "fightcore/Simulation.h"

using namespace fc;

static int g_failed = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } \
    } while (0)

static MatchSetup setup() {
    MatchSetup s;
    s.chars[0] = &chars::kael();
    s.chars[1] = &chars::kael();
    return s;
}

static FrameInput randomInput(uint32_t& seed) {
    seed = seed * 1664525u + 1013904223u;
    FrameInput in;
    in.dir = static_cast<uint8_t>(1 + (seed >> 8) % 9);
    in.buttons = static_cast<uint8_t>((seed >> 20) & 0x7F);
    return in;
}

static void testDeterminism() {
    std::puts("determinism");
    MatchSetup s = setup();
    MatchState a = createMatch(s), b = createMatch(s);
    uint32_t sa = 42, sb = 42;
    for (int i = 0; i < 5000; ++i) {
        FrameInput ia[2] = {randomInput(sa), randomInput(sa)};
        FrameInput ib[2] = {randomInput(sb), randomInput(sb)};
        step(a, s, ia);
        step(b, s, ib);
    }
    CHECK(hashState(a) == hashState(b));
}

static void testRollback() {
    std::puts("rollback");
    MatchSetup s = setup();
    MatchState m = createMatch(s);
    uint32_t seed = 7;
    std::vector<FrameInput> inputs;
    for (int i = 0; i < 1200; ++i) { inputs.push_back(randomInput(seed)); }
    for (int i = 0; i < 300; ++i) { FrameInput in[2] = {inputs[i * 2], inputs[i * 2 + 1]}; step(m, s, in); }
    MatchState snapshot = m; // Save
    for (int i = 300; i < 600; ++i) { FrameInput in[2] = {inputs[i * 2], inputs[i * 2 + 1]}; step(m, s, in); }
    uint64_t expected = hashState(m);
    m = snapshot; // Load
    for (int i = 300; i < 600; ++i) { FrameInput in[2] = {inputs[i * 2], inputs[i * 2 + 1]}; step(m, s, in); }
    CHECK(hashState(m) == expected);
}

static MatchState closeRange(const MatchSetup& s) {
    MatchState m = createMatch(s);
    m.f[0].pos = {Fx{}, Fx{}, Fx::ratio(-7, 10)};
    m.f[1].pos = {Fx{}, Fx{}, Fx::ratio(7, 10)};
    return m;
}

static void run(MatchState& m, const MatchSetup& s, FrameInput p0, FrameInput p1, int frames, EventSink* ev = nullptr) {
    for (int i = 0; i < frames; ++i) {
        FrameInput in[2] = {p0, p1};
        step(m, s, in, ev);
        p0.buttons &= BtnGuard; // nur Guard wird gehalten, Angriffe sind einzelne Drücke
        p1.buttons &= BtnGuard;
    }
}

static void testJabHits() {
    std::puts("jab hits");
    MatchSetup s = setup();
    MatchState m = closeRange(s);
    EventSink ev;
    run(m, s, {5, BtnLight}, {5, 0}, 8, &ev);
    CHECK(m.f[1].health < s.chars[1]->maxHealth);
    CHECK(m.f[1].state == FState::Hitstun);
    bool sawHit = false;
    for (auto& e : ev) sawHit |= e.kind == ImpactKind::Hit;
    CHECK(sawHit);
}

static void testBlock() {
    std::puts("block");
    MatchSetup s = setup();
    MatchState m = closeRange(s);
    run(m, s, {5, BtnLight}, {5, BtnGuard}, 8);
    CHECK(m.f[1].health == s.chars[1]->maxHealth);
    CHECK(m.f[1].state == FState::Blockstun);
}

static void testUltimateNeedsEssence() {
    std::puts("ultimate needs full essence");
    MatchSetup s = setup();
    MatchState m = closeRange(s);
    run(m, s, {5, BtnUltimate}, {5, 0}, 2);
    CHECK(m.f[0].state != FState::Attack);
    m = closeRange(s);
    m.f[0].essence = kEssenceMax;
    run(m, s, {5, BtnUltimate}, {5, 0}, 45);
    CHECK(m.realm.owner == 0);
    CHECK(m.f[0].essence == 0);
}

static void testWallBreak() {
    std::puts("wall break");
    MatchSetup s = setup();
    MatchState m = createMatch(s);
    EventSink ev;
    bool splat = false, broke = false;
    for (int hit = 0; hit < 20 && !broke; ++hit) {
        FighterState& f = m.f[1];
        f.pos = {s.stage.halfExtent - Fx::ratio(1, 2), Fx{}, Fx{}};
        f.vel = {Fx::ratio(6, 10), Fx{}, Fx{}};
        f.state = FState::Hitstun;
        f.stun = 20;
        run(m, s, {5, 0}, {5, 0}, 3, &ev);
        for (auto& e : ev) { splat |= e.kind == ImpactKind::WallSplat; broke |= e.kind == ImpactKind::WallBreak; }
    }
    CHECK(splat);
    CHECK(broke);
    CHECK(m.walls[0].broken);
}

static void testKnotPulls() {
    std::puts("technique: gravity knot pulls");
    MatchSetup s = setup();
    MatchState m = createMatch(s);
    m.f[0].essence = 500;
    Fx startDist = length(m.f[1].pos - m.f[0].pos);
    run(m, s, {5, BtnSpecial}, {5, 0}, 60);
    CHECK(m.f[0].essence == 350);
    CHECK(length(m.f[1].pos - m.f[0].pos) < startDist);
}

int main() {
    testDeterminism();
    testRollback();
    testJabHits();
    testBlock();
    testUltimateNeedsEssence();
    testWallBreak();
    testKnotPulls();
    if (g_failed) { std::printf("%d check(s) failed\n", g_failed); return EXIT_FAILURE; }
    std::puts("all tests passed");
    return EXIT_SUCCESS;
}
