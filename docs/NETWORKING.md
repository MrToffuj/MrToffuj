# Online & Lobbys

**Alle Modi laufen Peer-to-Peer.** Es gibt keine eigenen Spielserver: Spieler verbinden sich direkt
miteinander und niemand ist darauf angewiesen, dass wir Server betreiben.

Status: Simulation, Rollback-Session, Bots und Lobby-Auffüllung sind fertig und getestet.
Der Netzwerk-Transport fehlt noch.

## Warum P2P hier gut funktioniert

Die komplette Kampflogik ist deterministisch (Fixed-Point, keine Zufallszahlen ohne Seed).
Übers Netz gehen deshalb **nur Inputs** (~4 Byte pro Spieler und Frame), nie Positionen.
Jeder Peer rechnet das komplette Match selbst und kommt exakt zum gleichen Ergebnis.
**Bots** denken in der Simulation, ihre Inputs werden also auf jedem Peer gleich berechnet und nie gesendet.

## Modell: Rollback, Full-Mesh

Jeder Peer schickt seine Inputs an alle anderen. `fc::Session` sagt fehlende Inputs voraus (letzter Input
wird wiederholt) und spult bei Abweichungen zurück. Das gilt für alle Modi gleich, auch für 20er-Matches.

| | 1v1 | 2v2 | Brawl / Custom (20) |
|---|---|---|---|
| Verbindungen pro Peer | 1 | 3 | bis 19 |
| Upload pro Peer | ~0,3 KB/s | ~1 KB/s | ~5 KB/s |
| Rollback 8 Frames | – | – | ~0,25 ms CPU |

Mit 20 Spielern liegen Vorhersagen öfter daneben. Das kostet kaum Rechenzeit (die Simulation braucht
~0,03 ms pro Frame für 20 Kämpfer), kann aber sichtbare Korrekturen erzeugen. Dagegen:
**2–3 Frames Input-Delay** im 20er-Modus (in 1v1 0–1) und Glättung der Darstellung.

**Getestet:** 1v1 mit 4 Frames Verzögerung, 2v2 (2 Menschen + 2 Bots) mit 5 Frames → beide Seiten landen
exakt beim gleichen Zustand.

## Lobby & Host

- Ein Spieler ist **Lobby-Host**: Er verteilt Seed, Regeln und Slot-Belegung. Danach ist er im Match ein
  ganz normaler Peer, die Simulation läuft überall gleichberechtigt.
- Verlässt der Host das Spiel, übernimmt der nächste Peer (**Host-Migration**).
- `fc::buildMatch` füllt freie Slots deterministisch mit Bots: Jeder Peer baut aus Seed + Regeln dieselbe Lobby.
- **Mods:** Peers vergleichen `Registry::contentHash()`. Nur Spieler mit exakt denselben Mods kommen in dieselbe Lobby.
- **Brawl (offen):** Spieler können jederzeit joinen. Ein Neuer übernimmt einen Bot-Slot ab einem
  vereinbarten Frame und bekommt einen Snapshot des Spielzustands (~11 KB). Wer geht, wird wieder durch
  einen Bot ersetzt.

## Was man trotzdem braucht

Reines P2P braucht einen Weg, sich zu finden und durch Router (NAT) zu kommen:

- **Matchmaking/Lobbys + NAT-Durchdringung:** EOS (Epic Online Services, kostenlos, Crossplay PC/Konsole)
  oder Steam. Die vermitteln nur die Verbindung, die Spieldaten laufen direkt zwischen den Peers.
- **Relay als Notlösung:** Wenn zwei Peers sich gar nicht direkt erreichen, leitet EOS/Steam Pakete weiter.
- Ohne jeden Dienst geht es per **Einladungscode/Direkt-IP** im lokalen Netz oder für Custom Lobbys.

## Sicherheit

- Alle Peers vergleichen alle 60 Frames `hashState`. Weicht einer ab (Desync oder Cheat), wird er
  per Mehrheitsentscheid resynchronisiert bzw. aus der Lobby genommen.
- P2P macht Cheats wie „Wallhack“ nicht schlimmer als nötig: Positionen kennt jeder Peer ohnehin.
  Inputs können nicht gefälscht werden, ohne dass die Hashes auseinanderlaufen.

## Nächste Schritte

1. Online-Subsystem anbinden (EOS, Steam als Alternative)
2. Input-Transport über UDP, jedes Paket enthält die letzten ~8 Inputs (Paketverlust egal)
3. Input-Delay je nach Modus/Ping, Glättung der Darstellung nach Rollbacks
4. Desync-Erkennung + Snapshot-Resync, Join-in-Progress für Brawl
5. Host-Migration
