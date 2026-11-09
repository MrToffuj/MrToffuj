# Kampfsystem-Skelett (FightCore)

Grundlage, auf der **jeder** Charakter aufbaut. Code: [`FightCore/`](../FightCore).

## Prinzip

```
Charakter = CharacterDef (Daten)  +  Technique (Logik, stateless)
```

- **CharacterDef**: Lebenspunkte, Laufgeschwindigkeit, Hurtboxen, Moves (Frame-Daten), Kommandos.
- **Technique**: Die Fluch-Technik-artige Spezialfähigkeit. Hat nur `const`-Methoden. Zustand liegt in
  `FighterState::tech[8]` → automatisch rollback-fähig, ohne dass der Charakter-Designer an Netcode denken muss.

Neuer Charakter = neue `.cpp` in `FightCore/characters/` (Vorlage: `Kael.cpp`).

## Ressource: Essenz

| | |
|---|---|
| Max | 1000 |
| Gewinn | Treffen, getroffen werden, (halb) blocken |
| Kosten | Techniken (z. B. 100–300), Ultimate/Realm = volle Leiste |

## Move-Kategorien & Cancels

`Normal → Special → Technique → Ultimate`. Jeder Move hat eine `cancelMask`, die festlegt, in welche
Kategorien er nach Treffer/Block gecancelt werden darf. Combos entstehen so rein datengetrieben.

## Steuerung (Controller-first, handheld-freundlich)

| Taste | Funktion |
|---|---|
| Light / Heavy | Normals |
| Special | Technik (Richtung + Special = andere Technik, **keine Viertelkreise nötig**) |
| Ultimate | Realm öffnen (volle Essenz) |
| Guard | Blocken (halten) |
| Jump / Dash | Bewegung (Dash in 4 Richtungen inkl. Sidestep) |

Richtungen sind relativ zum Gegner (Numpad): 6 vor, 4 zurück, 8/2 Sidestep.
Input-Buffer: 8 Frames, jeder Tastendruck wird nur einmal verbraucht.

## Realm (domain-artige Ultimates)

- Öffnet sich bei `eventFrame` eines Ultimate-Moves, hält 10 s.
- Technik bekommt `onRealmOpen` und kann über `realmIsMine()` in `onTick` / `damagePercent` Buffs geben.
- Geplant: **Realm-Clash** (beide öffnen gleichzeitig → Input-Duell), Realm-Gegenmaßnahmen pro Charakter.

## Frame-Loop (60 Hz, fix)

1. Inputs in Buffer
2. Hitstop (eingefrorene Kämpfer überspringen)
3. Zustandsautomat pro Kämpfer (Idle/Walk/Dash/Air/Attack/Hitstun/Blockstun/Knockdown/Guard)
4. Physik: Gravitation, Boden, **Wände (zerstörbar)**, Auseinanderschieben
5. `Technique::onTick`
6. Treffer beider Seiten sammeln → dann anwenden (echte Trades)
7. Projektile
8. Realm-Timer, Siegbedingung

## Regeln für Determinismus (Rollback)

- Nur `fc::Fx` (Fixed-Point), **nie** float in der Simulation.
- `MatchState` bleibt trivially copyable → Save/Load = Kopie.
- Keine Zufallszahlen ohne Seed im State, keine Zeiger im State.
- Die Simulation sendet `ImpactEvent`s nach außen – alles Visuelle reagiert nur darauf.

## Tests

```bash
cd FightCore && cmake -S . -B build && cmake --build build && ./build/fightcore_tests
```

Getestet: Determinismus (5000 Frames Random-Input), Rollback, Treffer, Block, Ultimate-Kosten,
Wandbruch, Technik (Schwerkraftknoten zieht Gegner).
