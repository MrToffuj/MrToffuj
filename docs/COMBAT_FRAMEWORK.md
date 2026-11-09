# Kampfsystem-Skelett (FightCore)

Grundlage, auf der **jeder** Charakter aufbaut. Code: [`FightCore/`](../FightCore).

## Prinzip

```
Build = Body (Stats + Passiv)  +  Technik (Template)  +  optional Pakt
```

Design-Inhalte stehen in [GDD_KAMPFSYSTEM.md](GDD_KAMPFSYSTEM.md). Dieses Doc beschreibt die Technik dahinter.

| Datei | Inhalt |
|---|---|
| `include/fightcore/Body.h`, `src/Body.cpp` | 6 Bodies + Nullgeboren, Stat-Ableitung aus 10 Punkten |
| `include/fightcore/Technique.h` | Technik-Template + `TechniqueLogic`-Hooks |
| `include/fightcore/Pact.h`, `src/Pact.cpp` | Pakt-Bewertung (versteckter Wert) |
| `src/Simulation.cpp` | Basis-Moves, Inputs, Ressourcen, Treffer, Realm, Clash |
| `techniques/*.cpp` | Eine Datei pro Technik (Vorlage: `Nullraum.cpp`) |

### Neue Technik bauen

1. `TechniqueDef` füllen: 3 Skills (`move`, `ceCost`, `cooldown`), `apexMove`, `apexType`, `realmClass`.
2. `TechniqueLogic` ableiten, nur die Hooks überschreiben, die gebraucht werden:
   `onMoveEvent`, `onTick`, `onRealmOpen/Tick`, `onHitLanded`, `nullifiesIncoming`, `onParry`,
   `damageDealtPct`, `damageTakenPct`.
3. Zustand **nur** in `FighterState::tech[8]` speichern → automatisch rollback-fähig.

### Move-IDs

`< 100` = Basis-Move (für alle gleich), `>= 100` = Technik-Move (`Index + 100`).

## Frame-Loop (60 Hz, fix)

1. Inputs in Buffer, Hitstop
2. Ressourcen: Essenz-Regen/-Verbrauch, Burnout, Cooldowns, Pakt-Timer
3. Zustandsautomat pro Kämpfer
4. Physik: Gravitation, Boden, **Wände (zerstörbar)**, Auseinanderschieben
5. `TechniqueLogic::onTick`
6. Treffer beider Seiten sammeln → dann anwenden (echte Trades). Reihenfolge: Core-Rule-Block →
   Block/Parry → Schadensmodifikatoren → Nullfunke → Super Armor → Hitstun
7. Projektile
8. Realm: Clash auflösen, Sure-Hit-Effekte, Timer
9. Siegbedingung

## Regeln für Determinismus (Rollback)

- Nur `fc::Fx` (Fixed-Point), **nie** float in der Simulation.
- `MatchState` bleibt trivially copyable → Save/Load = Kopie.
- Keine Zufallszahlen ohne Seed im State, keine Zeiger im State.
- Die Simulation sendet `ImpactEvent`s nach außen – alles Visuelle reagiert nur darauf.

## Tests

```bash
cd FightCore && cmake -S . -B build && cmake --build build && ./build/fightcore_tests
```

Getestet: Body-Balance, Determinismus + Rollback (4000 Frames Fuzz), Hit/Block/Parry/Grab, Nullfunke,
Super Armor, Skill-Kosten/Cooldown/Burnout, Apex, Realm (Lock, Nullgeboren-Immunität), Clash,
Schutzkreis, Distanzfeld, Hitze-Stacks, Essenz-Burst, Wandbruch, Pakte.
