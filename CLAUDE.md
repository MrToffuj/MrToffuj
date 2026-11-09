# Carnage Game – Notizen für Claude

Controller-first 3D-Arena-Fighter (Unreal 5.7), inspiriert vom Gameplay eines bekannten Shonen-Anime.
Namen im Spiel sind eigene Anspielungen, nie Original-Namen (z. B. CE, Realm Expansion, Black Spark,
Carnage Game). Spieltexte und Code auf Englisch, Code soll menschlich geschrieben wirken.

## Aufbau
- `FightCore/` – deterministische Simulation in C++20 (Fixed Point, Rollback, P2P-Netcode, Bots, Mods).
  Unabhängig von Unreal, eigene Tests.
- `Game/` – Unreal-Projekt `FightGame.uproject`. Bindet FightCore über `Source/FightGame/Private/FightCore/FC_*.cpp` ein.
  - Plugins: `PlayStationPads` (DualSense/DS4 nativ per HID), `AnimeLook` (Toon-Post-Pass, `r.AnimeLook`).
  - Menüs sind Slate in C++ (`FightMenu.*`), Online über EOS (`FightEos.*`, `FightOnline.*`).
- `docs/` – Design und Technik (deutsch).

## Dieser PC (Windows)
- Engine: `D:\Games\Tomb Raider\UE_5.7`
- Projekt: `C:\Users\IHasK\MrToffuj`
- Compiler: Build Tools 2026, MSVC 14.51 per `%APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml`
  (`CompilerVersion` = `Latest`). Die 14.44 ist unvollständig (kein delayimp.lib).

Build (PowerShell, Editor muss zu sein):
```
& "D:\Games\Tomb Raider\UE_5.7\Engine\Build\BatchFiles\Build.bat" FightGameEditor Win64 Development -project="C:\Users\IHasK\MrToffuj\Game\FightGame.uproject" -waitmutex
```
Editor starten:
```
& "D:\Games\Tomb Raider\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Users\IHasK\MrToffuj\Game\FightGame.uproject"
```
Editor-Log: `Game\Saved\Logs\FightGame.log`

FightCore-Tests: `cmake -S FightCore -B FightCore/build` und dann `FightCore/build/fightcore_tests`.

## Arbeitsweise
- Branch `claude/3d-fighting-game-tech-j8wut9`, nach getaner Arbeit committen und pushen.
- Nach Änderungen am Unreal-Code selbst bauen und Fehler fixen, bevor der Nutzer testet.
- MSVC ist strenger als GCC: kein `bool | int`, `UE_LOG` in if/else mit Klammern.
- Der Nutzer spricht Deutsch, mag kurze Antworten im Gen-Z-Ton.
