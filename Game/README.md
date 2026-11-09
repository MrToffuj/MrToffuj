# Unreal-Projekt (FightGame)

Unreal-Engine-5-Projekt, das die deterministische Simulation aus [`/FightCore`](../FightCore) anzeigt
und spielbar macht. Aktuell mit **Platzhalter-Grafik** (Grundformen, Debug-Hitboxen), damit wir das
Kampfsystem testen können, bevor Modelle und Anime-Shader kommen.

## Einrichten

1. **Unreal Engine 5** über den Epic Games Launcher installieren (Projekt ist auf 5.7 gestellt; andere
   5.x: Rechtsklick auf `FightGame.uproject` → *Switch Unreal Engine version*).
2. **C++-Toolchain**:
   - Windows: Visual Studio 2022 mit „Game development with C++“ (oder JetBrains Rider)
   - Linux / Steam Deck: Clang-Toolchain laut Epic-Doku
3. Rechtsklick auf `FightGame.uproject` → **Generate Visual Studio project files**
4. `FightGame.sln` öffnen, Konfiguration **Development Editor**, bauen (F5 startet den Editor)
5. Im Editor **Play** drücken → Arena + 2 Kämpfer erscheinen

> Der Ordner `/FightCore` muss neben `/Game` liegen – das Modul kompiliert ihn direkt mit.

## Steuerung

| Aktion | Controller (positionsbasiert) | Tastatur P1 | Tastatur P2 |
|---|---|---|---|
| Bewegen | Linker Stick / D-Pad | WASD | Pfeiltasten |
| Light / Heavy / Grab | Links / Oben / Rechts (X/Y/B bzw. □/△/○) | J / K / L | Num1 / Num2 / Num3 |
| Jump | Unten (A / ✕) | Leertaste | Num0 |
| Guard | R1 / RB | Shift | Num4 |
| Mod | L1 / LB | Q | Num5 |
| Dash | R2 / RT | E | Num6 |
| Ult | L2 / LT | R | Num7 |
| Assist | R3 | F | Num8 |
| Ziel wechseln | L3 | Tab | Num9 |

Combos aus der GDD (Mod + Light = Skill 1, Guard + Grab = CE-Burst usw.) gelten genauso.

**Kamera:** sitzt hinter dem eigenen Kämpfer mit Lock-on auf den Gegner. Stick **hoch = auf den Gegner zu**,
runter = zurück, links/rechts = um den Gegner herum. Zurück + Heavy = Launcher.
Lokale Mitspieler (bis 4) spielen im Splitscreen, jeder sieht hinter seinem Kämpfer.

**Modi:** 1v1, 2v2, Brawl (offen, bis 20) und Custom (Teams, Ziel, Map frei wählbar). Freie Slots werden
mit Bots gefüllt. Einstellbar unter Projekt-Einstellungen → Fight Game → Match: Modus, Custom-Regeln,
Anzahl lokaler Spieler, Loadout (+ Team) pro Spieler, Bot-Stufe.

**Maps:** runde Kolonien mit schwarzer Barriere (Platzhalter-Optik), siehe [docs/MAPS.md](../docs/MAPS.md).
Kamera-Abstand, Höhe, Schulter-Versatz und FOV: Projekt-Einstellungen → Fight Game → Camera.

Belegung, Loadouts (Body / Technik / Assist pro Spieler), Stick-Deadzone und Debug-Anzeigen:
**Projekt-Einstellungen → Game → Fight Game**.

## Architektur

| Klasse | Aufgabe |
|---|---|
| `AFightGameMode` | legt den 2. lokalen Spieler an, spawnt den Director, keine Pawns |
| `AFightDirector` | fester 60-Hz-Tick über `fc::Session` (rollback-fähig), Input, Interpolation, Verfolger-Kamera pro Spieler, Events |
| `FFightInputSampler` | Controller/Tastatur → `fc::FrameInput`, kurze Taps gehen bei niedriger FPS nicht verloren |
| `AFightHUD` | Platzhalter-HUD pro Viewport (eigene Werte links, Gegner rechts), skaliert für Splitscreen/Handhelds |
| `AFightShapeVisual` | Platzhalter-Optik aus Engine-Grundformen |
| `UFightGameSettings` | Belegung, Loadouts, Debug |
| `Private/FightCore/FC_*.cpp` | binden die FightCore-Quellen in den Unreal-Build ein (nicht bearbeiten) |

**Regel:** Unreal liest den Simulations-Zustand nur und schreibt nie hinein. Alles Visuelle
(Zerstörung, Staub, Feuer, Impact-Frames) hängt an `AFightDirector::OnImpact`.

## Nächste Schritte

- Charakter-Modelle + Cel-Shading-Material + Outlines (siehe `docs/ART_STYLE.md`)
- `OnImpact` → Niagara-Staub, Chaos-Zerstörung der Wände, Krater-Decals
- Stepped Animation (Posen auf 2s/3s) über die Sim-Frames
- UMG-HUD im Anime-Stil, Button-Glyphen je Gerät
- Online P2P: siehe [docs/NETWORKING.md](../docs/NETWORKING.md) (Rollback + Bots fertig, Transport fehlt)
