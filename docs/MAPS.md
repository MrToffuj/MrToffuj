# Carnage Game – Kolonien

Jede Map ist eine **Kolonie**: ein kreisrunder Bereich, komplett umschlossen von einer gigantischen
schwarzen Barriere. Drinnen ist alles zerstörbar, die Barriere selbst nie.

## Die Barriere

**Gameplay (fertig, `FightCore`):**
- Exakter Kreis um den Map-Mittelpunkt, unzerstörbar.
- Wer hineingeschleudert wird, prallt ab (halbe Geschwindigkeit), bekommt Wall-Splat-Stun dazu und
  löst ein `BarrierHit`-Event aus (Kamera-Shake, VFX).
- Projektile lösen sich an ihr auf.

**Look (Ziel):**
- Pechschwarzer, matter „Vorhang“ ohne sichtbare Lichtreflexe. Er ragt so hoch, dass man oben kein Ende sieht.
- Am Boden ein feiner, leicht flimmernder Rand, wo die Barriere aufsetzt.
- Von innen wirkt die Kolonie dadurch wie eine Insel: Der Himmel ist nur direkt über einem zu sehen.
- Klar erkennbar inspiriert, aber eigenes Design (keine 1:1-Kopie einzelner Anime-Einstellungen).

**Umsetzung in Unreal:**
- Aktuell Platzhalter: 72 schwarze Segmente im Kreis, 300 m hoch (`AFightDirector::SpawnArena`).
- Ziel: ein einzelner, innen gerenderter Zylinder-Mesh mit **Unlit**-Material (reines Schwarz +
  Fresnel-Rand am Boden + langsam wandernde Noise-Streifen). Kostet fast nichts → handheld-tauglich.
- Eingang/Ausgang der Kolonie (Lobby, Respawn) als „Durchtreten“-Effekt durch die Barriere.

## Kolonien

| Kolonie | Radius | Inhalt | Modi |
|---|---|---|---|
| **Shrine Ward** | 14 m | 4 Steinsäulen um die Mitte, 1 Zone | 1v1 |
| **Harbor District** | 20 m | 6 Lagerhallen/Kisten, 3 Zonen | 2v2, kleine Custom-Matches |
| **Old Capital** | 70 m | 16 Häuser in zwei Ringen, 3 Zonen | Brawl, Custom |

Gebäude haben HP. Wer mit genug Tempo hineingeschleudert wird, beschädigt sie. Bei 0 HP brechen sie
zusammen und man fliegt durch. Projektile beschädigen sie ebenfalls.

## Teams & Spawns

- Team-Basen liegen auf den vier Himmelsrichtungen (bis zu 4 Flaggen bei Capture the Flag).
- Teams spawnen an ihrer Basis, Free-for-all verteilt alle auf einem Ring durch die Kolonie.

## Nächste Schritte

- Echte Barriere-Shader + Bodenrand
- Gebäude als Chaos Geometry Collections (siehe [DESTRUCTION.md](DESTRUCTION.md))
- Mehr Kolonien mit eigenem Stil (Hafen bei Nacht, Tempelbezirk, zerstörte Innenstadt)
