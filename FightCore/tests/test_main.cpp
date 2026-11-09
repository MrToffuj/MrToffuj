// Tests ohne Framework für alle Kernsysteme aus der GDD.
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <vector>

#include "Techniques.h"
#include "fightcore/Simulation.h"

using namespace fc;
using namespace fc::techniques;

static int g_failed = 0;
#define CHECK(cond)                                                                                  \
    do {                                                                                             \
        if (!(cond)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } \
    } while (0)

static MatchSetup makeSetup(const BodyDef& b0, const TechniqueDef* t0, const BodyDef& b1, const TechniqueDef* t1) {
    MatchSetup s;
    s.loadout[0] = {&b0, t0, {}, {}};
    s.loadout[1] = {&b1, t1, {}, {}};
    s.finalize();
    return s;
}

static MatchState closeRange(const MatchSetup& s) {
    MatchState m = createMatch(s);
    m.f[0].pos = {Fx{}, Fx{}, Fx::ratio(-7, 10)};
    m.f[1].pos = {Fx{}, Fx{}, Fx::ratio(7, 10)};
    return m;
}

using Script = std::function<void(int frame, FrameInput& p0, FrameInput& p1)>;
static void run(MatchState& m, const MatchSetup& s, int frames, const Script& script, EventSink* ev = nullptr) {
    for (int i = 0; i < frames; ++i) {
        FrameInput in[2];
        script(i, in[0], in[1]);
        step(m, s, in, ev);
    }
}
static Script press(uint8_t b0, uint8_t b1 = 0) {
    return [=](int f, FrameInput& p0, FrameInput& p1) { if (f == 0) { p0.buttons = b0; p1.buttons = b1; } };
}
static bool saw(const EventSink& ev, ImpactKind k) {
    for (const auto& e : ev) if (e.kind == k) return true;
    return false;
}

static FrameInput randomInput(uint32_t& seed) {
    seed = seed * 1664525u + 1013904223u;
    return FrameInput{static_cast<uint8_t>(1 + (seed >> 8) % 9), static_cast<uint8_t>(seed >> 20), 0};
}

// ------------------------------------------------------------------ Tests

static void testBodiesBalanced() {
    std::puts("bodies: 10 Punkte");
    for (const BodyDef* b : {&bodies::heavy(), &bodies::speedster(), &bodies::caster(), &bodies::brawler(),
                             &bodies::technician(), &bodies::summoner(), &bodies::nullborn()})
        CHECK(b->valid());
    CHECK(deriveStats(bodies::heavy()).maxHealth > deriveStats(bodies::speedster()).maxHealth);
    CHECK(deriveStats(bodies::speedster()).dashSpeed > deriveStats(bodies::heavy()).dashSpeed);
    CHECK(deriveStats(bodies::caster()).ceMax > deriveStats(bodies::heavy()).ceMax);
}

static void testDeterminismAndRollback() {
    std::puts("determinism + rollback (fuzz, alle Buttons)");
    const MatchSetup s = makeSetup(bodies::heavy(), &nullraum(), bodies::caster(), &glutkern());
    MatchState a = createMatch(s), b = createMatch(s);
    a.f[0].realmMeter = b.f[0].realmMeter = kRealmMeterMax;
    a.f[1].realmMeter = b.f[1].realmMeter = kRealmMeterMax;
    uint32_t sa = 42, sb = 42;
    MatchState snapshot{};
    std::vector<FrameInput> log;
    for (int i = 0; i < 4000; ++i) {
        FrameInput ia[2] = {randomInput(sa), randomInput(sa)};
        FrameInput ib[2] = {randomInput(sb), randomInput(sb)};
        if (i == 1500) snapshot = a;
        if (i >= 1500) { log.push_back(ia[0]); log.push_back(ia[1]); }
        step(a, s, ia);
        step(b, s, ib);
    }
    CHECK(hashState(a) == hashState(b));
    MatchState r = snapshot;
    for (size_t i = 0; i < log.size(); i += 2) { FrameInput in[2] = {log[i], log[i + 1]}; step(r, s, in); }
    CHECK(hashState(r) == hashState(a));
}

static void testHitBlockParryGrab() {
    std::puts("hit / block / parry / grab");
    const MatchSetup s = makeSetup(bodies::brawler(), &glutkern(), bodies::brawler(), &glutkern());
    const int16_t full = s.stats[1].maxHealth;

    MatchState m = closeRange(s);
    run(m, s, 8, press(BtnLight));
    CHECK(m.f[1].health < full && m.f[1].state == FState::Hitstun);

    m = closeRange(s);
    // Guard lange gehalten (kein Parry-Fenster mehr)
    run(m, s, 18, [](int f, FrameInput& p0, FrameInput& p1) { if (f == 10) p0.buttons = BtnLight; p1.buttons = BtnGuard; });
    CHECK(m.f[1].health == full && m.f[1].state == FState::Blockstun);

    // Parry: Guard frisch gedrückt kurz vor dem Treffer
    m = closeRange(s);
    EventSink ev;
    run(m, s, 8, [](int f, FrameInput& p0, FrameInput& p1) { if (f == 0) p0.buttons = BtnLight; if (f >= 3) p1.buttons = BtnGuard; }, &ev);
    CHECK(saw(ev, ImpactKind::Parry));
    CHECK(m.f[1].realmMeter == 100 && m.f[0].state == FState::Hitstun);

    // Grab schlägt Guard
    m = closeRange(s);
    run(m, s, 10, [](int f, FrameInput& p0, FrameInput& p1) { if (f == 0) p0.buttons = BtnGrab; p1.buttons = f > 0 ? BtnGuard : 0; });
    CHECK(m.f[1].health < full);
}

static void testZeroSpark() {
    std::puts("nullfunke (perfektes Timing)");
    const MatchSetup s = makeSetup(bodies::caster(), &glutkern(), bodies::heavy(), &glutkern());
    MatchState probe = closeRange(s);
    run(probe, s, 10, press(BtnLight));
    const int32_t link = probe.f[0].linkFrame;
    CHECK(link > 0);

    auto attempt = [&](int32_t secondPress) {
        MatchState m = closeRange(s);
        EventSink ev;
        run(m, s, 40, [&](int f, FrameInput& p0, FrameInput&) { if (f == 0 || f == secondPress) p0.buttons = BtnLight; }, &ev);
        return saw(ev, ImpactKind::ZeroSpark);
    };
    CHECK(attempt(link));
    CHECK(!attempt(link + 3));
}

static void testSuperArmor() {
    std::puts("heavy body: super armor");
    const MatchSetup s = makeSetup(bodies::heavy(), &glutkern(), bodies::speedster(), &glutkern());
    MatchState m = closeRange(s);
    EventSink ev;
    run(m, s, 25, press(BtnHeavy, BtnLight), &ev);
    CHECK(m.f[0].health < s.stats[0].maxHealth); // Jab hat getroffen ...
    CHECK(m.f[1].health < s.stats[1].maxHealth); // ... aber Heavy kam trotzdem durch
}

static void testSkillsCooldownBurnout() {
    std::puts("skills: kosten, cooldown, burnout");
    const MatchSetup s = makeSetup(bodies::caster(), &glutkern(), bodies::heavy(), &glutkern());
    MatchState m = createMatch(s);
    const int32_t ce0 = m.f[0].ce;
    run(m, s, 2, press(BtnMod | BtnLight));
    CHECK(m.f[0].state == FState::Attack && m.f[0].ce < ce0 && m.f[0].cooldown[0] > 0);

    // Während Cooldown kein zweiter Cast
    m.f[0].state = FState::Idle; m.f[0].moveId = -1;
    run(m, s, 2, press(BtnMod | BtnLight));
    CHECK(m.f[0].state != FState::Attack);

    // Essenz auf 0 -> Burnout, Technik gesperrt
    m = createMatch(s);
    m.f[0].ce = 7000 - s.stats[0].ceRegen; // nach Regen exakt die Kosten von Glutfalter
    run(m, s, 2, press(BtnMod | BtnLight));
    CHECK(m.f[0].burnout > 0);
    m.f[0].state = FState::Idle; m.f[0].moveId = -1; m.f[0].ce = 50000;
    run(m, s, 2, press(BtnMod | BtnHeavy));
    CHECK(m.f[0].state != FState::Attack);
}

static void testApex() {
    std::puts("apex: 50 % Essenz, einmal pro Runde");
    const MatchSetup s = makeSetup(bodies::caster(), &nullraum(), bodies::heavy(), &glutkern());
    MatchState m = createMatch(s);
    const int32_t full = m.f[0].ce;
    run(m, s, 2, press(BtnUlt));
    CHECK(m.f[0].apexUsed && m.f[0].ce <= full / 2 + 100);
    m.f[0].state = FState::Idle; m.f[0].ce = full;
    run(m, s, 2, press(BtnUlt));
    CHECK(m.f[0].state != FState::Attack);
}

static void testRealmLockAndNullborn() {
    std::puts("realm: meter, lock, nullgeboren immun");
    MatchSetup s = makeSetup(bodies::technician(), &nullraum(), bodies::brawler(), &glutkern());
    MatchState m = createMatch(s);
    run(m, s, 2, press(BtnMod | BtnUlt));
    CHECK(m.f[0].state != FState::Attack); // Meter leer

    m = createMatch(s);
    m.f[0].realmMeter = kRealmMeterMax;
    EventSink ev;
    run(m, s, kRealmCastFrames + 3, press(BtnMod | BtnUlt), &ev);
    CHECK(m.realm.owner == 0 && saw(ev, ImpactKind::RealmOpen));
    CHECK(m.f[1].state == FState::Locked);

    s = makeSetup(bodies::technician(), &nullraum(), bodies::nullborn(), nullptr);
    m = createMatch(s);
    m.f[0].realmMeter = kRealmMeterMax;
    run(m, s, kRealmCastFrames + 3, press(BtnMod | BtnUlt));
    CHECK(m.realm.owner == 0 && m.f[1].state != FState::Locked);
}

static void testRealmClash() {
    std::puts("realm clash (input-duell)");
    const MatchSetup s = makeSetup(bodies::caster(), &glutkern(), bodies::caster(), &nullraum());
    MatchState m = createMatch(s);
    m.f[0].realmMeter = m.f[1].realmMeter = kRealmMeterMax;
    // P0 öffnet zuerst, P1 kontert 10 Frames später mit eigenem Realm und hämmert dann Light
    run(m, s, kRealmCastFrames + 12 + kClashFrames + 5, [](int f, FrameInput& p0, FrameInput& p1) {
        if (f == 0) p0.buttons = BtnMod | BtnUlt;
        if (f == 10) p1.buttons = BtnMod | BtnUlt;
        if (f > 75 && f % 2 == 0) p1.buttons = BtnLight;
    });
    CHECK(m.realm.owner == 1);
}

static void testWardBlocksSureHit() {
    std::puts("schutzkreis gegen sure-hit");
    const MatchSetup s = makeSetup(bodies::caster(), &glutkern(), bodies::technician(), &nullraum());
    auto runRealm = [&](bool ward) {
        MatchState m = createMatch(s);
        m.f[0].realmMeter = kRealmMeterMax;
        run(m, s, kRealmCastFrames + 120, [&](int f, FrameInput& p0, FrameInput& p1) {
            if (f == 0) p0.buttons = BtnMod | BtnUlt;
            if (ward && f > kRealmCastFrames + 2) p1.buttons = BtnUlt;
        });
        return m.f[1].health;
    };
    const int16_t without = runRealm(false), with = runRealm(true);
    CHECK(without < s.stats[1].maxHealth);
    CHECK(with > without);
}

static void testNullraumField() {
    std::puts("nullraum: distanzfeld blockt lights, nicht grabs");
    const MatchSetup s = makeSetup(bodies::brawler(), &glutkern(), bodies::caster(), &nullraum());
    MatchState m = closeRange(s);
    EventSink ev;
    run(m, s, 10, press(BtnLight), &ev);
    CHECK(saw(ev, ImpactKind::Nullified) && m.f[1].health == s.stats[1].maxHealth);

    m = closeRange(s);
    run(m, s, 12, press(BtnGrab));
    CHECK(m.f[1].health < s.stats[1].maxHealth);

    m = closeRange(s);
    m.f[1].burnout = 100;
    run(m, s, 10, press(BtnLight));
    CHECK(m.f[1].health < s.stats[1].maxHealth); // Schwäche: Burnout = kein Feld
}

static void testGlutkernBurn() {
    std::puts("glutkern: hitze-stacks + ignite");
    const MatchSetup s = makeSetup(bodies::caster(), &glutkern(), bodies::heavy(), &glutkern());
    MatchState m = createMatch(s);
    EventSink ev;
    run(m, s, 80, press(BtnMod | BtnLight), &ev); // Glutfalter
    CHECK(m.f[0].tech[0] >= 1);
    const int16_t hp = m.f[1].health;
    ev.clear();
    run(m, s, 60, [](int, FrameInput&, FrameInput&) {}, &ev);
    CHECK(m.f[1].health < hp && saw(ev, ImpactKind::Ignite));
}

static void testBurstFromHitstun() {
    std::puts("essenz-burst aus hitstun");
    const MatchSetup s = makeSetup(bodies::brawler(), &glutkern(), bodies::brawler(), &glutkern());
    MatchState m = closeRange(s);
    EventSink ev;
    run(m, s, 25, [](int f, FrameInput& p0, FrameInput& p1) {
        if (f == 0) p0.buttons = BtnHeavy;
        if (f == 18) p1.buttons = BtnGuard | BtnGrab;
        if (f > 18) p1.buttons = BtnGuard;
    }, &ev);
    CHECK(saw(ev, ImpactKind::Burst));
    CHECK(m.f[0].state == FState::Hitstun);
}

static void testWallBreak() {
    std::puts("zerstörbare wand");
    const MatchSetup s = makeSetup(bodies::brawler(), &glutkern(), bodies::brawler(), &glutkern());
    MatchState m = createMatch(s);
    EventSink ev;
    for (int hit = 0; hit < 20 && !m.walls[0].broken; ++hit) {
        FighterState& f = m.f[1];
        f.pos = {s.stage.halfExtent - Fx::ratio(1, 2), Fx{}, Fx{}};
        f.vel = {Fx::ratio(6, 10), Fx{}, Fx{}};
        f.state = FState::Hitstun;
        f.stun = 20;
        run(m, s, 3, [](int, FrameInput&, FrameInput&) {}, &ev);
    }
    CHECK(saw(ev, ImpactKind::WallSplat) && saw(ev, ImpactKind::WallBreak) && m.walls[0].broken);
}

static void testPacts() {
    std::puts("pakte: versteckter wert");
    PlaystyleProfile p;
    PactChoice regenLarge{true, PactCategory::Resource, PactSize::Large, 1};
    PactResult caster = evaluatePact(regenLarge, bodies::caster(), &glutkern(), p);
    PactChoice regenSmall{true, PactCategory::Resource, PactSize::Small, 1};
    PactResult heavy = evaluatePact(regenSmall, bodies::heavy(), &glutkern(), p);
    CHECK(caster.hiddenValue > heavy.hiddenValue);
    CHECK(caster.tier >= PactTier::High && caster.mods.openRealm);
    CHECK(caster.mods.ceRegenPct < 100);
    CHECK(heavy.tier == PactTier::Fair || heavy.tier == PactTier::TooCheap);

    // Ungenutzter Skill opfern = fast wertlos -> Backlash
    p.skillUsage[2] = 0;
    PactResult unused = evaluatePact({true, PactCategory::Technique, PactSize::Large, 2}, bodies::caster(), &glutkern(), p);
    CHECK(unused.tier == PactTier::TooCheap && unused.mods.ceLeak > 0);

    // Nullgeboren: begrenzter Pakt-Stack
    PactResult nb = evaluatePact({true, PactCategory::Body, PactSize::Large, 1}, bodies::nullborn(), nullptr, p);
    CHECK(nb.hiddenValue <= 12);

    // Pakt wirkt im Match: Restriktion + verbotener Skill
    MatchSetup s = makeSetup(bodies::caster(), &glutkern(), bodies::heavy(), &glutkern());
    s.loadout[0].pact = regenLarge;
    s.loadout[1].pact = {true, PactCategory::Technique, PactSize::Medium, 0};
    s.finalize();
    MatchState m = createMatch(s);
    run(m, s, 2, press(0, BtnMod | BtnLight));
    CHECK(m.f[1].state != FState::Attack);
}

int main() {
    testBodiesBalanced();
    testDeterminismAndRollback();
    testHitBlockParryGrab();
    testZeroSpark();
    testSuperArmor();
    testSkillsCooldownBurnout();
    testApex();
    testRealmLockAndNullborn();
    testRealmClash();
    testWardBlocksSureHit();
    testNullraumField();
    testGlutkernBurn();
    testBurstFromHitstun();
    testWallBreak();
    testPacts();
    if (g_failed) { std::printf("%d check(s) failed\n", g_failed); return EXIT_FAILURE; }
    std::puts("all tests passed");
    return EXIT_SUCCESS;
}
