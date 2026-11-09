// Plain test runner, no framework needed.
#include <algorithm>
#include <chrono>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <vector>

#include "Techniques.h"
#include <fstream>
#include <sstream>

#include "fightcore/Json.h"
#include "fightcore/Registry.h"
#include "FakeNetwork.h"
#include "fightcore/Net.h"
#include "fightcore/Session.h"
#include "fightcore/Simulation.h"

using namespace fc;
using namespace fc::techniques;

static int g_failed = 0;
#define CHECK(cond)                                                                                  \
    do {                                                                                             \
        if (!(cond)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } \
    } while (0)

static MatchSetup makeSetup(const BodyDef& b0, const TechniqueDef* t0, const BodyDef& b1, const TechniqueDef* t1,
                            const CompanionDef* assist0 = nullptr) {
    MatchSetup s;
    s.loadout[0] = {&b0, t0, {}, {}, assist0};
    s.loadout[1] = {&b1, t1, {}, {}, nullptr};
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
    return FrameInput{static_cast<uint8_t>(1 + (seed >> 8) % 9), static_cast<uint16_t>((seed >> 20) & 0x1FF), 0};
}


static void testBodiesBalanced() {
    std::puts("bodies: 10 points each");
    for (const BodyDef* b : {&bodies::heavy(), &bodies::speedster(), &bodies::caster(), &bodies::brawler(),
                             &bodies::technician(), &bodies::summoner(), &bodies::heavensBind()})
        CHECK(b->valid());
    CHECK(deriveStats(bodies::heavy()).maxHealth > deriveStats(bodies::speedster()).maxHealth);
    CHECK(deriveStats(bodies::speedster()).dashSpeed > deriveStats(bodies::heavy()).dashSpeed);
    CHECK(deriveStats(bodies::caster()).ceMax > deriveStats(bodies::heavy()).ceMax);
}

static void testDeterminismAndRollback() {
    std::puts("determinism + rollback (fuzzed input)");
    const MatchSetup s = makeSetup(bodies::heavy(), &boundless(), bodies::caster(), &calamityFlames());
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

static void testCompanionFuzz() {
    std::puts("determinism + rollback with companions");
    const MatchSetup s = makeSetup(bodies::summoner(), &shadowPuppets(), bodies::caster(), &calamityFlames(),
                                   &companions::stoneWarden());
    MatchState a = createMatch(s), b = createMatch(s);
    uint32_t sa = 99, sb = 99;
    MatchState snapshot{};
    std::vector<FrameInput> log;
    for (int i = 0; i < 4000; ++i) {
        FrameInput ia[2] = {randomInput(sa), randomInput(sa)};
        FrameInput ib[2] = {randomInput(sb), randomInput(sb)};
        if (i == 2000) snapshot = a;
        if (i >= 2000) { log.push_back(ia[0]); log.push_back(ia[1]); }
        step(a, s, ia);
        step(b, s, ib);
    }
    CHECK(hashState(a) == hashState(b));
    MatchState r = snapshot;
    for (size_t i = 0; i < log.size(); i += 2) { FrameInput in[2] = {log[i], log[i + 1]}; step(r, s, in); }
    CHECK(hashState(r) == hashState(a));
}

static void testMovementRelativeToOpponent() {
    std::puts("stick: up walks at the target, sideways circles");
    const MatchSetup s = makeSetup(bodies::brawler(), &calamityFlames(), bodies::brawler(), &calamityFlames());
    MatchState m = createMatch(s);
    const Fx start = length(m.f[1].pos - m.f[0].pos);
    run(m, s, 30, [](int, FrameInput& p0, FrameInput&) { p0.dir = 8; });
    CHECK(length(m.f[1].pos - m.f[0].pos) < start);

    m = createMatch(s);
    run(m, s, 60, [](int, FrameInput& p0, FrameInput&) { p0.dir = 6; });
    // circling keeps roughly the same distance
    CHECK(m.f[0].pos.x != Fx{});
    CHECK(abs(length(m.f[1].pos - m.f[0].pos) - start) < Fx::ratio(1, 2));
}

static void testHitBlockParryGrab() {
    std::puts("hit / block / parry / grab");
    const MatchSetup s = makeSetup(bodies::brawler(), &calamityFlames(), bodies::brawler(), &calamityFlames());
    const int16_t full = s.stats[1].maxHealth;

    MatchState m = closeRange(s);
    run(m, s, 8, press(BtnLight));
    CHECK(m.f[1].health < full && m.f[1].state == FState::Hitstun);

    m = closeRange(s);
    // guard held long enough that it is no parry anymore
    run(m, s, 18, [](int f, FrameInput& p0, FrameInput& p1) { if (f == 10) p0.buttons = BtnLight; p1.buttons = BtnGuard; });
    CHECK(m.f[1].health == full && m.f[1].state == FState::Blockstun);

    // parry: guard pressed right before the hit
    m = closeRange(s);
    EventSink ev;
    run(m, s, 8, [](int f, FrameInput& p0, FrameInput& p1) { if (f == 0) p0.buttons = BtnLight; if (f >= 3) p1.buttons = BtnGuard; }, &ev);
    CHECK(saw(ev, ImpactKind::Parry));
    CHECK(m.f[1].realmMeter == 100 && m.f[0].state == FState::Hitstun);

    // grab beats guard
    m = closeRange(s);
    run(m, s, 10, [](int f, FrameInput& p0, FrameInput& p1) { if (f == 0) p0.buttons = BtnGrab; p1.buttons = f > 0 ? BtnGuard : 0; });
    CHECK(m.f[1].health < full);
}

static void testBlackSpark() {
    std::puts("black spark timing");
    const MatchSetup s = makeSetup(bodies::caster(), &calamityFlames(), bodies::heavy(), &calamityFlames());
    MatchState probe = closeRange(s);
    run(probe, s, 10, press(BtnLight));
    const int32_t link = probe.f[0].linkFrame;
    CHECK(link > 0);

    auto attempt = [&](int32_t secondPress) {
        MatchState m = closeRange(s);
        EventSink ev;
        run(m, s, 40, [&](int f, FrameInput& p0, FrameInput&) { if (f == 0 || f == secondPress) p0.buttons = BtnLight; }, &ev);
        return saw(ev, ImpactKind::BlackSpark);
    };
    CHECK(attempt(link));
    CHECK(!attempt(link + 3));
}

static void testSuperArmor() {
    std::puts("heavy body: super armor");
    const MatchSetup s = makeSetup(bodies::heavy(), &calamityFlames(), bodies::speedster(), &calamityFlames());
    MatchState m = closeRange(s);
    EventSink ev;
    run(m, s, 25, press(BtnHeavy, BtnLight), &ev);
    CHECK(m.f[0].health < s.stats[0].maxHealth); // the jab connected ...
    CHECK(m.f[1].health < s.stats[1].maxHealth); // ... and the heavy still went through
}

static void testSkillsCooldownBurnout() {
    std::puts("skills: cost, cooldown, burnout");
    const MatchSetup s = makeSetup(bodies::caster(), &calamityFlames(), bodies::heavy(), &calamityFlames());
    MatchState m = createMatch(s);
    const int32_t ce0 = m.f[0].ce;
    run(m, s, 2, press(BtnMod | BtnLight));
    CHECK(m.f[0].state == FState::Attack && m.f[0].ce < ce0 && m.f[0].cooldown[0] > 0);

    // no second cast during the cooldown
    m.f[0].state = FState::Idle; m.f[0].moveId = -1;
    run(m, s, 2, press(BtnMod | BtnLight));
    CHECK(m.f[0].state != FState::Attack);

    // spending the last CE burns you out
    m = createMatch(s);
    m.f[0].ce = 7000 - s.stats[0].ceRegen; // exactly Ember Moths after regen
    run(m, s, 2, press(BtnMod | BtnLight));
    CHECK(m.f[0].burnout > 0);
    m.f[0].state = FState::Idle; m.f[0].moveId = -1; m.f[0].ce = 50000;
    run(m, s, 2, press(BtnMod | BtnHeavy));
    CHECK(m.f[0].state != FState::Attack);
}

static void testMaxOutput() {
    std::puts("max output: half the CE, once per round");
    const MatchSetup s = makeSetup(bodies::caster(), &boundless(), bodies::heavy(), &calamityFlames());
    MatchState m = createMatch(s);
    const int32_t full = m.f[0].ce;
    run(m, s, 2, press(BtnUlt));
    CHECK(m.f[0].maxOutputUsed && m.f[0].ce <= full / 2 + 100);
    m.f[0].state = FState::Idle; m.f[0].ce = full;
    run(m, s, 2, press(BtnUlt));
    CHECK(m.f[0].state != FState::Attack);
}

static void testRealmLockAndHeavensBind() {
    std::puts("realm: meter, lock, heaven's bind is immune");
    MatchSetup s = makeSetup(bodies::technician(), &boundless(), bodies::brawler(), &calamityFlames());
    MatchState m = createMatch(s);
    run(m, s, 2, press(BtnMod | BtnUlt));
    CHECK(m.f[0].state != FState::Attack); // empty meter

    m = createMatch(s);
    m.f[0].realmMeter = kRealmMeterMax;
    EventSink ev;
    run(m, s, kRealmCastFrames + 3, press(BtnMod | BtnUlt), &ev);
    CHECK(m.f[0].realm.active() && saw(ev, ImpactKind::RealmOpen));
    CHECK(m.f[1].state == FState::Locked);

    s = makeSetup(bodies::technician(), &boundless(), bodies::heavensBind(), nullptr);
    m = createMatch(s);
    m.f[0].realmMeter = kRealmMeterMax;
    run(m, s, kRealmCastFrames + 3, press(BtnMod | BtnUlt));
    CHECK(m.f[0].realm.active() && m.f[1].state != FState::Locked);
}

static void testRealmClash() {
    std::puts("realm clash");
    const MatchSetup s = makeSetup(bodies::caster(), &calamityFlames(), bodies::caster(), &boundless());
    MatchState m = createMatch(s);
    m.f[0].realmMeter = m.f[1].realmMeter = kRealmMeterMax;
    // P1 opens first, P2 answers 10 frames later and mashes
    run(m, s, kRealmCastFrames + 12 + kClashFrames + 5, [](int f, FrameInput& p0, FrameInput& p1) {
        if (f == 0) p0.buttons = BtnMod | BtnUlt;
        if (f == 10) p1.buttons = BtnMod | BtnUlt;
        if (f > 75 && f % 2 == 0) p1.buttons = BtnLight;
    });
    CHECK(m.f[1].realm.active() && !m.f[0].realm.active());
}

static void testWardBlocksGuaranteedHit() {
    std::puts("simple realm vs guaranteed hits");
    const MatchSetup s = makeSetup(bodies::caster(), &calamityFlames(), bodies::technician(), &boundless());
    auto runRealm = [&](bool simpleRealm) {
        MatchState m = createMatch(s);
        m.f[0].realmMeter = kRealmMeterMax;
        run(m, s, kRealmCastFrames + 120, [&](int f, FrameInput& p0, FrameInput& p1) {
            if (f == 0) p0.buttons = BtnMod | BtnUlt;
            if (simpleRealm && f > kRealmCastFrames + 2) p1.buttons = BtnUlt;
        });
        return m.f[1].health;
    };
    const int16_t without = runRealm(false), with = runRealm(true);
    CHECK(without < s.stats[1].maxHealth);
    CHECK(with > without);
}

static void testBoundlessField() {
    std::puts("boundless: infinite gap blocks lights, not grabs");
    const MatchSetup s = makeSetup(bodies::brawler(), &calamityFlames(), bodies::caster(), &boundless());
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
    CHECK(m.f[1].health < s.stats[1].maxHealth); // burned out = no gap
}

static void testCalamityFlamesBurn() {
    std::puts("calamity flames: heat stacks + ignite");
    const MatchSetup s = makeSetup(bodies::caster(), &calamityFlames(), bodies::heavy(), &calamityFlames());
    MatchState m = createMatch(s);
    EventSink ev;
    run(m, s, 80, press(BtnMod | BtnLight), &ev); // Ember Moths
    CHECK(m.f[0].tech[0] >= 1);
    const int16_t hp = m.f[1].health;
    ev.clear();
    run(m, s, 60, [](int, FrameInput&, FrameInput&) {}, &ev);
    CHECK(m.f[1].health < hp && saw(ev, ImpactKind::Ignite));
}

static void testBurstFromHitstun() {
    std::puts("ce burst out of hitstun");
    const MatchSetup s = makeSetup(bodies::brawler(), &calamityFlames(), bodies::brawler(), &calamityFlames());
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

static void testScenery() {
    std::puts("map: pillars break, barrier holds");
    const MatchSetup s = makeSetup(bodies::brawler(), &calamityFlames(), bodies::brawler(), &calamityFlames());
    MatchState m = createMatch(s);
    EventSink ev;
    // keep throwing P2 into the first pillar
    const ObstacleDef& pillar = mapOf(s).obstacles[0];
    for (int hit = 0; hit < 20 && !m.obstacles[0].broken; ++hit) {
        FighterState& f = m.f[1];
        f.pos = {pillar.center.x - pillar.halfX - Fx::ratio(15, 10), Fx{}, pillar.center.z};
        f.vel = {Fx::ratio(6, 10), Fx{}, Fx{}};
        f.state = FState::Hitstun;
        f.stun = 20;
        run(m, s, 3, [](int, FrameInput&, FrameInput&) {}, &ev);
    }
    CHECK(saw(ev, ImpactKind::WallSplat) && saw(ev, ImpactKind::WallBreak) && m.obstacles[0].broken);

    // The barrier never breaks and nobody gets out
    ev.clear();
    for (int hit = 0; hit < 30; ++hit) {
        FighterState& f = m.f[1];
        f.pos = {Fx{}, Fx{}, mapOf(s).radius - Fx::fromInt(2)};
        f.vel = {Fx{}, Fx{}, Fx::fromInt(1)};
        f.state = FState::Hitstun;
        f.stun = 20;
        run(m, s, 4, [](int, FrameInput&, FrameInput&) {}, &ev);
        CHECK(length(Vec3{f.pos.x, Fx{}, f.pos.z}) <= mapOf(s).radius);
    }
    CHECK(saw(ev, ImpactKind::BarrierHit));
}

static void testOaths() {
    std::puts("oaths: hidden value");
    PlaystyleProfile p;
    OathChoice regenLarge{true, OathCategory::Resource, OathSize::Large, 1};
    OathResult caster = evaluateOath(regenLarge, bodies::caster(), &calamityFlames(), p);
    OathChoice regenSmall{true, OathCategory::Resource, OathSize::Small, 1};
    OathResult heavy = evaluateOath(regenSmall, bodies::heavy(), &calamityFlames(), p);
    CHECK(caster.hiddenValue > heavy.hiddenValue);
    CHECK(caster.tier >= OathTier::High && caster.mods.openRealm);
    CHECK(caster.mods.ceRegenPct < 100);
    CHECK(heavy.tier == OathTier::Fair || heavy.tier == OathTier::TooCheap);

    // giving up a skill you never use backfires
    p.skillUsage[2] = 0;
    OathResult unused = evaluateOath({true, OathCategory::Technique, OathSize::Large, 2}, bodies::caster(), &calamityFlames(), p);
    CHECK(unused.tier == OathTier::TooCheap && unused.mods.ceLeak > 0);

    // heaven's bind has a capped oath value
    OathResult nb = evaluateOath({true, OathCategory::Body, OathSize::Large, 1}, bodies::heavensBind(), nullptr, p);
    CHECK(nb.hiddenValue <= 12);

    // the restriction applies in the match
    MatchSetup s = makeSetup(bodies::caster(), &calamityFlames(), bodies::heavy(), &calamityFlames());
    s.loadout[0].oath = regenLarge;
    s.loadout[1].oath = {true, OathCategory::Technique, OathSize::Medium, 0};
    s.finalize();
    MatchState m = createMatch(s);
    run(m, s, 2, press(0, BtnMod | BtnLight));
    CHECK(m.f[1].state != FState::Attack);
}


// ---- companions

static Script assist(uint8_t dir, int at = 0) {
    return [=](int f, FrameInput& p0, FrameInput&) { if (f == at) { p0.buttons = BtnAssist; p0.dir = dir; } };
}

static void testSummonerAssist() {
    std::puts("companions: summoner assist, 60% cap, upkeep, cooldown");
    const MatchSetup s = makeSetup(bodies::summoner(), &calamityFlames(), bodies::brawler(), &calamityFlames(), &companions::bladeWolf());
    MatchState m = createMatch(s);
    CHECK(m.f[0].comp[0].onField());
    CHECK(m.f[1].comp[0].roster < 0); // brawlers get none

    // upkeep slows down CE regen
    m.f[0].ce = 0;
    run(m, s, 60, [](int, FrameInput&, FrameInput&) {});
    CHECK(m.f[0].ce == 60 * (s.stats[0].ceRegen - companions::bladeWolf().upkeep));

    m = closeRange(s);
    EventSink ev;
    run(m, s, 60, assist(5), &ev);
    const int dmg = s.stats[1].maxHealth - m.f[1].health;
    CHECK(dmg > 0);
    CHECK(dmg <= companions::bladeWolf().moves[0].hit.damage * kCompanionDamagePct / 100);
    CHECK(m.f[0].bond == 1);

    // can't call it again right away
    const int16_t hp = m.f[1].health;
    run(m, s, 40, assist(5));
    CHECK(m.f[1].health == hp);
}

static void testAssistRoles() {
    std::puts("companions: stick picks the role");
    const MatchSetup s = makeSetup(bodies::summoner(), &calamityFlames(), bodies::brawler(), &calamityFlames(), &companions::bladeWolf());
    const std::pair<uint8_t, AssistRole> cases[] = {
        {5, AssistRole::Attack}, {8, AssistRole::Pressure}, {6, AssistRole::AntiAir}, {2, AssistRole::Protect}};
    for (auto [dir, role] : cases) {
        MatchState m = createMatch(s);
        run(m, s, 2, assist(dir));
        CHECK(m.f[0].comp[0].action == static_cast<int8_t>(role));
    }
}

static void testLeash() {
    std::puts("companions: leash when the owner gets hit");
    const MatchSetup s = makeSetup(bodies::summoner(), &calamityFlames(), bodies::brawler(), &calamityFlames(), &companions::sparkOwl());
    MatchState m = closeRange(s);
    run(m, s, 3, [](int f, FrameInput& p0, FrameInput& p1) {
        if (f == 0) { p0.buttons = BtnAssist; p1.buttons = BtnLight; }
    });
    CHECK(m.f[0].comp[0].action >= 0);
    run(m, s, 6, [](int, FrameInput&, FrameInput&) {});
    CHECK(m.f[0].state == FState::Hitstun);
    CHECK(m.f[0].comp[0].action < 0 && m.f[0].comp[0].cooldown > 0);
}

static void testGuardShield() {
    std::puts("companions: guard type blocks for the owner");
    const MatchSetup s = makeSetup(bodies::summoner(), &calamityFlames(), bodies::brawler(), &calamityFlames(), &companions::stoneWarden());
    MatchState m = closeRange(s);
    run(m, s, 20, assist(2));
    const int16_t compHp = m.f[0].comp[0].hp;
    run(m, s, 10, press(0, BtnLight));
    CHECK(m.f[0].health == s.stats[0].maxHealth);
    CHECK(m.f[0].comp[0].hp < compHp);
}

static void testCompanionDownAndRevive() {
    std::puts("companions: goes down, comes back after 15 s for CE");
    const MatchSetup s = makeSetup(bodies::summoner(), &calamityFlames(), bodies::brawler(), &calamityFlames(), &companions::bladeWolf());
    MatchState m = createMatch(s);
    m.f[0].bond = 3;
    EventSink ev;
    {
        // the enemy punches the companion standing right in front of it
        m.f[0].comp[0].hp = 10;
        m.f[0].comp[0].pos = m.f[1].pos - m.f[1].facing * Fx::ratio(-9, 10) + Vec3{Fx{}, Fx::ratio(6, 10), Fx{}};
        m.f[0].pos = {Fx{}, Fx{}, Fx::fromInt(-6)};
        FrameInput in[2] = {{}, {5, BtnLight, 0}};
        step(m, s, in, &ev);
        // pin it there for the active frames
        for (int i = 0; i < 8; ++i) {
            m.f[0].comp[0].pos = m.f[1].pos + m.f[1].facing * Fx::ratio(9, 10) + Vec3{Fx{}, Fx::ratio(6, 10), Fx{}};
            FrameInput none[2] = {};
            step(m, s, none, &ev);
        }
    }
    CHECK(saw(ev, ImpactKind::CompanionDown));
    CHECK(m.f[0].comp[0].down > 0 && m.f[0].bond == 0);
    m.f[0].ce = 0;
    run(m, s, 900, [](int, FrameInput&, FrameInput&) {});
    CHECK(!m.f[0].comp[0].onField()); // 15 s are up but there is not enough CE yet
    run(m, s, 400, [](int, FrameInput&, FrameInput&) {});
    CHECK(m.f[0].comp[0].onField());
    CHECK(m.f[0].ce < 400 * s.stats[0].ceRegen); // the revive cost CE
}

static void testShadowMenagerie() {
    std::puts("shadow puppets: summon, max two, the dead stay dead");
    const MatchSetup s = makeSetup(bodies::caster(), &shadowPuppets(), bodies::brawler(), &calamityFlames());
    MatchState m = createMatch(s);
    auto skill = [&](uint16_t btn) {
        m.f[0].cooldown = {0, 0, 0};
        run(m, s, 25, press(BtnMod | btn));
    };
    skill(BtnLight);  // hounds
    skill(BtnHeavy);  // wing
    int onField = 0;
    for (const CompanionState& c : m.f[0].comp) onField += c.onField();
    CHECK(onField == 2);
    skill(BtnGrab);   // toad replaces the oldest one
    onField = 0;
    for (const CompanionState& c : m.f[0].comp) onField += c.onField();
    CHECK(onField == 2);
    TechContext ctx{m, s, 0, nullptr};
    CHECK(ctx.companionSlot(0) < 0 && ctx.companionSlot(2) >= 0);

    // toad dies -> its skill is locked for the round
    const int slot = ctx.companionSlot(2);
    m.f[0].comp[slot].hp = 1;
    m.f[0].comp[slot].pos = m.f[1].pos + m.f[1].facing * Fx::ratio(9, 10) + Vec3{Fx{}, Fx::ratio(6, 10), Fx{}};
    m.f[1].state = FState::Idle;
    FrameInput in[2] = {{}, {5, BtnLight, 0}};
    step(m, s, in);
    for (int i = 0; i < 8; ++i) {
        m.f[0].comp[slot].pos = m.f[1].pos + m.f[1].facing * Fx::ratio(9, 10) + Vec3{Fx{}, Fx::ratio(6, 10), Fx{}};
        FrameInput none[2] = {};
        step(m, s, none);
    }
    CHECK(ctx.companionLost(2));
    m.f[0].state = FState::Idle; m.f[0].cooldown = {0, 0, 0};
    run(m, s, 2, press(BtnMod | BtnGrab));
    CHECK(m.f[0].state != FState::Attack);
}

static void testBondLinkAttack() {
    std::puts("bond level 2 link attack");
    const MatchSetup s = makeSetup(bodies::summoner(), &calamityFlames(), bodies::heavy(), &calamityFlames(), &companions::bladeWolf());
    MatchState m = closeRange(s);
    m.f[0].bond = 3;
    CHECK(bondLevel(m.f[0]) == 2);
    EventSink ev;
    // assist during the hit confirm
    run(m, s, 20, [](int f, FrameInput& p0, FrameInput&) {
        if (f == 0) p0.buttons = BtnLight;
        if (f == 8) p0.buttons = BtnAssist;
    }, &ev);
    bool link = false;
    for (const auto& e : ev) link |= e.kind == ImpactKind::CompanionCall && e.tag >= 10;
    CHECK(link);
    CHECK(m.f[0].bond == 4);
}

static void testRealmHitsCompanions() {
    std::puts("companions are not realm immune");
    const MatchSetup s = makeSetup(bodies::caster(), &calamityFlames(), bodies::summoner(), &calamityFlames());
    MatchState m = createMatch(s);
    m.f[0].realmMeter = kRealmMeterMax;
    const int16_t hp = m.f[1].comp[0].hp;
    run(m, s, kRealmCastFrames + 40, press(BtnMod | BtnUlt));
    CHECK(m.f[1].comp[0].hp < hp);
}

static void testCompanionOaths() {
    std::puts("oaths: companion sacrifices, oath spirit, the adaptor");
    PlaystyleProfile p;
    // no companions, nothing to give
    OathResult none = evaluateOath({true, OathCategory::Companion, OathSize::Large, 2}, bodies::brawler(), &calamityFlames(), p);
    CHECK(none.tier == OathTier::TooCheap);
    // a summoner giving up a slot overpays
    OathResult big = evaluateOath({true, OathCategory::Companion, OathSize::Large, 1}, bodies::summoner(), &calamityFlames(), p);
    CHECK(big.tier == OathTier::Overpaid && big.mods.bonusCompanion == 2);
    // everyone else only gets a companion from a timed oath
    OathResult timed = evaluateOath({true, OathCategory::Time, OathSize::Large, 0}, bodies::brawler(), &calamityFlames(), p);
    CHECK(timed.tier >= OathTier::High && timed.mods.bonusCompanion == 1 && timed.mods.durationFrames > 0);

    MatchSetup s = makeSetup(bodies::brawler(), &calamityFlames(), bodies::heavy(), &calamityFlames());
    s.loadout[0].oath = {true, OathCategory::Time, OathSize::Large, 0};
    s.finalize();
    MatchState m = createMatch(s);
    CHECK(m.f[0].comp[0].onField() && m.f[0].comp[0].expires > 0);
    run(m, s, 3601, [](int, FrameInput&, FrameInput&) {});
    CHECK(!m.f[0].comp[0].onField());

    // the overpaid oath brings The Adaptor
    s = makeSetup(bodies::summoner(), &calamityFlames(), bodies::heavy(), &calamityFlames());
    s.loadout[0].oath = {true, OathCategory::Companion, OathSize::Large, 1};
    s.finalize();
    m = createMatch(s);
    int slot = -1;
    for (int i = 0; i < kCompanionSlots; ++i)
        if (m.f[0].comp[i].onField() && companionDef(s, 0, m.f[0].comp[i]).adaptive) slot = i;
    CHECK(slot >= 0);
}

static void testRollbackSession() {
    std::puts("rollback: two peers, 4 frames of delay");
    const MatchSetup s = makeSetup(bodies::summoner(), &shadowPuppets(), bodies::caster(), &boundless());
    // reference run with every input known up front
    std::vector<FrameInput> in[2];
    uint32_t seed = 1234;
    for (int f = 0; f < 600; ++f) { in[0].push_back(randomInput(seed)); in[1].push_back(randomInput(seed)); }
    MatchState ref = createMatch(s);
    for (int f = 0; f < 600; ++f) { FrameInput i[2] = {in[0][f], in[1][f]}; step(ref, s, i); }

    // peer A plays P1, peer B plays P2, remote inputs arrive 4 frames late
    constexpr int kDelay = 4;
    Session a(s), b(s);
    for (int f = 0; f < 600; ++f) {
        a.setInput(0, f, in[0][f]);
        b.setInput(1, f, in[1][f]);
        if (f >= kDelay) {
            a.setInput(1, f - kDelay, in[1][f - kDelay]);
            b.setInput(0, f - kDelay, in[0][f - kDelay]);
        }
        a.advance();
        b.advance();
    }
    for (int f = 600 - kDelay; f < 600; ++f) { a.setInput(1, f, in[1][f]); b.setInput(0, f, in[0][f]); }
    a.applyRollback();
    b.applyRollback();
    CHECK(a.frame() == 600 && b.frame() == 600);
    CHECK(hashState(a.state()) == hashState(ref));
    CHECK(hashState(b.state()) == hashState(ref));
    CHECK(a.rollbackCount() > 0 && a.lastRollbackDepth() <= kDelay);
}

// ---- modes, teams, bots

static FrameInput none20[kMaxFighters] = {};

static void testTeamsNoFriendlyFire() {
    std::puts("2v2: teams, no friendly fire, elimination");
    Loadout humans[2];
    humans[0] = {&bodies::brawler(), &calamityFlames(), {}, {}, nullptr, 0};
    humans[1] = {&bodies::brawler(), &calamityFlames(), {}, {}, nullptr, 0};
    MatchSetup s = buildMatch(GameMode::Teams, humans, 2, 1, 7);
    CHECK(s.count == 4);
    int team0 = 0;
    for (int i = 0; i < 4; ++i) team0 += s.loadout[i].team == 0;
    CHECK(team0 == 2);
    CHECK(!s.loadout[0].bot && s.loadout[3].bot);
    // turn bots off so only our inputs matter
    for (int i = 0; i < 4; ++i) s.loadout[i].bot = false;

    MatchState m = createMatch(s);
    // teammate right in front, the jab must not hurt
    m.f[1].pos = m.f[0].pos + Vec3{Fx{}, Fx{}, Fx::ratio(14, 10)};
    m.f[0].facing = {Fx{}, Fx{}, Fx::fromInt(1)};
    const int16_t hp = m.f[1].health;
    FrameInput in[kMaxFighters] = {};
    in[0].buttons = BtnLight;
    step(m, s, in);
    for (int f = 0; f < 10; ++f) step(m, s, none20);
    CHECK(m.f[1].health == hp);

    // wipe team 1
    for (int i = 0; i < 4; ++i)
        if (m.f[i].team == 1) m.f[i].health = 0;
    step(m, s, none20);
    CHECK(m.winner == 0);
}

static void testLockOn() {
    std::puts("lock-on: L3 locks and lets go, flick switches, free movement follows the stick");
    MatchSetup s = buildMatch(GameMode::Brawl, nullptr, 0, 1, 3);
    for (int i = 0; i < s.count; ++i) { s.loadout[i].bot = false; s.loadout[i].lockOnAtStart = false; }
    MatchState m = createMatch(s);
    CHECK(!m.f[0].lockOn);

    // free movement: stick to world +x, the fighter turns and walks that way
    const Fx x0 = m.f[0].pos.x;
    FrameInput in[kMaxFighters] = {};
    in[0].moveX = 127;
    for (int f = 0; f < 20; ++f) step(m, s, in);
    CHECK(m.f[0].pos.x > x0 && m.f[0].facing.x > Fx::ratio(9, 10));

    // move next to an enemy and lock on
    const int enemy = 5;
    m.f[enemy].pos = m.f[0].pos + Vec3{Fx::fromInt(3), Fx{}, Fx{}};
    FrameInput lock[kMaxFighters] = {};
    lock[0].buttons = BtnTarget;
    step(m, s, lock);
    CHECK(m.f[0].lockOn && m.f[0].target == enemy);
    step(m, s, none20);

    FrameInput flick[kMaxFighters] = {};
    flick[0].buttons = BtnTargetNext;
    step(m, s, flick);
    CHECK(m.f[0].lockOn && m.f[0].target != enemy);
    step(m, s, none20);

    step(m, s, lock);
    CHECK(!m.f[0].lockOn);
}

static void testBotFightsBack() {
    std::puts("bots attack an idle player");
    Loadout human{&bodies::brawler(), &calamityFlames(), {}, {}, nullptr, 0};
    MatchSetup s = buildMatch(GameMode::Duel, &human, 1, 2, 11);
    CHECK(s.loadout[1].bot);
    MatchState m = createMatch(s);
    for (int f = 0; f < 900 && m.winner == kNoWinner; ++f) step(m, s, none20);
    CHECK(m.f[0].health < s.stats[0].maxHealth);
}

static void testBrawl20Bots() {
    std::puts("brawl: 20 bots, respawns, determinism, rollback, cost");
    const MatchSetup s = buildMatch(GameMode::Brawl, nullptr, 0, 1, 2026);
    CHECK(s.count == kMaxFighters);
    MatchState a = createMatch(s), b = createMatch(s);
    EventSink ev;
    bool respawned = false;
    MatchState snapshot{};
    const auto t0 = std::chrono::steady_clock::now();
    for (int f = 0; f < 6000; ++f) {
        if (f == 3000) snapshot = a;
        ev.clear();
        step(a, s, none20, &ev);
        respawned |= saw(ev, ImpactKind::Respawn);
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    for (int f = 0; f < 6000; ++f) step(b, s, none20);
    CHECK(hashState(a) == hashState(b));
    MatchState r = snapshot;
    for (int f = 3000; f < 6000; ++f) step(r, s, none20);
    CHECK(hashState(r) == hashState(a));

    int kos = 0;
    for (int i = 0; i < s.count; ++i) kos += a.f[i].kos;
    CHECK(kos > 0 && respawned);
    std::printf("  20 fighters: %.3f ms per frame, %d KOs in 100 s\n", ms / 6000.0, kos);
    CHECK(ms / 6000.0 < 1.0);

    // Open brawl: no time limit, no lives, it just keeps going
    MatchState late = createMatch(s);
    late.frame = 1000000;
    for (int f = 0; f < 10; ++f) step(late, s, none20);
    CHECK(late.winner == kNoWinner);
}

static void testSessionTeams() {
    std::puts("rollback: 2v2 with two online humans and two bots");
    Loadout humans[2] = {{&bodies::speedster(), &boundless(), {}, {}, nullptr, 0},
                         {&bodies::heavy(), &calamityFlames(), {}, {}, nullptr, 1}};
    const MatchSetup s = buildMatch(GameMode::Teams, humans, 2, 1, 5);
    std::vector<FrameInput> in[2];
    uint32_t seed = 77;
    for (int f = 0; f < 600; ++f) { in[0].push_back(randomInput(seed)); in[1].push_back(randomInput(seed)); }
    MatchState ref = createMatch(s);
    for (int f = 0; f < 600; ++f) { FrameInput i[kMaxFighters] = {in[0][f], in[1][f]}; step(ref, s, i); }

    constexpr int kDelay = 5;
    Session a(s), b(s);
    for (int f = 0; f < 600; ++f) {
        a.setInput(0, f, in[0][f]);
        b.setInput(1, f, in[1][f]);
        if (f >= kDelay) { a.setInput(1, f - kDelay, in[1][f - kDelay]); b.setInput(0, f - kDelay, in[0][f - kDelay]); }
        a.advance();
        b.advance();
    }
    for (int f = 600 - kDelay; f < 600; ++f) { a.setInput(1, f, in[1][f]); b.setInput(0, f, in[0][f]); }
    a.applyRollback();
    b.applyRollback();
    CHECK(hashState(a.state()) == hashState(ref) && hashState(b.state()) == hashState(ref));
}

// ---- custom mode

static ModeRules customRules(Objective o, int teams) {
    ModeRules r = rulesFor(GameMode::Custom);
    r.objective = o;
    r.teamCount = static_cast<uint8_t>(teams);
    return r;
}

static void park(MatchState& m, const MatchSetup& s) {
    // Everyone stands far apart, out of the way
    for (int i = 0; i < m.count; ++i) {
        m.f[i].pos = {Fx::fromInt(-60 + (i % 10) * 12), Fx{}, Fx::fromInt(i < 10 ? 55 : -55)};
        (void)s;
    }
}

static void testCaptureTheFlag() {
    std::puts("custom: capture the flag");
    MatchSetup s = buildMatch(GameMode::Custom, customRules(Objective::CaptureTheFlag, 2), nullptr, 0, 1, 9);
    for (int i = 0; i < s.count; ++i) s.loadout[i].bot = false;
    MatchState m = createMatch(s);
    park(m, s);
    int runner = -1;
    for (int i = 0; i < m.count && runner < 0; ++i) if (m.f[i].team == 0) runner = i;
    EventSink ev;
    // Grab team 1's flag ...
    m.f[runner].pos = m.objective.flags[1].pos;
    step(m, s, none20, &ev);
    CHECK(m.objective.flags[1].carrier == runner && saw(ev, ImpactKind::FlagTaken));
    // ... carriers can't use their technique ...
    m.f[runner].ce = s.stats[runner].ceMax;
    FrameInput in[kMaxFighters] = {};
    in[runner].buttons = BtnMod | BtnLight;
    step(m, s, in);
    CHECK(m.f[runner].state != FState::Attack || m.f[runner].moveId < kTechMoveOffset);
    // ... and bring it home
    m.f[runner].state = FState::Idle;
    m.f[runner].pos = m.objective.flags[0].pos;
    ev.clear();
    step(m, s, none20, &ev);
    CHECK(saw(ev, ImpactKind::FlagCaptured) && m.objective.score[0] == 1 && m.objective.flags[1].home);
}

static void testOddball() {
    std::puts("custom: oddball, 4 teams");
    MatchSetup s = buildMatch(GameMode::Custom, customRules(Objective::Oddball, 4), nullptr, 0, 1, 4);
    int perTeam[4] = {};
    for (int i = 0; i < s.count; ++i) { s.loadout[i].bot = false; ++perTeam[s.loadout[i].team]; }
    CHECK(perTeam[0] == 5 && perTeam[3] == 5);
    MatchState m = createMatch(s);
    park(m, s);
    m.f[7].pos = m.objective.ballPos;
    for (int f = 0; f < 181; ++f) step(m, s, none20);
    CHECK(m.objective.ballCarrier == 7 && m.objective.score[m.f[7].team] == 3);
    m.f[7].health = 0;
    step(m, s, none20);
    CHECK(m.objective.ballCarrier < 0);
}

static void testCheckpoints() {
    std::puts("custom: checkpoints");
    MatchSetup s = buildMatch(GameMode::Custom, customRules(Objective::Checkpoints, 2), nullptr, 0, 1, 4);
    for (int i = 0; i < s.count; ++i) s.loadout[i].bot = false;
    MatchState m = createMatch(s);
    park(m, s);
    int a = -1, b = -1;
    for (int i = 0; i < m.count; ++i) {
        if (m.f[i].team == 0 && a < 0) a = i;
        if (m.f[i].team == 1 && b < 0) b = i;
    }
    const Vec3 zone = mapOf(s).zones[0];
    m.f[a].pos = zone;
    m.f[b].pos = zone + Vec3{Fx::fromInt(1), Fx{}, Fx{}};
    for (int f = 0; f < 300; ++f) { m.f[a].pos = zone; m.f[b].pos = zone + Vec3{Fx::fromInt(1), Fx{}, Fx{}}; step(m, s, none20); }
    CHECK(m.objective.zones[0].owner < 0); // contested: nobody takes it
    m.f[b].pos = Vec3{Fx::fromInt(50), Fx{}, Fx::fromInt(50)};
    for (int f = 0; f < 300; ++f) { m.f[a].pos = zone; step(m, s, none20); }
    CHECK(m.objective.zones[0].owner == 0 && m.objective.score[0] > 0);
}

static void testFriendlyFireAndFfaDeathmatch() {
    std::puts("custom: friendly fire, ffa deathmatch with score limit");
    ModeRules r = customRules(Objective::Deathmatch, 2);
    r.friendlyFire = true;
    r.fighters = 4;
    MatchSetup s = buildMatch(GameMode::Custom, r, nullptr, 0, 1, 4);
    for (int i = 0; i < s.count; ++i) s.loadout[i].bot = false;
    MatchState m = createMatch(s);
    int a = 0, ally = -1;
    for (int i = 1; i < m.count; ++i) if (m.f[i].team == m.f[a].team) ally = i;
    // Ally stands right between P1 and the enemy P1 is locked on to
    const Vec3 toTarget = normalizeXZ(m.f[m.f[a].target].pos - m.f[a].pos, m.f[a].facing);
    m.f[ally].pos = m.f[a].pos + toTarget * Fx::ratio(14, 10);
    const int16_t hp = m.f[ally].health;
    FrameInput in[kMaxFighters] = {};
    in[a].buttons = BtnHeavy; // Heavy, because a Boundless ally would auto-block a Light
    step(m, s, in);
    for (int f = 0; f < 20; ++f) step(m, s, none20);
    CHECK(m.f[ally].health < hp);

    ModeRules ffa = customRules(Objective::Deathmatch, 0);
    ffa.fighters = 6;
    ffa.scoreLimit = 2;
    MatchSetup s2 = buildMatch(GameMode::Custom, ffa, nullptr, 0, 1, 4);
    for (int i = 0; i < s2.count; ++i) s2.loadout[i].bot = false;
    MatchState m2 = createMatch(s2);
    for (int k = 1; k <= 2; ++k) { m2.f[k].health = 0; m2.f[k].lastHitBy = 0; step(m2, s2, none20); }
    CHECK(m2.f[0].kos == 2 && m2.winner == 0);
}

static void testBotsPlayObjective() {
    std::puts("bots: 20 bots playing capture the flag score");
    ModeRules rules = customRules(Objective::CaptureTheFlag, 2);
    rules.map = &maps::harborDistrict();
    const MatchSetup s = buildMatch(GameMode::Custom, rules, nullptr, 0, 2, 21);
    MatchState m = createMatch(s);
    bool taken = false;
    EventSink ev;
    for (int f = 0; f < 3 * 60 * 60 && !taken; ++f) {
        ev.clear();
        step(m, s, none20, &ev);
        taken = saw(ev, ImpactKind::FlagTaken);
    }
    CHECK(taken);
}

// ---- techniques from the second batch

struct Duel {
    MatchSetup s;
    MatchState m;
    EventSink ev;
    Duel(const TechniqueDef* tech, const BodyDef& body = bodies::caster(), const TechniqueDef* other = &calamityFlames()) {
        s = makeSetup(body, tech, bodies::heavy(), other);
        m = closeRange(s);
        m.f[0].realmMeter = kRealmMeterMax;
    }
    void cast(uint16_t buttons, int frames = 90) {
        m.f[0].state = FState::Idle;
        m.f[0].moveId = -1;
        m.f[0].cooldown = {0, 0, 0};
        m.f[0].burnout = 0;
        if (s.stats[0].ceMax > 0) m.f[0].ce = s.stats[0].ceMax;
        run(m, s, frames, press(buttons), &ev);
    }
    int16_t hpLost() const { return static_cast<int16_t>(s.stats[1].maxHealth - m.f[1].health); }
};

static void testEveryTechniqueDoesDamage() {
    std::puts("every technique: skills and max output connect, no crashes");
    const TechniqueDef* all[] = {&graniteCannon(), &skyWeave(),    &thunderVessel(), &starMass(),    &blossomDraw(),
                                 &spiritHoard(),   &jackpot(),     &strawEffigy(),   &puppetPilot(), &lightMalice(),
                                 &boundless(),     &calamityFlames(), &shadowPuppets()};
    for (const TechniqueDef* t : all) {
        Duel d(t);
        // let film / frames build up, put the opponent right in front
        run(d.m, d.s, 400, [](int, FrameInput&, FrameInput&) {});
        d.m.f[1].pos = d.m.f[0].pos + d.m.f[0].facing * Fx::ratio(16, 10);
        for (uint16_t b : {static_cast<uint16_t>(BtnMod | BtnLight), static_cast<uint16_t>(BtnMod | BtnHeavy),
                           static_cast<uint16_t>(BtnMod | BtnGrab), static_cast<uint16_t>(BtnUlt)}) {
            d.m.f[1].health = d.s.stats[1].maxHealth;
            d.m.f[1].pos = d.m.f[0].pos + d.m.f[0].facing * Fx::ratio(16, 10);
            d.cast(b, 140);
        }
        int total = 0;
        for (const auto& e : d.ev) total += e.kind == ImpactKind::Hit || e.kind == ImpactKind::GuaranteedHit;
        if (total == 0) std::printf("  %s never hit anything\n", t->name);
        CHECK(total > 0);
    }
}

static void testGraniteStacks() {
    std::puts("granite cannon: hits charge the beam");
    Duel cold(&graniteCannon()), hot(&graniteCannon());
    cold.m.f[1].pos = cold.m.f[0].pos + Vec3{Fx{}, Fx{}, Fx::fromInt(6)};
    hot.m.f[1].pos = cold.m.f[1].pos;
    hot.m.f[0].tech[0] = 5;
    cold.cast(BtnMod | BtnLight, 40);
    hot.cast(BtnMod | BtnLight, 40);
    CHECK(cold.hpLost() > 0 && hot.hpLost() > cold.hpLost() * 2);
}

static void testSkyFilm() {
    std::puts("sky weave: needs film, a hit tears it");
    Duel d(&skyWeave());
    d.cast(BtnMod | BtnLight, 4);
    CHECK(d.m.f[0].state != FState::Attack); // no film yet
    run(d.m, d.s, 250, [](int, FrameInput&, FrameInput&) {});
    CHECK(d.m.f[0].tech[0] >= 2);
    run(d.m, d.s, 12, press(0, BtnLight));
    CHECK(d.m.f[0].tech[0] == 0);
}

static void testAmberBurnout() {
    std::puts("thunder vessel: amber form ends in burnout");
    Duel d(&thunderVessel());
    d.cast(BtnUlt, 40);
    CHECK(d.m.f[0].tech[1] > 0);
    run(d.m, d.s, 480, [](int, FrameInput&, FrameInput&) {});
    CHECK(d.m.f[0].burnout > 0 && d.m.f[0].tech[4] > 0); // burned out and exposed
}

static void testArcAndBlossomCounters() {
    std::puts("counters: arc counter and falling petal punish a jab");
    for (const TechniqueDef* t : {&thunderVessel(), &blossomDraw()}) {
        Duel d(t);
        const uint16_t stance = t == &thunderVessel() ? static_cast<uint16_t>(BtnMod | BtnGrab) : static_cast<uint16_t>(BtnMod | BtnHeavy);
        run(d.m, d.s, 25, [&](int f, FrameInput& p0, FrameInput& p1) {
            if (f == 0) p0.buttons = stance;
            if (f == 3) p1.buttons = BtnLight;
        }, &d.ev);
        CHECK(d.m.f[0].health == d.s.stats[0].maxHealth && d.hpLost() > 0);
    }
    // two absolute counters must not bounce forever
    MatchSetup s = makeSetup(bodies::caster(), &blossomDraw(), bodies::caster(), &blossomDraw());
    MatchState m = closeRange(s);
    m.f[0].tech[1] = m.f[1].tech[1] = 600;
    run(m, s, 12, press(BtnLight, BtnLight));
    CHECK(true);
}

static void testToolArsenal() {
    std::puts("tool arsenal: no CE needed, spear shuts techniques down, kill window");
    MatchSetup s = makeSetup(bodies::heavensBind(), &toolArsenal(), bodies::caster(), &boundless());
    MatchState m = closeRange(s);
    CHECK(m.f[0].ce == 0);
    m.f[1].realm.timer = 300;
    m.f[1].realm.center = m.f[1].pos;
    run(m, s, 30, press(BtnMod | BtnGrab));
    CHECK(m.f[1].health < s.stats[1].maxHealth && m.f[1].burnout > 0 && !m.f[1].realm.active());
    m = closeRange(s);
    m.f[0].realmMeter = kRealmMeterMax;
    run(m, s, kRealmCastFrames + 3, press(BtnMod | BtnUlt));
    CHECK(m.f[1].state == FState::Locked && m.f[0].tech[1] > 0 && !m.f[0].realm.active());
    // still immune to guaranteed hits
    CHECK(s.stats[0].guaranteedHitImmune);
}

static void testStrawMarks() {
    std::puts("straw effigy: nail marks, pin burst detonates from range");
    Duel d(&strawEffigy());
    d.m.f[1].pos = d.m.f[0].pos + Vec3{Fx{}, Fx{}, Fx::fromInt(5)};
    d.cast(BtnMod | BtnLight, 40);
    CHECK(d.m.f[0].tech[0] == (1 << 1));
    const int16_t afterNail = d.m.f[1].health;
    d.m.f[1].pos = d.m.f[0].pos + Vec3{Fx{}, Fx{}, Fx::fromInt(15)};
    d.cast(BtnMod | BtnHeavy, 30);
    CHECK(d.m.f[1].health < afterNail && d.m.f[0].tech[0] == 0);
}

static void testJackpot() {
    std::puts("jackpot: full meter = unlimited CE and healing");
    Duel d(&jackpot());
    d.m.f[0].tech[0] = 95;
    d.m.f[0].health = 300;
    run(d.m, d.s, 10, press(BtnLight), &d.ev); // the jab tips it over
    CHECK(d.m.f[0].tech[1] > 0);
    d.m.f[0].ce = 0;
    run(d.m, d.s, 60, [](int, FrameInput&, FrameInput&) {});
    CHECK(d.m.f[0].ce == d.s.stats[0].ceMax && d.m.f[0].health > 300);
}

static void testPuppetFrame() {
    std::puts("puppet pilot: frame deploys, losing it locks the technique");
    Duel d(&puppetPilot());
    run(d.m, d.s, 2, [](int, FrameInput&, FrameInput&) {});
    TechContext ctx{d.m, d.s, 0, nullptr};
    const int slot = ctx.companionSlot(0);
    CHECK(slot >= 0);
    d.m.f[0].lostMask = static_cast<uint8_t>(1u << d.s.techRosterStart[0]);
    d.m.f[0].comp[slot] = CompanionState{};
    d.cast(BtnMod | BtnLight, 4);
    CHECK(d.m.f[0].state != FState::Attack);
}

static void testSpiritAbsorb() {
    std::puts("spirit hoard: absorb eats a projectile and revives a spirit");
    MatchSetup s = makeSetup(bodies::caster(), &spiritHoard(), bodies::caster(), &calamityFlames());
    MatchState m = createMatch(s);
    m.f[0].lostMask = static_cast<uint8_t>(1u << s.techRosterStart[0]);
    m.f[1].ce = s.stats[1].ceMax;
    run(m, s, 80, [](int f, FrameInput& p0, FrameInput& p1) {
        if (f == 0) p1.buttons = BtnMod | BtnLight; // Ember Moths at P1
        if (f == 22) p0.buttons = BtnMod | BtnGrab; // absorb just before it lands
    });
    CHECK(m.f[0].lostMask == 0 && m.f[0].health == s.stats[0].maxHealth);
}

static void testLightRecoil() {
    std::puts("light & malice: every skill costs your own health");
    Duel d(&lightMalice());
    d.cast(BtnMod | BtnLight, 30);
    CHECK(d.m.f[0].health < d.s.stats[0].maxHealth);
    d.m.f[0].health = 5;
    d.cast(BtnUlt, 80);
    CHECK(d.m.f[0].health == 1 && !d.m.f[0].out);
}

// ---- cursed objects, outfits, mods

static void testCursedObjects() {
    std::puts("cursed objects: only count where the mode allows them");
    const CursedObjectDef* finger = cursed::find("withered_finger");
    const CursedObjectDef* cube = cursed::find("sealed_cube");
    CHECK(finger && cube && cursed::builtins().size() >= 15);

    // duel: slots are off, nothing happens
    MatchSetup duel = makeSetup(bodies::brawler(), &calamityFlames(), bodies::brawler(), &calamityFlames());
    duel.loadout[0].cursed[0] = finger;
    duel.finalize();
    CHECK(duel.items[0][Effect::DamagePct] == 0);

    // custom with three slots: finger + cube are active, a fourth item is ignored
    ModeRules r = rulesFor(GameMode::Custom);
    r.fighters = 2;
    r.teamCount = 0;
    r.cursedObjectSlots = 2;
    Loadout humans[2] = {{&bodies::brawler(), &calamityFlames(), {}, {}, nullptr, 0}, {&bodies::brawler(), &calamityFlames(), {}, {}, nullptr, 0}};
    humans[0].cursed = {finger, cube, cursed::find("lucky_ball")};
    MatchSetup s = buildMatch(GameMode::Custom, r, humans, 2, 1, 1);
    CHECK(s.items[0][Effect::DamagePct] == 15 && s.items[0][Effect::StartRealmMeterPct] == 50 && s.items[0][Effect::CritChancePct] == 0);
    MatchState m = createMatch(s);
    CHECK(m.f[0].realmMeter == kRealmMeterMax / 2 && m.f[1].realmMeter == 0);
    CHECK(s.stats[0].damagePct > s.stats[1].damagePct);
}

static void testOutfits() {
    std::puts("outfits: both cuts on both bodies, unlocks");
    for (const OutfitDef* o : outfits::builtins())
        CHECK(std::string_view(o->meshMasculine).size() > 0 && std::string_view(o->meshFeminine).size() > 0);
    PlayerRecord rec;
    const OutfitDef* kesa = Registry::instance().outfit("five_color_kesa");
    const OutfitDef* jacket = Registry::instance().outfit("academy_jacket");
    CHECK(kesa && jacket && isUnlocked(*jacket, rec) && !isUnlocked(*kesa, rec));
    rec.matchesWithTechnique["spirit_hoard"] = 10;
    CHECK(isUnlocked(*kesa, rec));
    // any cut on any body
    Appearance look;
    look.body = BodyType::Male;
    look.pieces[static_cast<size_t>(OutfitSlot::Bottom)] = {Registry::instance().outfit("academy_skirt"), Cut::Feminine};
    look.body = BodyType::Female;
    look.pieces[static_cast<size_t>(OutfitSlot::Top)] = {jacket, Cut::Masculine};
    CHECK(look.pieces[static_cast<size_t>(OutfitSlot::Top)].item == jacket);
}

static std::string readFile(const char* path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void testMods() {
    std::puts("mods: load the example mod, fight with its technique, reject broken mods");
    Registry& reg = Registry::instance();
    reg.unloadMods();
    const uint64_t vanilla = reg.contentHash();
    const std::string text = readFile(FC_EXAMPLE_MOD);
    CHECK(!text.empty());
    std::string err;
    CHECK(reg.loadMod(text, &err));
    if (!err.empty()) std::printf("  %s\n", err.c_str());
    CHECK(reg.contentHash() != vanilla && reg.mods().size() == 1);
    const TechniqueDef* frost = reg.technique("frost_bloom");
    const MapDef* shrine = reg.map("frozen_shrine");
    CHECK(frost && shrine && reg.cursedObject("frozen_heart") && reg.outfit("fur_collar_coat"));
    CHECK(shrine && shrine->obstacles.size() == 4 && shrine->radius == Fx::fromInt(60));

    // loading the same mod twice is refused
    CHECK(!reg.loadMod(text, &err));

    // the modded technique works in a real fight: beam, summon, counter, max output
    if (frost) {
        Duel d(frost);
        d.m.f[1].pos = d.m.f[0].pos + Vec3{Fx{}, Fx{}, Fx::fromInt(5)};
        d.cast(BtnMod | BtnLight, 40);
        CHECK(d.hpLost() > 0);
        d.cast(BtnMod | BtnHeavy, 20);
        TechContext ctx{d.m, d.s, 0, nullptr};
        CHECK(ctx.companionSlot(0) >= 0);
        d.m.f[1].pos = d.m.f[0].pos + d.m.f[0].facing * Fx::ratio(14, 10);
        const int16_t before = d.m.f[1].health;
        run(d.m, d.s, 25, [](int f, FrameInput& p0, FrameInput& p1) {
            if (f == 0) p0.buttons = BtnMod | BtnGrab;
            if (f == 3) p1.buttons = BtnLight;
        });
        CHECK(d.m.f[1].health < before);
        d.cast(BtnUlt, 90);
        CHECK(d.m.f[0].maxOutputUsed);
    }

    // a modded map runs and keeps everyone inside
    if (shrine) {
        ModeRules r = rulesFor(GameMode::Brawl);
        r.map = shrine;
        r.fighters = 8;
        const MatchSetup s = buildMatch(GameMode::Brawl, r, nullptr, 0, 1, 3);
        MatchState m = createMatch(s);
        for (int f = 0; f < 1200; ++f) step(m, s, none20);
        for (int i = 0; i < m.count; ++i) CHECK(length(Vec3{m.f[i].pos.x, Fx{}, m.f[i].pos.z}) <= shrine->radius);
    }

    // broken mods are rejected without leaving anything behind
    const size_t techniques = reg.techniques().size();
    CHECK(!reg.loadMod(R"({"id":"broken","techniques":[{"id":"x","skills":[]}]})", &err));
    CHECK(!reg.loadMod(R"({"id":"broken2","cursedObjects":[{"id":"y","effects":{"flying":10}}]})", &err));
    CHECK(!reg.loadMod(R"({"id":"broken3", )", &err));
    CHECK(!reg.loadMod(R"({"id":"broken4","techniques":[{"id":"boundless","skills":[{},{},{}],"maxOutput":{}}]})", &err));
    CHECK(reg.techniques().size() == techniques);
    reg.unloadMods();
    CHECK(!reg.technique("frost_bloom") && reg.contentHash() == vanilla);
}

static void testJsonNumbers() {
    std::puts("json: exact fixed point numbers");
    json::Value v;
    CHECK(json::parse(R"({"a": 1.5, "b": -0.25, "c": 12, "d": "x", "e": [1, 2]})", v));
    CHECK(v.fxOr("a", {}) == Fx::ratio(3, 2) && v.fxOr("b", {}) == Fx::ratio(-1, 4) && v.intOr("c", 0) == 12);
    CHECK(v.stringOr("d", "") == "x" && v.get("e")->items.size() == 2);
    CHECK(!json::parse("{\"a\": 1e5}", v));
}

// ---- peer to peer

struct P2PMatch {
    FakeNetwork net;
    std::vector<std::unique_ptr<FakeTransport>> transports;
    std::vector<net::LobbyConfig> configs;
    std::vector<MatchSetup> setups;
    std::vector<std::unique_ptr<net::PeerSession>> peers;
    std::vector<std::vector<FrameInput>> log; // per slot, per frame
    std::vector<bool> active;
    std::vector<uint8_t> localPeers;
    uint8_t round = 0;
    int delay = 2;

    P2PMatch(int peerCount, int playersPerPeer, GameMode mode, const ModeRules& rules, int latency, int jitter, int loss) {
        net.latency = latency;
        net.jitter = jitter;
        net.lossPct = loss;
        net.cut.assign(static_cast<size_t>(peerCount), false);
        for (int p = 0; p < peerCount; ++p) transports.push_back(std::make_unique<FakeTransport>(net, static_cast<uint8_t>(p)));
        auto requests = [&](int p) {
            std::vector<net::PlayerRequest> r;
            for (int k = 0; k < playersPerPeer; ++k) {
                net::PlayerRequest req;
                req.body = k % 2 ? "Caster" : "Brawler";
                req.technique = (p + k) % 2 ? "boundless" : "calamity_flames";
                req.team = static_cast<uint8_t>(p % 2);
                r.push_back(req);
            }
            return r;
        };
        // handshake over the same lossy network
        net::LobbyHost host(*transports[0], requests(0));
        std::vector<std::unique_ptr<net::LobbyClient>> clients;
        for (int p = 1; p < peerCount; ++p) clients.push_back(std::make_unique<net::LobbyClient>(*transports[static_cast<size_t>(p)], requests(p)));
        net::LobbyConfig hostConfig;
        bool started = false;
        for (int t = 0; t < 600; ++t) {
            bool all = true;
            for (auto& c : clients) all &= c->poll();
            host.poll();
            if (!started && host.peerCount() == peerCount) {
                CHECK(host.playerCount() == peerCount * playersPerPeer);
                hostConfig = host.start(mode, rules, 1, 99, static_cast<uint8_t>(delay));
                started = true;
            }
            ++net.now;
            if (started && all) break;
        }
        configs.push_back(hostConfig);
        for (auto& c : clients) configs.push_back(c->config());
        for (int p = 0; p < peerCount; ++p) {
            MatchSetup s;
            std::string err;
            const bool built = net::buildSetup(configs[static_cast<size_t>(p)], s, &err);
            if (!built) {
                // nothing below can run on a broken setup
                std::printf("  FAIL peer %d never got a usable lobby: %s\n", p, err.c_str());
                std::exit(1);
            }
            setups.push_back(s);
        }
        for (int p = 0; p < peerCount; ++p) {
            localPeers.push_back(p == 0 ? 0 : clients[static_cast<size_t>(p - 1)]->localPeer());
            peers.push_back(std::make_unique<net::PeerSession>(configs[static_cast<size_t>(p)], setups[static_cast<size_t>(p)],
                                                                localPeers.back(), *transports[static_cast<size_t>(p)]));
        }
        log.assign(static_cast<size_t>(setups[0].count), std::vector<FrameInput>(static_cast<size_t>(delay)));
        active.assign(static_cast<size_t>(peerCount), true);
        net.now = 0;
        net.inFlight.clear();
    }

    // One peer moves on to the next round. Peers still in the old one stop counting for the log.
    void rematch(size_t p, uint8_t newRound) {
        if (newRound != round) {
            round = newRound;
            log.assign(log.size(), std::vector<FrameInput>(static_cast<size_t>(delay)));
        }
        peers[p] = std::make_unique<net::PeerSession>(configs[p], setups[p], localPeers[p], *transports[p], newRound);
    }

    void tick(bool randomInputs, uint32_t salt) {
        for (size_t p = 0; p < peers.size(); ++p) {
            if (!active[p]) continue;
            net::PeerSession& ps = *peers[p];
            const int32_t before = ps.frame();
            std::vector<FrameInput> chosen;
            for (int k = 0; k < ps.localPlayerCount(); ++k) {
                uint32_t seed = static_cast<uint32_t>(before) * 2654435761u ^ (static_cast<uint32_t>(p * 31 + k) + salt);
                FrameInput in = randomInputs ? randomInput(seed) : FrameInput{};
                ps.setLocalInput(k, in);
                chosen.push_back(in);
            }
            ps.tick();
            if (ps.frame() > before && ps.round() == round)
                for (int k = 0; k < ps.localPlayerCount(); ++k) {
                    auto& slotLog = log[static_cast<size_t>(ps.localSlots()[static_cast<size_t>(k)])];
                    const size_t f = static_cast<size_t>(before + delay);
                    if (slotLog.size() <= f) slotLog.resize(f + 1);
                    slotLog[f] = chosen[static_cast<size_t>(k)];
                }
        }
        ++net.now;
    }

    // A frame every active peer has confirmed and still keeps in its history
    int32_t commonFrame() const {
        int32_t f = INT32_MAX, newest = 0;
        for (size_t p = 0; p < peers.size(); ++p) {
            if (!active[p]) continue;
            f = std::min(f, peers[p]->session().confirmedAll());
            newest = std::max(newest, peers[p]->frame());
        }
        return std::max(f, newest - Session::kHistory + 2);
    }

    bool agree(int32_t f) const {
        uint64_t h = 0;
        bool first = true;
        for (size_t p = 0; p < peers.size(); ++p) {
            if (!active[p]) continue;
            const MatchState* st = peers[p]->session().stateAt(f);
            if (!st) return false;
            if (first) { h = hashState(*st); first = false; }
            else if (hashState(*st) != h) return false;
        }
        return true;
    }

    bool matchesReference(int32_t f) const {
        MatchState ref = createMatch(setups[0]);
        std::vector<FrameInput> in(static_cast<size_t>(kMaxFighters));
        for (int32_t g = 0; g < f; ++g) {
            for (size_t slot = 0; slot < log.size(); ++slot)
                in[slot] = static_cast<size_t>(g) < log[slot].size() ? log[slot][static_cast<size_t>(g)] : FrameInput{};
            step(ref, setups[0], in.data());
        }
        return hashState(ref) == hashState(*peers[0]->session().stateAt(f));
    }
};

static void runAndCheck(P2PMatch& m, int ticks, const char* label) {
    for (int t = 0; t < ticks; ++t) m.tick(true, 7);
    for (int t = 0; t < 90; ++t) m.tick(false, 7); // let everything arrive
    const int32_t f = m.commonFrame();
    bool desync = false;
    for (auto& p : m.peers) desync |= p->desynced();
    const double seconds = static_cast<double>(m.net.now) / 60.0;
    std::printf("  %s: frame %d, rollbacks %d, %.1f KB/s per peer\n", label, m.peers[0]->frame(), m.peers[0]->session().rollbackCount(),
                static_cast<double>(m.net.bytes) / static_cast<double>(m.peers.size()) / seconds / 1024.0);
    CHECK(!desync);
    CHECK(f > ticks / 2);
    CHECK(m.agree(f));
    CHECK(m.matchesReference(f));
}

static void testP2PDuel() {
    std::puts("p2p 1v1: 4 frames lag, jitter, 10% loss");
    P2PMatch m(2, 1, GameMode::Duel, rulesFor(GameMode::Duel), 4, 3, 10);
    CHECK(m.setups[0].count == 2 && !m.setups[0].loadout[1].bot);
    CHECK(hashState(createMatch(m.setups[0])) == hashState(createMatch(m.setups[1])));
    runAndCheck(m, 900, "1v1");
}

static void testP2PTeamsSplitscreen() {
    std::puts("p2p 2v2: two consoles with two players each");
    P2PMatch m(2, 2, GameMode::Teams, rulesFor(GameMode::Teams), 6, 2, 5);
    int humans = 0;
    for (int i = 0; i < m.setups[0].count; ++i) humans += !m.setups[0].loadout[i].bot;
    CHECK(humans == 4);
    runAndCheck(m, 900, "2v2");
}

static void testP2PBrawl() {
    std::puts("p2p brawl: 6 peers + 14 bots, lag, jitter, 8% loss");
    P2PMatch m(6, 1, GameMode::Brawl, rulesFor(GameMode::Brawl), 5, 4, 8);
    CHECK(m.setups[0].count == kMaxFighters);
    runAndCheck(m, 600, "brawl");
}

static void testP2PDisconnect() {
    std::puts("p2p: a player drops, a bot takes over, the rest keep playing in sync");
    P2PMatch m(3, 1, GameMode::Brawl, rulesFor(GameMode::Brawl), 3, 2, 5);
    for (int t = 0; t < 300; ++t) m.tick(true, 3);
    m.net.cut[2] = true;
    m.active[2] = false;
    for (int t = 0; t < 600; ++t) m.tick(true, 3);
    for (int t = 0; t < 90; ++t) m.tick(false, 3);
    CHECK(m.peers[0]->peerDropped(2) && m.peers[1]->peerDropped(2));
    const int slot = m.peers[2]->localSlots()[0];
    CHECK(m.peers[0]->session().isBotControlled(slot, m.peers[0]->frame()));
    CHECK(!m.peers[0]->desynced() && !m.peers[1]->desynced());
    CHECK(m.agree(m.commonFrame()));
    CHECK(m.peers[0]->frame() > 700);
}

static void testP2PDesyncDetected() {
    std::puts("p2p: a peer with a different setup gets caught");
    P2PMatch m(2, 1, GameMode::Duel, rulesFor(GameMode::Duel), 2, 0, 0);
    // tamper with what peer 1 thinks the match is
    m.setups[1].loadout[0].body = &bodies::heavy();
    m.setups[1].finalize();
    m.peers[1] = std::make_unique<net::PeerSession>(m.configs[1], m.setups[1], 1, *m.transports[1]);
    for (int t = 0; t < 400; ++t) m.tick(true, 1);
    CHECK(m.peers[0]->desynced() || m.peers[1]->desynced());
}

static void testP2PTimeSync() {
    std::puts("p2p: a peer that starts late gets caught up");
    P2PMatch m(2, 1, GameMode::Duel, rulesFor(GameMode::Duel), 3, 1, 0);
    m.active[1] = false;
    for (int t = 0; t < 12; ++t) m.tick(true, 5);
    m.active[1] = true;
    for (int t = 0; t < 600; ++t) m.tick(true, 5);
    const int gap = std::abs(m.peers[0]->frame() - m.peers[1]->frame());
    CHECK(gap <= 4);
    CHECK(m.agree(m.commonFrame()));
}

static void testLobbyEncoding() {
    std::puts("p2p lobby: config round trip, version and mod checks");
    ModeRules r = rulesFor(GameMode::Custom);
    r.objective = Objective::CaptureTheFlag;
    r.teamCount = 4;
    r.cursedObjectSlots = 3;
    Loadout human{&bodies::summoner(), &shadowPuppets(), {}, {}, &companions::stoneWarden(), 2};
    human.cursed[0] = cursed::find("lucky_ball");
    const MatchSetup s = buildMatch(GameMode::Custom, r, &human, 1, 2, 77);
    std::vector<uint8_t> owners(1, 0);
    net::LobbyConfig cfg = net::describe(s, owners, 1, 77, 3);
    net::LobbyConfig back;
    CHECK(net::decode(net::encode(cfg), 0, back));
    MatchSetup rebuilt;
    CHECK(net::buildSetup(back, rebuilt));
    CHECK(hashState(createMatch(rebuilt)) == hashState(createMatch(s)));
    CHECK(rebuilt.items[0][Effect::CritChancePct] == 6 && rebuilt.loadout[0].assist == &companions::stoneWarden());
    back.contentHash ^= 1;
    std::string err;
    CHECK(!net::buildSetup(back, rebuilt, &err) && !err.empty());
    back.contentHash ^= 1;
    back.protocol = 999;
    CHECK(!net::buildSetup(back, rebuilt, &err));
}

static void testP2PRematch() {
    std::puts("p2p: rematch in the same lobby, peers restart a few ticks apart");
    P2PMatch m(2, 1, GameMode::Duel, rulesFor(GameMode::Duel), 3, 2, 5);
    for (int t = 0; t < 200; ++t) m.tick(true, 11);
    m.rematch(0, 1);
    for (int t = 0; t < 9; ++t) m.tick(true, 11); // peer 1 still sends round 0 packets, peer 0 must ignore them
    m.rematch(1, 1);
    for (int t = 0; t < 400; ++t) m.tick(true, 12);
    for (int t = 0; t < 90; ++t) m.tick(false, 12);
    const int32_t f = m.commonFrame();
    CHECK(f > 300);
    CHECK(m.agree(f));
    CHECK(m.matchesReference(f));
    CHECK(!m.peers[0]->desynced() && !m.peers[1]->desynced());
    const MatchState* settled = m.peers[0]->confirmedState();
    CHECK(settled && settled->frame <= m.peers[0]->frame() && settled->frame > 300);
}

int main() {
    testBodiesBalanced();
    testDeterminismAndRollback();
    testMovementRelativeToOpponent();
    testHitBlockParryGrab();
    testBlackSpark();
    testSuperArmor();
    testSkillsCooldownBurnout();
    testMaxOutput();
    testRealmLockAndHeavensBind();
    testRealmClash();
    testWardBlocksGuaranteedHit();
    testBoundlessField();
    testCalamityFlamesBurn();
    testBurstFromHitstun();
    testScenery();
    testOaths();
    testCompanionFuzz();
    testSummonerAssist();
    testAssistRoles();
    testLeash();
    testGuardShield();
    testCompanionDownAndRevive();
    testShadowMenagerie();
    testBondLinkAttack();
    testRealmHitsCompanions();
    testCompanionOaths();
    testRollbackSession();
    testTeamsNoFriendlyFire();
    testLockOn();
    testBotFightsBack();
    testBrawl20Bots();
    testSessionTeams();
    testCaptureTheFlag();
    testOddball();
    testCheckpoints();
    testFriendlyFireAndFfaDeathmatch();
    testBotsPlayObjective();
    testEveryTechniqueDoesDamage();
    testGraniteStacks();
    testSkyFilm();
    testAmberBurnout();
    testArcAndBlossomCounters();
    testToolArsenal();
    testStrawMarks();
    testJackpot();
    testPuppetFrame();
    testSpiritAbsorb();
    testLightRecoil();
    testCursedObjects();
    testOutfits();
    testJsonNumbers();
    testMods();
    testLobbyEncoding();
    testP2PDuel();
    testP2PTeamsSplitscreen();
    testP2PBrawl();
    testP2PDisconnect();
    testP2PDesyncDetected();
    testP2PTimeSync();
    testP2PRematch();
    if (g_failed) { std::printf("%d check(s) failed\n", g_failed); return EXIT_FAILURE; }
    std::puts("all tests passed");
    return EXIT_SUCCESS;
}
