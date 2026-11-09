# GDD: Kampfsystem

> Basierend auf dem GDD von Joshua Jäger (09.11.2026). Gameplay ist von Fluch-Energie-Kampfsystemen
> inspiriert, **alle Namen sind eigenes IP**. ✅ = in `FightCore` implementiert und getestet.

## 1. Vision & Design-Pillars

Jeder Spieler baut seinen Char aus **Body + Technik** zusammen. Alle teilen dieselbe Steuerung und
dieselben Systeme. Der Spielstil entsteht nur aus der Kombi.

1. **Gleiche Basis, eigener Stil.** Identische Inputs und Systeme, Unterschiede nur über Body und Technik.
2. **Build statt Roster.** Body und Technik frei kombinierbar, Anti-Synergien sind Absicht.
3. **Jede Stärke hat ein Gegenmittel.** Schere-Stein-Papier auf Body-Ebene, Schwäche-Pflicht pro Technik.
4. **Entdecken statt Tutorial.** Binding Oaths sind ein verstecktes Layer.
5. **Timing schlägt Zahlen.** Black Spark, Parry und Realm-Clash belohnen Reads und Execution.

## Begriffe

Das Spiel läuft auf **Englisch**. Die Namen spielen bewusst auf das Vorbild an, sind aber eigene Begriffe.

| Vorbild | Bei uns |
|---|---|
| Cursed Energy | **CE** |
| Cursed Technique | **Technique (CT)** |
| Maximum Technique | **Max Output** |
| Domain Expansion | **Realm Expansion** |
| Simple Domain | **Simple Realm** |
| Black Flash | **Black Spark** |
| RCT | **Reverse Flow** |
| Binding Vow | **Binding Oath** |
| Heavenly Restriction | **Heaven's Bind** |
| Sure-Hit | **Guaranteed Hit** |
| Shikigami | **Familiars** (im Code: Companions) |
| Culling Game | **Carnage Game** (Kolonien = Maps, siehe [MAPS.md](MAPS.md)) |

## 2. Gemeinsame Basis ✅

### Steuerung (Controller)

| Input | Aktion |
|---|---|
| Stick | hoch = zum Gegner, runter = zurück, links/rechts = umkreisen (Kamera hinter dem Kämpfer) |
| Light / Heavy / Grab | Normals (zurück + Heavy = Launcher) |
| Guard (R1) | Blocken, frisch gedrückt kurz vor Treffer = **Parry** |
| Guard + Dash | **Dodge** (Unverwundbarkeit) |
| Mod (L1) + Light / Heavy / Grab | Skill 1 / 2 / 3 (Offense / Utility / Defense) |
| Ult (L2) | Max Output |
| Mod + Ult | Realm öffnen |
| Ult (während gegnerischem Realm, halten) | Simple Realm |
| Guard + Grab | **CE-Burst** – Notfall-Pushback, auch aus Hitstun |
| Mod + Guard (halten) | Reverse Flow |
| Mod + Dash | Verstärkung an/aus |
| Assist (R3) + Richtung | Begleiter rufen (siehe 9) |

Modifier zählen **im Frame des Drückens** (wichtig für den Input-Buffer).

### Ressourcen

| Ressource | Funktion | Lädt über |
|---|---|---|
| HP | Lebensleiste | Reverse Flow (max. 30 % pro Runde) |
| CE | Skills, Max Output, Burst, Verstärkung, Core Rules | Passive Regen, Body-abhängig |
| Realm-Meter | Voraussetzung für Realm | Schaden, Black Spark, Parry |
| Burnout | CE leer → Technik 3 s gesperrt | automatisch |

### Universal-Mechaniken

- **Verstärkung:** +20 % Schaden, −20 % Schaden durch CE-Angriffe, kostet CE/Frame.
- **Black Spark:** Normal genau auf das Hitstop-Ende des vorherigen Treffers gedrückt
  (±1 Frame, Brawler ±3). ×2,5 Schaden, +25 % Realm-Meter, extra Hitstop → Impact-Frame.
- **Reverse Flow:** Heilung, Char steht still und ist verwundbar.
- **Simple Realm:** blockt Guaranteed Hit, kostet CE pro Frame (Technician halb).
- **Realm:** bei 100 % Meter, siehe 5.

## 3. Body Types ✅

Jeder Body hat genau **10 Punkte** auf HP / Speed / CE / Range.

| Body | HP | Speed | CE | Range | Passiv |
|---|---|---|---|---|---|
| Heavy | 5 | 1 | 2 | 2 | Super Armor auf Heavies, Grabs ×1,5 |
| Speedster | 1 | 5 | 2 | 2 | Dash-Cancel nach Treffer, Crit (×1,3) von Seite/Rücken |
| Caster | 1 | 2 | 4 | 3 | Regen ×1,5, Cooldowns −25 % |
| Brawler | 3 | 3 | 2 | 2 | Black-Spark-Fenster ±3 |
| Technician | 2 | 3 | 3 | 2 | Parry-Fenster 8 statt 5, Simple Realm halb so teuer |
| Summoner | 2 | 2 | 3 | 3 | Assist-Slot: 1 permanenter Begleiter |

**Heaven's Bind (Sonder-Build):** Speedster ohne Technik und ohne CE. Dafür +20 % Speed,
+15 % Schaden, immun gegen Guaranteed Hit, begrenzter Binding Oath-Wert.

**Anti-Synergien sind gewollt:** z. B. Boundless auf Heavy = Feld-Tank mit CE-Hunger.

Abgeleitete Werte (`src/Body.cpp`): HP = 600 + 120 × HP, CE = 400 + 150 × CE,
Reichweite ×(0,85 + 0,075 × Range), Realm-Gewinn 80 % + 10 % × CE.

## 4. Technik-Template ✅

| Baustein | Anzahl | Regel |
|---|---|---|
| Core Rule | 1 | Passive Eigenheit, die alle Skills färbt |
| Skills | 3 | Offense, Utility, Defense/Movement – kosten CE, haben Cooldown |
| Max Output | 1 | Burst / Zone / Transform, kostet 50 % CE, 1× pro Runde, bestrafbares Startup |
| Realm | 1 | Klasse Damage / Lock / Debuff / BuffField |
| Schwäche | 1 | Bewusst eingebaut, nie optional |

**Design-Regel:** Fehlt Approach, Anti-Air, Defense oder Comeback → bei Skills nachbessern, nie bei Stats.

## 5. Realm ✅

Cast-Zeit (60 F) und Dauer (600 F) für alle gleich, nur der Effekt unterscheidet sich.

| Klasse | Effekt |
|---|---|
| Damage | Guaranteed Hit: 20 Schaden alle 0,5 s |
| Lock | Gegner alle 200 F für 90 F festgesetzt, kaum Schaden |
| Debuff | Gegner: keine CE-Regen, Skills +50 % teurer, +15 % Schaden genommen |
| BuffField | Besitzer: Cooldowns 2× schneller, Skills −50 % Kosten, +15 % Schaden |

- **Geschlossener Realm:** Besitzer bewegt sich mit halbem Tempo. **Offener Realm** (Oath-Unlock):
  volle Bewegung, Wirkung nur im Radius 10.
- **Konter:** Simple Realm, Gegen-Realm → **Clash** (90 F Light hämmern, mehr Drücke gewinnt, bei
  Gleichstand hält der Verteidiger).

## 6. Technik-Katalog

| Technik | Core Rule | Skills (Off / Util / Def) | Max Output | Realm | Schwäche | Status |
|---|---|---|---|---|---|---|
| **Boundless** | Infinite Gap: Auto-Block gegen Projektile & Lights, kostet CE | Repel / Attract / Fold Step | Singularity (Burst) | Endless Void (Lock) | Kein CE = kein Feld, Grabs gehen durch | ✅ |
| **Calamity Flames** | Hitze-Stacks: Gegner brennt | Ember Moths / Magma Pillar (Zone) / Heat Mantle | Falling Star (Burst) | Iron Coffin (Damage) | Setup, langes Startup | ✅ |
| **Shadow Puppets** | Shadow Pool: bis zu 2 Bestien als Begleiter | Shadow Hounds / Night Wing (Anti-Air) / Maw Toad (Grab-Pull) | Flood Colossus (Zone) | Shadow Garden (BuffField) | Gefallene Bestien weg für die Runde | ✅ |
| Butcher's Cut | Schnitt-Schaden skaliert mit HP-Verlust des Gegners | Fernschnitt / Nahschnitt / Schnittnetz | Flame Arrow (Burst) | Cruel Shrine (Damage) | Blockbar, wenig Defense | ⏳ |
| Pure Love | Copy Mark: geparrter Skill 20 s gespeichert | Spirit Strike / Spirit Guard / Mimic Swap | Bound Spirit Unleashed (Transform) | Hall of Mirrors (BuffField) | Ohne Kopie dünnes Kit | ⏳ |
| Soul Sculpting | Seelenberührung baut Debuff auf | Seelenhand / Leibverformung / Seelenpanzer | Seelenspaltung (Transform) | Perfect Form (Debuff) | Braucht Kontakt | ⏳ |
| Clap Swap | Klatschen tauscht Positionen (auch Objekte) | Tauschschlag / Köder-Tausch / Objekt-Tausch | Tauschwirbel (Zone) | Lock | Klatschen kostet Tempo, lesbar | ⏳ |
| Blood Craft | Blut-Meter, Skills teils mit HP bezahlt | Blutlanze / Roter Strom / Verdichtung | Blutnova (Burst) | BuffField (gratis Blut) | Glass Cannon | ⏳ |
| Frame Sorcery | 24-Bilder-Regel: wer deine Frames bricht, friert ein | Bildsprung / Bildfessel / Bildfolge | Zeitraffer (Transform) | Debuff | Lesbar, Regelbruch friert dich selbst ein | ⏳ |
| Spell Voice | Befehle kosten CE + Selbstschaden | „Halt!“ / „Zerbirst!“ / „Fort!“ | Massenbefehl (Burst) | Lock | Selbstschaden, wenig Defense | ⏳ |
| **Granite Cannon** | Output-Stacks: Treffer laden den Beam | Granite Beam / Shockwave (Anti-Air) / Charge Guard | Full Output (Burst) | Granite Furnace (Damage) | Charge lesbar, kaum Mobility | ✅ |
| **Sky Weave** | Sky-Film: Himmel wird zur faltbaren Fläche, Film-Ladungen bauen sich auf | Sky Drop / Sky Pull / Sky Wall | Sky Tear (Zone) | Folded Heaven (Debuff) | Film braucht Setup, reißt bei Treffern | ✅ |
| **Thunder Vessel** | Charge-Stacks, Bolt entlädt alles | Bolt (Beam) / Jolt Dash / Arc Counter | Amber Form (Transform, danach Burnout) | Storm Hall (BuffField) | Nach Amber schutzlos | ✅ |
| **Star Mass** | Masse-Stacks: Normals stapeln virtuelle Masse | Mass Slam / Weightless / Star Wing (Striker-Begleiter) | Black Hole (Burst, zieht an) | Gravity Well (Damage) | Viel Setup, CE-hungrig | ✅ |
| **Blossom Draw** | Counter-Zone: Guard kontert Nahkampf automatisch (kostet CE) | Draw Cut / Falling Petal (Iai-Counter) / Step In | Absolute Counter (Transform) | Petal Field (BuffField) | Reaktiv, ohne Read nutzlos | ✅ |
| **Spirit Hoard** | Spirit-Pool: bis zu 3 absorbierte Geister | Prism Serpent / Release / Absorb | Spiral Collapse (Burst, verbraucht Geister) | Spirit Nest (Debuff) | Pool leer = nackt, Geister killbar | ✅ |
| **Jackpot** | Jackpot-Meter über Treffer + Blocks → unendlich CE + Auto-Heilung | Ball Rush / Shutter Wall / Reel Dash | Fever Mode (Transform) | Lucky Express (BuffField, Walzen drehen) | Ohne Jackpot dünn | ✅ |
| **Straw Effigy** | Resonanz-Marken: Nägel markieren, später zünden | Nail / Pin Burst / Decoy Doll | Full Resonance (Burst) | Resonance Chamber (Debuff) | Fragil, Marken sichtbar | ✅ |
| **Puppet Pilot** | Remote-Body: Battle Frame kämpft, du bleibst hinten | Cannon / Stray Drone (Anti-Air) / Dash Blade | Overdrive Frame (Transform) | Puppet Theater (Lock) | Frame kaputt = Technik weg | ✅ |
| **Light & Malice** | Lichtgeschwindigkeit + manifestierte Tötungsabsicht | Light Beam / Light Step / Killing Intent | Reversal Beam (Burst) | Blinding Hall (Damage) | Jeder Skill kostet eigene HP (bleibt bei min. 1), sehr teuer | ✅ |

## 7. Binding Oaths ✅ (Grundsystem)

Verstecktes Layer: Spieler wählt **Kategorie + Größe**, nicht die Belohnung.

| Kategorie | Opfer (`detail`) |
|---|---|
| Körper | Max-HP, Speed, Block-Stärke |
| Ressource | CE-Pool, CE-Regen, Reverse Flow-Heilung |
| Technik | Skill 1/2/3 sperren, Realm verbieten, Max Output verbieten |
| Regel | „nie blocken“, „nur Nahkampf“ |
| Zeit | gilt nur 60 s, Wert ×1,5 |
| Begleiter | Begleiter-HP, Slot sperren, Begleiter dauerhaft verlieren |

**Versteckter Wert** = Größe (1/2/4) × Empfindlichkeit (0–10, abhängig von Body + Playstyle-Profil).
Ungenutzte Skills sind fast wertlos, Caster zahlt für Regen viel, Heavy fast nichts.
Heaven's Bind: Wert max. 12.

| Wert | Stufe | Folge | Aura |
|---|---|---|---|
| < 4 | Zu billig | Backlash: CE-Leck, Cooldowns +25 % | Flackern |
| 4–11 | Fair | +Schaden (max 15 %), +Realm-Gewinn | Ruhig |
| 12–23 | Hoch | + **Offener Realm** (Heaven's Bind: +1 Black Spark-Frame) | Knistern |
| ≥ 24 | Überzahlt | + kleiner Bonus (+5 %), nie das Doppelte | Knistern |

Regeln: 1 Binding Oath pro Match, Belohnung nie größer als das Opfer, Gegner sieht nur **dass** ein Binding Oath aktiv ist.
Playstyle-Profil wird vor dem Match zwischen Clients ausgetauscht → deterministisch.

## 8. Balance-Framework

| Zyklus | Matchup | Grund |
|---|---|---|
| 1 | Speedster > Caster | schließt die Range |
| 1 | Caster > Heavy | kitet den Tank |
| 1 | Heavy > Speedster | Armor frisst Hits, ein Grab entscheidet |
| 2 | Brawler > Summoner | Pressure, Assist kommt nicht zum Zug |
| 2 | Summoner > Technician | Multi-Threat überlastet Counter |
| 2 | Technician > Brawler | liest Pressure und kontert |

**Checkliste pro Build:** Approach · Anti-Air · Defense-Option · Pressure/Okizeme · Comeback · Schwäche.

**Gegen Power-Creep:** Mehr Power kostet immer Startup, Setup, Ressourcen oder HP.

## 9. Begleiter-System ✅

Begleiter kämpfen neben dem Spieler, sind aber **kein zweiter Char**, sondern ein Werkzeug:
ein Spieler, zwei Ziele, **ein CE-Pool**. Code: `include/fightcore/Companion.h`, `src/Companion.cpp`.

### Wer bekommt Begleiter

| Quelle | Begleiter | Regel |
|---|---|---|
| Summoner-Body | 1 permanenter Begleiter (frei wählbar) | Fällt aus → kehrt nach 15 s zurück, Revive kostet 150 CE (wartet, bis genug da ist) |
| Shadow Puppets | bis zu 2 Bestien aus dem Shadow Pool | Skill beschwört; 3. Bestie ersetzt die älteste; gefallene sind für die Runde weg (Skill gesperrt) |
| Andere Bodies | keiner | nur über zeitlich begrenzten Binding Oath |

Summoner + Shadow Puppets geht: Assist + 2 Bestien (3 Slots).

### Steuerung

- **Assist-Button + Richtung:** neutral = Angriff, vor = Druck, seitlich = Anti-Air, zurück = Schutz.
  (Mit der Kamera hinter dem Kämpfer sind „hoch“ und „vorwärts“ dieselbe Stick-Richtung, daher liegt Anti-Air auf seitlich.)
- Der passende Begleiter wird automatisch gewählt (Rolle → bevorzugter Typ).
- Begleiter handeln nur auf Befehl, sonst folgen sie dem Spieler. Kein Autopilot.
- Pro Begleiter ein Cooldown nach jeder Aktion.
- **Leash:** wird der Spieler getroffen, bricht jeder Begleiter seine Aktion ab.
- Kein Rufen während Burnout, Hitstun, Lock oder Clash.

### Ressourcen

- Begleiter-HP: eigene, kleine Leiste. Gegner können Begleiter direkt treffen (Melee + Projektile).
- **Upkeep:** jeder Begleiter auf dem Feld zieht CE pro Frame → weniger für Skills, Max Output, Realm.
- **Schadens-Cap:** Begleiter-Treffer machen 60 % eines vergleichbaren Skills (Engine-Regel).

### Begleiter-Typen & Aktions-Bausteine

| Typ | Aufgabe | Summoner-Beispiel | Shadow Pool |
|---|---|---|---|
| Striker | Offense, Druck, Combo-Verlängerung | Blade Wolf | Shadow Hounds |
| Zoner | Projektile, Anti-Air, Raumkontrolle | Spark Owl | Night Wing |
| Support | Grabs, Pulls, Utility | Vine Toad | Maw Toad |
| Guard | blockt für den Spieler (auch beim Reverse Flow) | Stone Warden | – |

Jede Aktion ist aus generischen Bausteinen gebaut: **Rush** (hinsprinten + Schlag), **Shoot** (Projektil),
**Rise** (Anti-Air vor dem Spieler), **Pull** (Gegner zum Spieler ziehen, unblockbar), **Shield**
(stellt sich davor, fängt Treffer von vorne ab, Grabs gehen durch).

### Bond-Level (nur fürs laufende Match)

| Stufe | ab | Effekt |
|---|---|---|
| 1 | 0 | – |
| 2 | 3 erfolgreiche Assists | **Link-Attack:** Assist im Hit-Confirm → Begleiter taucht am Gegner auf, schlägt sofort zu, +25 % |
| 3 | 6 erfolgreiche Assists | Begleiter wirkt im Realm mit: Damage-Tick 25 statt 20, Lock 120 statt 90 F, BuffField/Debuff +25 % statt +15 % |

Fällt ein Begleiter, ist der Bond-Fortschritt weg.

### Schwächen & Counterplay

- Gegner kann den Begleiter ignorieren und den Spieler angreifen (Leash).
- Begleiter sind **nicht realm-immun**: Guaranteed Hit-Schaden und Lock erfassen sie (auch wenn der Spieler im Simple Realm steht).
- Begleiter verlieren → weniger Druck, Utility und Bond.

### Oath-Anbindung

Opfer-Feld **Begleiter**: HP, Slot sperren, Begleiter dauerhaft verlieren. Ohne Begleiter wertlos (Backlash).

| Stufe | Hidden Reward |
|---|---|
| Hoch | **Oath Spirit**: temporärer Zusatz-Begleiter (30 s, kein Upkeep) |
| Überzahlt | **The Adaptor**: mächtige Sonderform mit **Adaption** – dieselbe Angriffs-Kategorie macht beim 2. Treffer nur noch halben Schaden |

Bodies ohne Begleiter: ein **zeitlich begrenzter Binding Oath** mit hohem Wert gibt einen Oath Spirit für die Dauer des Oaths.

## 10. Spielmodi ✅

| Modus | Spieler | Regeln | Kolonie |
|---|---|---|---|
| **1v1** | 2 | Elimination | Shrine Ward (Radius 30 m) |
| **2v2** | 4 | Team-Elimination, kein Friendly Fire | Harbor District (45 m) |
| **Brawl** | bis 20 | Offene Kolonie: **kein Zeitlimit, keine Leben**, Respawn nach 5 s, endet nie | Old Capital (200 m) |
| **Custom** | bis 20 | Eigene Teams (0 = jeder gegen jeden, bis 20 Teams), Ziel frei wählbar, Friendly Fire an/aus, Punkte-/Zeitlimit, Respawn-Zeit | jede Kolonie |

**Custom-Ziele:**

| Ziel | Regel |
|---|---|
| Elimination | Letztes Team steht, kein Respawn |
| Deathmatch | Jeder K.O. = 1 Punkt fürs Team (FFA: eigene K.O.s) |
| Capture the Flag | Gegnerische Flagge zur eigenen Basis bringen, während die eigene zu Hause ist. Fallengelassene Flaggen kehren nach 15 s zurück oder per Berührung durchs eigene Team. Bis zu 4 Flaggen-Teams. |
| Oddball | Ball halten = 1 Punkt pro Sekunde |
| Checkpoints | Zonen allein besetzen (4 s) → gehören dem Team, jede Zone = 1 Punkt pro Sekunde. Umkämpfte Zonen werden nicht eingenommen. |

- **Flaggen- und Ballträger können ihre Technik nicht nutzen** (nur Nahkampf), damit Träger angreifbar bleiben.
- Alle Modi **online (P2P) oder lokal** (Splitscreen bis 4). Freie Plätze werden mit **Bots** aufgefüllt
  (3 Stufen). Bots spielen auch die Ziele. Details: [NETWORKING.md](NETWORKING.md).
- **Lock-on:** Ohne Lock läuft man frei in Kamerarichtung, Angriffe richten sich leicht auf nahe Gegner aus (Soft Aim).
  **L3 lockt** auf den Gegner vor dir bzw. löst den Lock, **rechter Stick flicken** wechselt das Ziel.
- **Gegner-HP sind nie sichtbar:** Das HUD zeigt beim Gegner nur Name und Lock.
- **K.O.-Wertung:** Wer den letzten Treffer setzt (auch Realm-Schaden, Brennen), bekommt den K.O.
- **Realms haben einen Radius** (geschlossen 18 m um den Cast-Punkt, offen 10 m um den Besitzer).
  Mehrere Realms gleichzeitig sind möglich; wer im gegnerischen Realm seinen eigenen öffnet, löst einen Clash aus.

## 11. Cursed Objects ✅

Passive Gegenstände, die man vor einer Runde anlegt, wenn der Modus sie erlaubt:

| Modus | Slots |
|---|---|
| 1v1, 2v2 | 0 (aus, fair) |
| Brawl | 2 |
| Custom | 0–5 einstellbar (Standard 3) |

| Objekt | Effekt |
|---|---|
| Withered Finger | +15 % Schaden, +10 % Schaden genommen |
| Womb Painting | 10 % Lebensraub, −10 % max HP |
| Stitched Doll | Begleiter +25 % Schaden |
| Unraveling Rope | Guaranteed Hits nur halber Schaden, −10 % CE |
| Slaughter Edge | +20 % Normals-Schaden, −15 % CE-Regen |
| Soul Splitter | Black-Spark-Fenster +2 Frames |
| Thunder Charm | +15 % Technik-Schaden, −5 % max HP |
| Silk Blindfold | Parry-Fenster +3 Frames |
| Sealed Cube | Start mit halbem Realm-Meter, −15 % Realm-Gewinn |
| Promise Ring | +30 % Realm-Gewinn, −10 % Schaden |
| Patchwork Charm | +120 HP pro K.O. |
| Overtime Watch | Nach 60 s am Leben +25 % Schaden |
| Lucky Ball | 6 % Chance auf doppelten Schaden |
| Storage Worm | Cooldowns −15 %, −10 % max HP |
| Thorn Talisman | Nahkampf-Angreifer kriegen 25 % zurück, −5 % Tempo |

Per Mod erweiterbar (20 Effekt-Arten, siehe [MODDING.md](MODDING.md)).

## 12. Outfits ✅ (Datenmodell)

- **Zwei Grundkörper** (männlich, weiblich) mit gemeinsamem Skelett.
- **Jedes Kleidungsstück hat einen männlichen und einen weiblichen Schnitt**, und **jeder Schnitt passt auf
  jeden Körper**. Spieler kombinieren frei.
- **Slots:** Kopf, Gesicht, Oberteil, Unterteil, Schuhe, Accessoire.
- **Freischalten** über Siege, K.O.s, Matches, Black Sparks, geöffnete Realms oder Matches mit einer
  bestimmten Technik. Fortschritt wird im Profil gespeichert.
- **28 eingebaute Teile**, inspiriert von bekannten Looks:
  - Akademie-Uniform als Standard
  - Hooded Academy Jacket
  - Silk Blindfold, Round Shades
  - Five Color Kesa
  - Overtime Suit + Patterned Tie
  - Shrine Kimono
  - Black Compression Shirt
  - Pachinko Parlor Jacket, Hammer and Nail Pouch
  - je ein Teil pro Technik
- **Rein kosmetisch:** kein Einfluss aufs Gameplay. Per Mod erweiterbar.
- **Noch offen:** Die 3D-Modelle selbst. Der Code lädt sie, sobald sie unter `/Game/Outfits/<id>/` liegen.

## 13. Offene Punkte

- [ ] Restliche 7 Techniken implementieren (Pure Love braucht noch den Bound Spirit als Guard-Begleiter)
- [ ] Notfall-Binding Oath mitten im Match, Technik-spezifische Hidden Rewards, Vow-Journal
- [ ] Synergie-Matrix Bodies × Techniken
- [ ] Realm-Clash-Regeln pro Body
- [ ] Frame-Daten-Feinschliff per Playtest
- [ ] Wie wird das Playstyle-Profil genau gemessen?
