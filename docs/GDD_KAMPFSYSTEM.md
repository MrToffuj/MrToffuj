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
| Light / Heavy / Grab | Normals (2 + Heavy = Launcher) |
| Guard (R1) | Blocken, frisch gedrückt kurz vor Treffer = **Parry** |
| Guard + Dash | **Dodge** (Unverwundbarkeit) |
| Mod (L1) + Light / Heavy / Grab | Skill 1 / 2 / 3 (Offense / Utility / Defense) |
| Ult (L2) | Apex-Technik |
| Mod + Ult | Realm öffnen |
| Ult (während gegnerischem Realm, halten) | Schutzkreis |
| Guard + Grab | **Essenz-Burst** – Notfall-Pushback, auch aus Hitstun |
| Mod + Guard (halten) | Rückfluss |
| Mod + Dash | Verstärkung an/aus |

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
| Summoner | 2 | 2 | 3 | 3 | Assist-Slot (⏳ noch nicht implementiert) |

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
| Schattenmenagerie | Schattenpool mit Bestien-Slots | Schattenhunde / Nachtschwinge (Anti-Air) / Schlundkröte (Grab-Pull) | Flutkoloss (Zone) | Schattengarten (BuffField) | Gefallene Bestien weg für die Runde | ⏳ |
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

## 9. Offene Punkte

- [ ] Summoner-Assist (Entity-System für Begleiter, auch für Schattenmenagerie nötig)
- [ ] Restliche 8 Techniken implementieren
- [ ] Notfall-Pakt mitten im Match, Technik-spezifische Hidden Rewards, Vow-Journal
- [ ] Synergie-Matrix Bodies × Techniken
- [ ] Realm-Clash-Regeln pro Body
- [ ] Frame-Daten-Feinschliff per Playtest
- [ ] Wie wird das Playstyle-Profil genau gemessen?
