# GDD: Kampfsystem

> Basierend auf dem GDD von Joshua Jäger (09.11.2026). Gameplay ist von Fluch-Energie-Kampfsystemen
> inspiriert, **alle Namen sind eigenes IP**. ✅ = in `FightCore` implementiert und getestet.

## 1. Vision & Design-Pillars

Jeder Spieler baut seinen Char aus **Body + Technik** zusammen. Alle teilen dieselbe Steuerung und
dieselben Systeme. Der Spielstil entsteht nur aus der Kombi.

1. **Gleiche Basis, eigener Stil.** Identische Inputs und Systeme, Unterschiede nur über Body und Technik.
2. **Build statt Roster.** Body und Technik frei kombinierbar, Anti-Synergien sind Absicht.
3. **Jede Stärke hat ein Gegenmittel.** Schere-Stein-Papier auf Body-Ebene, Schwäche-Pflicht pro Technik.
4. **Entdecken statt Tutorial.** Pakte sind ein verstecktes Layer.
5. **Timing schlägt Zahlen.** Nullfunke, Parry und Realm-Clash belohnen Reads und Execution.

## Begriffe

| Begriff | Bedeutung |
|---|---|
| **Essenz** | Kampfenergie (Skills, Apex, Burst, Verstärkung) |
| **Technik** | Die angeborene Spezialfähigkeit eines Chars |
| **Apex-Technik** | Maximum-Move der Technik (Burst / Zone / Transform) |
| **Realm** | Ultimate-Zone mit Sure-Hit-Effekt |
| **Schutzkreis** | Anti-Realm-Konter |
| **Nullfunke** | Perfekt getimter Treffer (×2,5 Schaden) |
| **Rückfluss** | Heilung mit Essenz |
| **Pakt** | Verstecktes Opfer-/Belohnungssystem |
| **Nullgeboren** | Sonder-Build ohne Technik |

## 2. Gemeinsame Basis ✅

### Steuerung (Controller)

| Input | Aktion |
|---|---|
| Stick | hoch = zum Gegner, runter = zurück, links/rechts = umkreisen (Kamera hinter dem Kämpfer) |
| Light / Heavy / Grab | Normals (zurück + Heavy = Launcher) |
| Guard (R1) | Blocken, frisch gedrückt kurz vor Treffer = **Parry** |
| Guard + Dash | **Dodge** (Unverwundbarkeit) |
| Mod (L1) + Light / Heavy / Grab | Skill 1 / 2 / 3 (Offense / Utility / Defense) |
| Ult (L2) | Apex-Technik |
| Mod + Ult | Realm öffnen |
| Ult (während gegnerischem Realm, halten) | Schutzkreis |
| Guard + Grab | **Essenz-Burst** – Notfall-Pushback, auch aus Hitstun |
| Mod + Guard (halten) | Rückfluss |
| Mod + Dash | Verstärkung an/aus |
| Assist (R3) + Richtung | Begleiter rufen (siehe 9) |

Modifier zählen **im Frame des Drückens** (wichtig für den Input-Buffer).

### Ressourcen

| Ressource | Funktion | Lädt über |
|---|---|---|
| HP | Lebensleiste | Rückfluss (max. 30 % pro Runde) |
| Essenz | Skills, Apex, Burst, Verstärkung, Core Rules | Passive Regen, Body-abhängig |
| Realm-Meter | Voraussetzung für Realm | Schaden, Nullfunke, Parry |
| Burnout | Essenz leer → Technik 3 s gesperrt | automatisch |

### Universal-Mechaniken

- **Verstärkung:** +20 % Schaden, −20 % Schaden durch Essenz-Angriffe, kostet Essenz/Frame.
- **Nullfunke:** Normal genau auf das Hitstop-Ende des vorherigen Treffers gedrückt
  (±1 Frame, Brawler ±3). ×2,5 Schaden, +25 % Realm-Meter, extra Hitstop → Impact-Frame.
- **Rückfluss:** Heilung, Char steht still und ist verwundbar.
- **Schutzkreis:** blockt Sure-Hit, kostet Essenz pro Frame (Technician halb).
- **Realm:** bei 100 % Meter, siehe 5.

## 3. Body Types ✅

Jeder Body hat genau **10 Punkte** auf HP / Speed / Essenz / Range.

| Body | HP | Speed | Essenz | Range | Passiv |
|---|---|---|---|---|---|
| Heavy | 5 | 1 | 2 | 2 | Super Armor auf Heavies, Grabs ×1,5 |
| Speedster | 1 | 5 | 2 | 2 | Dash-Cancel nach Treffer, Crit (×1,3) von Seite/Rücken |
| Caster | 1 | 2 | 4 | 3 | Regen ×1,5, Cooldowns −25 % |
| Brawler | 3 | 3 | 2 | 2 | Nullfunke-Fenster ±3 |
| Technician | 2 | 3 | 3 | 2 | Parry-Fenster 8 statt 5, Schutzkreis halb so teuer |
| Summoner | 2 | 2 | 3 | 3 | Assist-Slot: 1 permanenter Begleiter |

**Nullgeboren (Sonder-Build):** Speedster ohne Technik und ohne Essenz. Dafür +20 % Speed,
+15 % Schaden, immun gegen Sure-Hit, begrenzter Pakt-Wert.

**Anti-Synergien sind gewollt:** z. B. Nullraum auf Heavy = Feld-Tank mit Essenz-Hunger.

Abgeleitete Werte (`src/Body.cpp`): HP = 600 + 120 × HP, Essenz = 400 + 150 × Essenz,
Reichweite ×(0,85 + 0,075 × Range), Realm-Gewinn 80 % + 10 % × Essenz.

## 4. Technik-Template ✅

| Baustein | Anzahl | Regel |
|---|---|---|
| Core Rule | 1 | Passive Eigenheit, die alle Skills färbt |
| Skills | 3 | Offense, Utility, Defense/Movement – kosten Essenz, haben Cooldown |
| Apex-Technik | 1 | Burst / Zone / Transform, kostet 50 % Essenz, 1× pro Runde, bestrafbares Startup |
| Realm | 1 | Klasse Damage / Lock / Debuff / BuffField |
| Schwäche | 1 | Bewusst eingebaut, nie optional |

**Design-Regel:** Fehlt Approach, Anti-Air, Defense oder Comeback → bei Skills nachbessern, nie bei Stats.

## 5. Realm ✅

Cast-Zeit (60 F) und Dauer (600 F) für alle gleich, nur der Effekt unterscheidet sich.

| Klasse | Effekt |
|---|---|
| Damage | Sure-Hit: 20 Schaden alle 0,5 s |
| Lock | Gegner alle 200 F für 90 F festgesetzt, kaum Schaden |
| Debuff | Gegner: keine Essenz-Regen, Skills +50 % teurer, +15 % Schaden genommen |
| BuffField | Besitzer: Cooldowns 2× schneller, Skills −50 % Kosten, +15 % Schaden |

- **Geschlossener Realm:** Besitzer bewegt sich mit halbem Tempo. **Offener Realm** (Pakt-Unlock):
  volle Bewegung, Wirkung nur im Radius 10.
- **Konter:** Schutzkreis, Gegen-Realm → **Clash** (90 F Light hämmern, mehr Drücke gewinnt, bei
  Gleichstand hält der Verteidiger).

## 6. Technik-Katalog

| Technik | Core Rule | Skills (Off / Util / Def) | Apex | Realm | Schwäche | Status |
|---|---|---|---|---|---|---|
| **Nullraum** | Distanzfeld: Auto-Block gegen Projektile & Lights, kostet Essenz | Abstoßung / Anziehung / Faltschritt | Singularität (Burst) | Ereignishorizont (Lock) | Kein Essenz = kein Feld, Grabs gehen durch | ✅ |
| **Glutkern** | Hitze-Stacks: Gegner brennt | Glutfalter / Magmasäule (Zone) / Hitzemantel | Sternfall (Burst) | Schmelzkammer (Damage) | Setup, langes Startup | ✅ |
| **Schattenmenagerie** | Schattenpool: bis zu 2 Bestien als Begleiter | Schattenhunde / Nachtschwinge (Anti-Air) / Schlundkröte (Grab-Pull) | Flutkoloss (Zone) | Schattengarten (BuffField) | Gefallene Bestien weg für die Runde | ✅ |
| Schnittkunst | Schnitt-Schaden skaliert mit HP-Verlust des Gegners | Fernschnitt / Nahschnitt / Schnittnetz | Flammenpfeil (Burst) | Schnittaltar (Damage) | Blockbar, wenig Defense | ⏳ |
| Echo | Echo-Marke: geparrter Skill 20 s gespeichert | Geisterhieb / Geisterschild / Echo-Tausch | Geist entfesselt (Transform) | Spiegelsaal (BuffField) | Ohne Echo dünnes Kit | ⏳ |
| Seelenformung | Seelenberührung baut Debuff auf | Seelenhand / Leibverformung / Seelenpanzer | Seelenspaltung (Transform) | Vollkommene Form (Debuff) | Braucht Kontakt | ⏳ |
| Tauschhand | Klatschen tauscht Positionen (auch Objekte) | Tauschschlag / Köder-Tausch / Objekt-Tausch | Tauschwirbel (Zone) | Lock | Klatschen kostet Tempo, lesbar | ⏳ |
| Blutbindung | Blut-Meter, Skills teils mit HP bezahlt | Blutlanze / Roter Strom / Verdichtung | Blutnova (Burst) | BuffField (gratis Blut) | Glass Cannon | ⏳ |
| Taktgesetz | 24-Bilder-Regel: wer deine Frames bricht, friert ein | Bildsprung / Bildfessel / Bildfolge | Zeitraffer (Transform) | Debuff | Lesbar, Regelbruch friert dich selbst ein | ⏳ |
| Befehlsstimme | Befehle kosten Essenz + Selbstschaden | „Halt!“ / „Zerbirst!“ / „Fort!“ | Massenbefehl (Burst) | Lock | Selbstschaden, wenig Defense | ⏳ |

## 7. Pakte ✅ (Grundsystem)

Verstecktes Layer: Spieler wählt **Kategorie + Größe**, nicht die Belohnung.

| Kategorie | Opfer (`detail`) |
|---|---|
| Körper | Max-HP, Speed, Block-Stärke |
| Ressource | Essenz-Pool, Essenz-Regen, Rückfluss-Heilung |
| Technik | Skill 1/2/3 sperren, Realm verbieten, Apex verbieten |
| Regel | „nie blocken“, „nur Nahkampf“ |
| Zeit | gilt nur 60 s, Wert ×1,5 |
| Begleiter | Begleiter-HP, Slot sperren, Begleiter dauerhaft verlieren |

**Versteckter Wert** = Größe (1/2/4) × Empfindlichkeit (0–10, abhängig von Body + Playstyle-Profil).
Ungenutzte Skills sind fast wertlos, Caster zahlt für Regen viel, Heavy fast nichts.
Nullgeboren: Wert max. 12.

| Wert | Stufe | Folge | Aura |
|---|---|---|---|
| < 4 | Zu billig | Backlash: Essenz-Leck, Cooldowns +25 % | Flackern |
| 4–11 | Fair | +Schaden (max 15 %), +Realm-Gewinn | Ruhig |
| 12–23 | Hoch | + **Offener Realm** (Nullgeboren: +1 Nullfunke-Frame) | Knistern |
| ≥ 24 | Überzahlt | + kleiner Bonus (+5 %), nie das Doppelte | Knistern |

Regeln: 1 Pakt pro Match, Belohnung nie größer als das Opfer, Gegner sieht nur **dass** ein Pakt aktiv ist.
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
ein Spieler, zwei Ziele, **ein Essenz-Pool**. Code: `include/fightcore/Companion.h`, `src/Companion.cpp`.

### Wer bekommt Begleiter

| Quelle | Begleiter | Regel |
|---|---|---|
| Summoner-Body | 1 permanenter Begleiter (frei wählbar) | Fällt aus → kehrt nach 15 s zurück, Revive kostet 150 Essenz (wartet, bis genug da ist) |
| Schattenmenagerie | bis zu 2 Bestien aus dem Schattenpool | Skill beschwört; 3. Bestie ersetzt die älteste; gefallene sind für die Runde weg (Skill gesperrt) |
| Andere Bodies | keiner | nur über zeitlich begrenzten Pakt |

Summoner + Schattenmenagerie geht: Assist + 2 Bestien (3 Slots).

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
- **Upkeep:** jeder Begleiter auf dem Feld zieht Essenz pro Frame → weniger für Skills, Apex, Realm.
- **Schadens-Cap:** Begleiter-Treffer machen 60 % eines vergleichbaren Skills (Engine-Regel).

### Begleiter-Typen & Aktions-Bausteine

| Typ | Aufgabe | Summoner-Beispiel | Schattenpool |
|---|---|---|---|
| Striker | Offense, Druck, Combo-Verlängerung | Klingenwolf | Schattenhunde |
| Zoner | Projektile, Anti-Air, Raumkontrolle | Funkeneule | Nachtschwinge |
| Support | Grabs, Pulls, Utility | Rankenkröte | Schlundkröte |
| Guard | blockt für den Spieler (auch beim Rückfluss) | Steinwächter | – |

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
- Begleiter sind **nicht realm-immun**: Sure-Hit-Schaden und Lock erfassen sie (auch wenn der Spieler im Schutzkreis steht).
- Begleiter verlieren → weniger Druck, Utility und Bond.

### Pakt-Anbindung

Opfer-Feld **Begleiter**: HP, Slot sperren, Begleiter dauerhaft verlieren. Ohne Begleiter wertlos (Backlash).

| Stufe | Hidden Reward |
|---|---|
| Hoch | **Paktgeist**: temporärer Zusatz-Begleiter (30 s, kein Upkeep) |
| Überzahlt | **Wandler**: mächtige Sonderform mit **Adaption** – dieselbe Angriffs-Kategorie macht beim 2. Treffer nur noch halben Schaden |

Bodies ohne Begleiter: ein **Zeit-Pakt** mit hohem Wert gibt einen Paktgeist für die Dauer des Pakts.

## 10. Spielmodi ✅

| Modus | Spieler | Regeln | Arena |
|---|---|---|---|
| **1v1** | 2 | Elimination | 24 × 24 m |
| **2v2** | 4 | Team-Elimination, kein Friendly Fire | 28 × 28 m |
| **Brawl** | bis 20 | Free-for-all, Respawn nach 5 s, 5 min Zeitlimit oder 15 K.O.s, meiste K.O.s gewinnen | 120 × 120 m |

- Alle Modi **online oder lokal** (Splitscreen bis 4 Spieler). Nicht volle Lobbys werden mit **Bots**
  aufgefüllt (3 Stufen: leicht / normal / schwer). Details: [NETWORKING.md](NETWORKING.md).
- **Lock-on:** automatisch auf den nächsten Gegner, L3 wechselt das Ziel (bleibt dann fest, bis es K.O. ist).
- **K.O.-Wertung:** Wer den letzten Treffer setzt (auch Realm-Schaden, Brennen), bekommt den K.O.
- **Realms haben einen Radius** (geschlossen 18 m um den Cast-Punkt, offen 10 m um den Besitzer).
  Im Brawl kann es mehrere Realms gleichzeitig geben. Wer in einem gegnerischen Realm seinen eigenen
  öffnet, löst einen Clash gegen dessen Besitzer aus.

## 11. Offene Punkte

- [ ] Restliche 7 Techniken implementieren (Echo braucht noch einen Guard-Begleiter)
- [ ] Notfall-Pakt mitten im Match, Technik-spezifische Hidden Rewards, Vow-Journal
- [ ] Synergie-Matrix Bodies × Techniken
- [ ] Realm-Clash-Regeln pro Body
- [ ] Frame-Daten-Feinschliff per Playtest
- [ ] Wie wird das Playstyle-Profil genau gemessen?
