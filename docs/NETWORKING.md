# Online & Lobbys

**Alle Modi laufen Peer-to-Peer.** Es gibt keine eigenen Spielserver: Spieler verbinden sich direkt
miteinander und niemand ist darauf angewiesen, dass wir Server betreiben.

**Status:**
- Fertig und getestet: Netcode (`FightCore/include/fightcore/Net.h`), Lobby-Handshake, Rollback über Netz,
  Zeitsync, Desync-Erkennung, Bot-Übernahme bei Disconnect und Rematch.
- In Unreal eingebaut: EOS fürs Internet (Login ohne Account, Lobby-Suche, NAT-Durchdringung, Relay)
  und UDP für LAN/Direkt-IP.
- Noch nicht in der Engine kompiliert: Hier läuft keine Unreal Engine und kein EOS-SDK.

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

Beide Backends implementieren `IFightTransport` (`FightNet.h`). Der Director merkt nicht, welches läuft.

### EOS (Standard fürs Internet)

`FFightEos` (`Game/Source/FightGame/Public/FightEos.h`) macht vier Schritte:

1. **Login über EOS Connect mit Device-ID:**
   - Kein Epic-Account, kein Passwort. Die ID hängt an der Installation.
   - Konsolen loggen sich stattdessen mit ihrem Plattform-Token ein (PSN, Xbox, Nintendo). Das kommt aus
     den Plattform-SDKs und geht nur auf Devkits.
2. **Lobby:**
   - Der Host öffnet eine öffentliche EOS-Lobby.
   - Joiner suchen eine offene Lobby im selben **Bucket** (`carnage:<Protokoll>:<Modus>:<Mod-Hash>`). Wer
     eine andere Version, andere Mods oder einen anderen Modus hat, findet die Lobby gar nicht erst.
   - Alternativ joint man gezielt per **Lobby-Code**: Der Host sieht ihn im HUD und schickt ihn Freunden.
3. **Handshake:** Danach läuft der normale Hello/Start-Handshake von fc::net über EOS P2P.
   Adressen sind Product-User-IDs.
4. **Match-Start:** Der Host stellt die Lobby auf „nur Einladung“, damit niemand mehr reinplatzt.

EOS P2P übernimmt NAT-Durchdringung und fällt auf Epics Relays zurück, wenn zwei Konsolen sich nicht direkt
erreichen. Die Spieldaten laufen trotzdem nicht über unsere Server, wir haben keine.

- Match-Pakete gehen unzuverlässig raus, weil der Netcode selbst wiederholt.
- EOS-Pakete sind max. 1170 Byte groß. Nur der Start-Packet einer großen Lobby ist größer: Der geht
  zerlegt über einen zuverlässigen Kanal.

**Einrichten (einmalig, kostenlos):**

1. Auf [dev.epicgames.com](https://dev.epicgames.com/portal) ein Produkt anlegen.
2. Unter *Product Settings → Clients* einen Client anlegen, mit einer Policy, die Connect, Lobbies und P2P
   erlaubt (Preset „Peer2Peer“).
3. Product-, Sandbox-, Deployment-ID sowie Client-ID und -Secret eintragen unter
   **Project Settings → Game → Online → EOS**.
4. Falls der Login scheitert: Prüfen, ob die Client-Policy „Connect“ erlaubt. Device-ID braucht sonst keine Einrichtung.

Die Client-Credentials einer Game-Client-Policy dürfen mit dem Spiel ausgeliefert werden, so ist EOS gedacht.

### Direkt-IP (LAN)

- `FFightUdpTransport` (`Game/Source/FightGame/Public/FightNet.h`):
  - UDP über das Sockets-Modul, non-blocking.
  - Der Host vergibt Peer-Ids in Ankunftsreihenfolge.
  - Clients tragen nach dem Start die Adressen der anderen ein.
- Einstellungen unter **Project Settings → Game → Online:**
  - `NetMode` (Offline / Host / Join), `Backend` (EOS / Direct IP), `DisplayName`
  - EOS: IDs + optional `LobbyCode`
  - Direct IP: `HostAddress`, `Port`
  - `WaitForConsoles` (Start, sobald so viele Konsolen da sind)
  - `LobbyTimeoutSeconds` (danach startet es trotzdem, der Rest sind Bots)
  - `InputDelay`
- `AFightDirector` nutzt online `PeerSession` statt `Session`. Lokale Spieler sitzen auf den Slots, die der
  Host vergibt (`GetLocalFighter`). Splitscreen online geht also auch.
- Outfits sind rein kosmetisch und bleiben lokal: Andere sehen dich im Standard-Look. Die Übertragung über
  die Lobby kommt als nächster Schritt.

## EOS vs. Steam

- **EOS** läuft auf PC und allen Konsolen inkl. Switch 2. Damit gibt es Crossplay, deshalb ist EOS Standard.
- **Steam Networking Sockets** gehen nur für Steam-Spieler. Sie können später als dritter `IFightTransport`
  für die Steam-Version dazukommen (gutes Relay-Netz, Einladungen übers Overlay).

## Sicherheit

- Ein Peer kann nur Inputs für seine eigenen Slots schicken. Alles andere ignorieren die anderen.
- Wer am Zustand manipuliert, fällt beim Hash-Vergleich auf.
- Positionen kennt jeder Peer ohnehin. Das ist bei P2P-Fightinggames normal.

## Nächste Schritte

1. In der Engine kompilieren und mit zwei Rechnern über EOS testen
2. Konsolen-Login (Plattform-Tokens) auf Devkits
3. Lobby-Menü im Spiel statt Project Settings
4. Outfits über die Lobby mitschicken
5. Bei Desync: Snapshot vom Host (~11 KB) statt nur Meldung
6. Join-in-Progress für Brawl (Neuer übernimmt einen Bot-Slot + Snapshot)
7. Host-Migration in der Lobby (im Match braucht man den Host nur für Disconnect-Entscheidungen)
