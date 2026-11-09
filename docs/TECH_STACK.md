# Tech-Stack-Entscheidung

## Entscheidung: **C++ mit Unreal Engine 5**

| Bereich | Wahl |
|---|---|
| Sprache (Kern) | **C++20** |
| Engine | **Unreal Engine 5** (Rendering, Animation, Plattform-Ports) |
| Kampf-Simulation | Eigenes, engine-unabhängiges C++-Modul (deterministisch, Fixed-Point) |
| Netcode | Rollback (GGPO-Prinzip), 60 Ticks/s fest |
| Input | Unreal *Enhanced Input* + eigener Input-Buffer |
| Scripting/Prototyping | Blueprints nur für UI, VFX-Trigger, Menüs – **nie** für Frame-Daten |

## Warum C++ / Unreal?

1. **Konsolen-Support offiziell**: PlayStation 5, Xbox Series, Nintendo Switch 2 – Unreal hat zertifizierte Plattform-SDKs. Godot bräuchte Drittanbieter-Ports, Rust/eigene Engine wären ein Riesenaufwand.
2. **Genre-Standard**: Viele moderne 3D-Fighter laufen auf Unreal; Know-how, Tutorials und Tools sind da.
3. **Performance auf Handhelds**: C++ gibt volle Kontrolle über Speicher und Frame-Budget – wichtig für Steam Deck und Switch 2 (Ziel: stabile 60 FPS, Gameplay-Logik fix 60 Hz).
4. **Determinismus**: Rollback-Netcode verlangt bit-genaue Simulation. In C++ können wir Fixed-Point-Math und eigenen State-Save/Load selbst bauen – mit GC-Sprachen (C#) deutlich fummeliger.

## Architektur-Grundregel

```
┌───────────────────────────────┐
│ Unreal (Präsentation)          │  Rendering, Kamera, VFX, Audio, UI
└──────────────▲────────────────┘
               │ liest nur State
┌──────────────┴────────────────┐
│ FightCore (reines C++)         │  Hitboxen, Frame-Daten, Techniken,
│ deterministisch, kein UObject  │  Energie-Ressource, Rollback-Snapshots
└───────────────────────────────┘
```

Die Kampflogik kennt Unreal nicht → testbar, rollback-fähig, portabel.

## Controller & Handheld

- Controller-first: Alle Menüs komplett mit Pad bedienbar, kein Maus-Zwang.
- Input-Buffer (~6–8 Frames), Motion-Inputs **und** vereinfachtes Steuerungsschema (Smart-Controls) für Handhelds.
- Glyphen dynamisch je nach Gerät (Xbox / PlayStation / Nintendo / Steam Deck).
- Rebinding pro Profil, Stick-Deadzones einstellbar.
- UI-Skalierung für 7–8"-Screens, Text min. ~18 px bei 800p.
- Grafik-Presets „Handheld“ (z. B. 720p–800p, dynamische Auflösung, reduzierte Schatten/Partikel).

## Lizenz & IP

- Unreal: 5 % Royalty ab 1 Mio. USD Bruttoumsatz pro Spiel.
- Konsolen-SDKs erfordern Entwickler-Registrierung bei Sony / Microsoft / Nintendo.
- Spiel ist nur **gameplay-inspiriert** von Fluch-Energie-Kampfsystemen (Techniken, Domänen-artige Ultimates). **Keine** Namen, Figuren, Orte oder Begriffe aus bestehenden Werken – alles eigenes IP.
