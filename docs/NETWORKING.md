# Online & Lobbys

**Alle Modi laufen Peer-to-Peer.** Es gibt keine eigenen Spielserver: Spieler verbinden sich direkt
miteinander und niemand ist darauf angewiesen, dass wir Server betreiben.

**Status:**
- Fertig und getestet: Netcode (`FightCore/include/fightcore/Net.h`), Lobby-Handshake, Rollback über Netz,
  Zeitsync, Desync-Erkennung, Bot-Übernahme bei Disconnect und Rematch.
- In Unreal eingebaut: UDP-Transport für LAN und Direkt-IP.
- Noch offen: NAT-Durchdringung über EOS oder Steam.

## Warum P2P hier gut funktioniert

Die komplette Kampflogik ist deterministisch (Fixed-Point, keine Zufallszahlen ohne Seed).
- Übers Netz gehen deshalb **nur Inputs** (5 Byte pro Spieler und Frame), nie Positionen.
- Jeder Peer rechnet das komplette Match selbst und kommt exakt zum gleichen Ergebnis.
- **Bots** denken in der Simulation. Ihre Inputs werden auf jedem Peer gleich berechnet und nie gesendet.

## Ablauf

```
Join-Konsole ──Hello (Version, Mod-Hash, Loadouts)──▶ Host
Host        ──Start (LobbyConfig, deine Peer-Id)───▶ alle      (3× + Wiederholungen)
alle        ◀──────Inputs / Hash, jeder mit jedem───▶ alle      (Full Mesh, 60 Hz)
```

1. **Lobby:** `LobbyHost` sammelt Hellos.
   - Falsche Version oder andere Mods werden abgelehnt (`Registry::contentHash()`).
   - Beim Start füllt `buildMatch` freie Slots mit Bots.
   - Das Ergebnis geht als `LobbyConfig` raus. Die enthält nur Namen und IDs (Body, Technik, Assist,
     Cursed Objects, Oath, Map, Regeln), keine Pointer.
2. **Gleiches Setup überall:** Jeder Peer, auch der Host, baut mit `buildSetup(lobby)` sein `MatchSetup`.
   So sind die Matches auf allen Konsolen Byte für Byte identisch.
3. **Match:** `PeerSession` wird einmal pro 60-Hz-Tick aufgerufen und macht Folgendes:
   - Pakete empfangen.
   - Einen Frame simulieren, mit Rollback, wenn ein Input anders war als vorhergesagt.
   - Eigene Inputs an alle schicken.
4. **Rematch:** Neue `PeerSession` mit `round + 1`.
   - Pakete aus einer anderen Runde werden ignoriert, Peers dürfen also ein paar Ticks versetzt neu starten.
   - Das Rundenende kommt nur aus dem **bestätigten** Zustand (`confirmedState()`). Ein vorhergesagter KO,
     der zurückgerollt wird, beendet also nie das Match.

## Pakete

| Typ | Inhalt |
|---|---|
| Hello | Protokoll-Version, Mod-Hash, lokale Spieler (Body, Technik, Assist, Team, Oath, Spielstil-Profil, Cursed Objects) |
| Start | Peer-Id + komplette LobbyConfig inkl. Adressliste aller Peers (für Full Mesh) |
| Inputs | Runde, Ack, eigener Frame, Frame-Vorsprung, ab-Frame, Anzahl, Inputs aller eigenen Slots |
| Hash | Runde, Frame, `hashState` (alle 60 Frames) |
| Drop | Runde, wer weg ist, ab welchem Frame ein Bot übernimmt, letzte bekannte Inputs |

- **Paketverlust ist egal:** Jedes Inputs-Paket wiederholt alles, was die Gegenseite noch nicht bestätigt hat.
- **Input-Delay:** Standard 2 Frames, einstellbar 0–6. Das versteckt die meisten Rollbacks.
- **Zeitsync:** Wer klar vor den anderen liegt (Frame-Vorsprung ≥ 2), setzt ab und zu einen Tick aus.
  So läuft niemand dauerhaft voraus und das Rollback-Fenster (32 Frames) reicht.
- **Desync:** Alle 60 Frames vergleichen die Peers den Hash eines bestätigten Frames.
  Bei Abweichung meldet `desynced()` das und das HUD zeigt es an.
- **Disconnect:** Hört der Host 3 s nichts von einem Peer, übernimmt ab einem festen Frame ein Bot dessen
  Slots. Der Host schickt allen die letzten Inputs des Leavers mit, damit alle bis dahin dasselbe rechnen.
  Der Rest spielt weiter.

## Gemessen (Tests über ein simuliertes Netz)

`FightCore/tests/FakeNetwork.h` simuliert Latenz, Jitter und Paketverlust deterministisch.

| Test | Netz | Ergebnis |
|---|---|---|
| 1v1 | 4 Frames Latenz, Jitter 3, 10 % Verlust | synchron, 4,2 KB/s pro Peer |
| 2v2, 2 Konsolen mit je 2 Spielern | 3 Frames, 5 % | synchron, 9,1 KB/s pro Peer |
| Brawl, 6 Konsolen + 14 Bots | 5 Frames, Jitter 4, 8 % Verlust | synchron, ~25 KB/s pro Peer |
| Disconnect | Peer fällt mitten im Match weg | Bot übernimmt, Rest bleibt synchron |
| Desync | ein Peer mit falschem Setup | wird erkannt |
| Später Start | ein Peer hängt 12 Ticks hinterher | holt auf (≤ 4 Frames Abstand) |
| Rematch | Peers starten 9 Ticks versetzt neu | synchron |

Jeder Test prüft auch, dass das Netzmatch exakt dem Offline-Nachrechnen derselben Inputs entspricht.
Unter ASAN/UBSan ist alles sauber.

Die Bandbreite ist großzügig, weil jedes Paket bis zu 24 unbestätigte Frames wiederholt.
Bei echter Latenz kommen Acks schneller, dann ist es weniger.

## Unreal

- `FFightUdpTransport` (`Game/Source/FightGame/Public/FightNet.h`):
  - UDP über das Sockets-Modul, non-blocking.
  - Der Host vergibt Peer-Ids in Ankunftsreihenfolge.
  - Clients tragen nach dem Start die Adressen der anderen ein.
- Einstellungen unter **Project Settings → Game → Online:**
  - `NetMode` (Offline / Host / Join)
  - `HostAddress`, `Port`
  - `WaitForConsoles` (Start, sobald so viele Konsolen da sind)
  - `LobbyTimeoutSeconds` (danach startet es trotzdem, der Rest sind Bots)
  - `InputDelay`
- `AFightDirector` nutzt online `PeerSession` statt `Session`. Lokale Spieler sitzen auf den Slots, die der
  Host vergibt (`GetLocalFighter`). Splitscreen online geht also auch.
- Outfits sind rein kosmetisch und bleiben lokal: Andere sehen dich im Standard-Look. Die Übertragung über
  die Lobby kommt als nächster Schritt.

## Was man trotzdem braucht

Direkt-IP klappt im LAN oder mit Port-Weiterleitung. Fürs Internet braucht man einen Weg, sich zu finden
und durch Router (NAT) zu kommen:

- **Matchmaking/Lobbys + NAT-Durchdringung:** EOS (Epic Online Services, kostenlos, Crossplay PC/Konsole)
  oder Steam. Die vermitteln nur die Verbindung, die Spieldaten laufen weiter direkt zwischen den Peers.
  Dafür reicht ein zweiter `fc::net::Transport` (EOS P2P bzw. Steam Networking Sockets). Der Netcode
  darüber bleibt gleich.
- **Relay als Notlösung:** Wenn zwei Peers sich gar nicht direkt erreichen, leiten EOS/Steam Pakete weiter.

## Sicherheit

- Ein Peer kann nur Inputs für seine eigenen Slots schicken. Alles andere ignorieren die anderen.
- Wer am Zustand manipuliert, fällt beim Hash-Vergleich auf.
- Positionen kennt jeder Peer ohnehin. Das ist bei P2P-Fightinggames normal.

## Nächste Schritte

1. EOS-Transport (Lobbys, NAT, Relay), Steam als Alternative
2. Outfits über die Lobby mitschicken
3. Bei Desync: Snapshot vom Host (~11 KB) statt nur Meldung
4. Join-in-Progress für Brawl (Neuer übernimmt einen Bot-Slot + Snapshot)
5. Host-Migration in der Lobby (im Match braucht man den Host nur für Disconnect-Entscheidungen)
