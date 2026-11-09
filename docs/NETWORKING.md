# Online & Lobbys

Status: **Simulation, Rollback-Session, Bots und Lobby-Auffüllung sind fertig und getestet.**
Der Netzwerk-Transport (Matchmaking, Verbindungen) fehlt noch.

## Grundidee

Die komplette Kampflogik ist deterministisch (Fixed-Point, keine Zufallszahlen ohne Seed).
Übers Netz gehen deshalb **nur Inputs** (~3 Byte pro Spieler und Frame), nie Positionen.
Bots denken **in der Simulation**: Ihre Inputs werden auf jedem Rechner gleich berechnet und
müssen nicht übertragen werden.

## Modell pro Modus

| Modus | Spieler | Modell |
|---|---|---|
| 1v1 | 2 | Peer-to-Peer-Rollback (Relay-Server als Fallback bei NAT-Problemen) |
| 2v2 | 4 | Rollback über Relay-Server (sammelt und verteilt Inputs) |
| Brawl | bis 20 | Dedizierter Server führt die Simulation **autoritativ**, Clients machen Rollback-Prediction |

**Rollback (`fc::Session`):** Lokale Inputs werden sofort simuliert, fehlende Inputs werden
vorhergesagt (letzter Input wiederholt). Weicht ein ankommender Input ab, wird der Zustand des Frames
geladen und neu simuliert. Getestet für 1v1 mit 4 Frames Verzögerung und 2v2 (2 Menschen + 2 Bots)
mit 5 Frames Verzögerung: Beide Seiten landen exakt beim gleichen Zustand.

**Brawl:** Bei 20 Spielern liegen öfter Vorhersagen daneben, deshalb gibt der Server die Inputs vor
(bestätigte Frames) und schickt regelmäßig Prüfsummen (`hashState`). Bei einer Abweichung schickt er
einen Snapshot. Der komplette Zustand ist ~11 KB groß, dadurch sind Join-in-Progress und Resync billig.

## Budgets (gemessen)

| | Wert |
|---|---|
| Simulation, 20 Kämpfer | ~0,025 ms pro Frame (0,15 % von 16,6 ms) |
| Rollback 8 Frames, 20 Kämpfer | ~0,2 ms |
| Spielzustand | ~11 KB (Rollback-Verlauf 32 Frames ≈ 350 KB) |
| Bandbreite Brawl pro Client | ~5 KB/s rein (20 × 60 × ~4 Byte) |

## Lobbys

`fc::buildMatch(mode, humans, count, botLevel, seed)`:

- Menschen belegen die ersten Slots, **freie Slots werden mit Bots gefüllt** (Body, Technik, Assist per Seed).
- 2v2: Menschen behalten ihr Team, Bots füllen das kleinere Team auf.
- Der Seed wird vom Host verteilt → alle Clients bauen exakt dieselbe Lobby.
- Lokal (Splitscreen, bis 4 Spieler) läuft genauso: lokale Spieler = Menschen, Rest Bots.

## Nächste Schritte

1. Online-Subsystem: **EOS** (Crossplay PC/Konsole) bzw. Steam für Lobbys/Matchmaking
2. Input-Transport: UDP mit Redundanz (jedes Paket enthält die letzten N Inputs)
3. Relay-/Dedicated-Server-Build (Unreal Server-Target, nutzt dieselbe FightCore-Simulation)
4. Desync-Erkennung über `hashState` alle 60 Frames
5. Spieler, die rausfliegen, werden mitten im Match durch Bots ersetzt (`loadout.bot = true`)
