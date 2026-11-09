# Modding

Mods können **Techniken, Kolonien (Maps), Cursed Objects und Outfits** hinzufügen.
Ein Mod ist ein Ordner unter `Game/Mods/<ModName>/` mit einer `mod.json`. Das Spiel lädt beim Start alle
Mods (alphabetisch) und zeigt sie neben den eingebauten Inhalten an.

Ein komplettes Beispiel mit allem liegt in [`Game/Mods/ExampleMod/mod.json`](../Game/Mods/ExampleMod/mod.json).

## Warum Daten statt Code

- **Online fair & synchron:** Alle Spieler rechnen dasselbe Match P2P. Zahlen aus Mods werden exakt als
  Festkomma gelesen, nie als Float. Die Lobby vergleicht den `contentHash` aller geladenen Mods, und nur Spieler
  mit identischen Mods können zusammen spielen.
- **Sicher:** Ein Mod kann keinen Code auf deinem Rechner ausführen.
- **Robust:** Alle Werte werden auf sinnvolle Bereiche begrenzt (z. B. Schaden max. 400). Ist ein Mod
  fehlerhaft, wird er komplett übersprungen und das Log sagt, warum.

## Aufbau

```jsonc
{
  "id": "my_mod",            // eindeutig
  "name": "My Mod",
  "version": "1.0",
  "author": "you",
  "techniques":    [ ... ],
  "maps":          [ ... ],
  "cursedObjects": [ ... ],
  "outfits":       [ ... ]
}
```
`//`-Kommentare sind erlaubt. Alle `id`s müssen eindeutig sein, auch gegenüber den eingebauten Inhalten.

## Techniken

Jede Technik folgt dem gleichen Template wie die eingebauten: **3 Skills + Max Output + Realm + Core Rule**.

```jsonc
{
  "id": "frost_bloom", "name": "Frost Bloom",
  "coreRule": "...", "weakness": "...",
  "core":  { "type": "stacksOnHit", "max": 5, "damagePerStack": 6 },
  "realm": { "name": "Winter Garden", "class": "debuff" },   // damage | lock | debuff | buffField
  "skills": [ skill, skill, skill ],                          // Offense, Utility, Defense/Movement
  "maxOutput": skill + { "type": "zone" }                     // burst | zone | transform
}
```

**Core Rules** (`core.type`):

| Typ | Wirkung | Felder |
|---|---|---|
| `none` | – | |
| `stacksOnHit` | Jeder Treffer gibt einen Stack, jeder Stack = mehr Schaden | `max`, `damagePerStack` (%) |
| `burnOnHit` | Technik-Treffer lassen das Ziel brennen | `damage` (pro 20 Frames), `frames` |
| `guardProjectiles` | Projektile werden automatisch geblockt, kostet CE | `ceCost` |

**Skill-Effekte** (`effect`), jeweils mit `name`, `ce`, `cooldown`, `startup`, `recovery` und Treffer-Werten
(`damage`, `hitstun`, `knockback`, `launch`, `impact`: light | medium | heavy | crushing, `unblockable`):

| Effekt | Was passiert | Extra-Felder |
|---|---|---|
| `melee` | Nahkampfschlag vor dir | `range`, `radius`, `active` |
| `projectile` | Geschoss nach vorne | `speed`, `radius`, `life` |
| `beam` | Strahl, trifft den ersten Gegner auf der Linie | `length`, `radius` |
| `zone` | Fläche unter dem Ziel, verzögert | `radius`, `delay` |
| `pull` | Ziel zu dir ziehen | `range` |
| `teleport` | Blink nach vorne (Stick zurück = nach hinten) | `range` |
| `buff` | Zeitlich begrenzter Buff | `frames`, `damagePct`, `speedPct`, `takenPct` |
| `counter` | Konter-Haltung, Nahkampftreffer werden gekontert | `frames` + Treffer-Werte des Konters |
| `summon` | Begleiter beschwören bzw. angreifen lassen | `companion`: `name`, `type` (striker / zoner / support / guard), `hp`, `upkeep`, `cooldown`, Treffer-Werte |
| `heal` | Heilen | `heal` |

Jeder Skill kann zusätzlich `selfDamage` haben (Rückstoß, lässt dich nie unter 1 HP fallen).

Zeiten in Frames (60 pro Sekunde), Distanzen in Metern, Dezimalzahlen erlaubt (`0.35`), keine Exponenten.

## Kolonien (Maps)

```jsonc
{
  "id": "frozen_shrine", "name": "Frozen Shrine",
  "radius": 60,                                 // Barriere-Radius in m (10–250)
  "obstacles": [ { "x": 0, "z": 0, "width": 12, "depth": 8, "height": 8, "hp": 1000 } ],  // max. 96
  "zones": [[0, 15], [30, -20], [-30, 20]]      // Checkpoint-Zonen, max. 3
}
```
Die schwarze Barriere kommt automatisch. Hindernisse sind zerstörbar.

## Cursed Objects

```jsonc
{ "id": "frozen_heart", "name": "Frozen Heart", "description": "...",
  "effects": { "damageTakenPct": -15, "speedPct": -10 } }
```

Mögliche Effekte (Werte sind Änderungen, `10` = +10 %):
`damagePct`, `damageTakenPct`, `normalDamagePct`, `skillDamagePct`, `companionDamagePct`, `speedPct`,
`ceRegenPct`, `ceMaxPct`, `maxHealthPct`, `cooldownPct`, `realmGainPct`, `startRealmMeterPct`,
`blackSparkWindow` (Frames), `parryWindow` (Frames), `lifestealPct`, `healOnKo` (HP),
`guaranteedHitTakenPct`, `overtimeDamagePct`, `critChancePct`, `thornsPct`.

## Outfits

```jsonc
{ "id": "fur_collar_coat", "name": "Fur Collar Coat", "slot": "top",   // head | face | top | bottom | shoes | accessory
  "meshMasculine": "/Game/Mods/MyMod/FurCoat_M",
  "meshFeminine":  "/Game/Mods/MyMod/FurCoat_F",
  "unlock": { "kind": "matchesWithTechnique", "technique": "frost_bloom", "amount": 5 } }
```
- Jedes Teil braucht **beide Schnitte**, und beide müssen auf beide Körper passen (gemeinsames Skelett,
  siehe unten).
- Freischalt-Arten: `default`, `wins`, `kos`, `matches`, `blackSparks`, `realmsOpened`, `matchesWithTechnique`.
- Meshes kommen als Unreal-Assets (Plugin/Pak) mit dem Mod. Fehlt ein Mesh, bleibt das Teil unsichtbar.

## Für Mesh-Ersteller

- Beide Grundkörper (`/Game/Characters/Body_Male`, `/Game/Characters/Body_Female`) teilen sich **ein Skelett**.
- Kleidungsteile werden auf dieses Skelett geriggt und folgen dem Körper (Leader Pose).
- Damit ein Schnitt auf beiden Körpern sitzt: Morph Targets für die Körperform (Brust/Hüfte/Schultern) im
  Kleidungsmesh, die das Spiel passend zum gewählten Körper setzt.

## Was (noch) nicht geht

- Eigene Logik per Skript. Geplant: eine Sandbox-Skriptsprache mit reiner Ganzzahl-Mathe, damit es deterministisch bleibt.
- Eigene Animationen und VFX für Mod-Techniken kommen über Unreal-Plugins (Asset-Mods); die Spiellogik bleibt im JSON.
