# Zerstörbare Welt

Ziel: Fast alles ist zerstörbar und verhält sich wie sein Material. Wände zerfallen, Boden bekommt
Krater, Holz brennt, **Staub überall**.

## Zwei Ebenen (wichtig für Rollback!)

| Ebene | Was | Wo |
|---|---|---|
| **Gameplay-Zerstörung** | Wand-HP, Wandbruch → Arena wird größer / Stage-Übergang | `FightCore` (deterministisch) |
| **Kosmetische Zerstörung** | Trümmer, Krater, Risse, Feuer, Staub, Splitter | Unreal (Chaos + Niagara), reagiert auf `ImpactEvent`s |

Physik-Trümmer sind **nie** gameplay-relevant – sonst müssten sie im Rollback mitsimuliert werden
(auf Switch 2 unmöglich). Events werden über `(frame, kind, source, tag)` dedupliziert, weil Rollback
Frames neu simuliert.

## Material-Tabelle

| Material | Reaktion | Staub/Partikel | Sonstiges |
|---|---|---|---|
| Stein/Beton | Risse → bricht in vorfrakturierte Chunks, fällt in sich zusammen | dicke graue Staubwolke | Bewehrungsstahl ragt raus |
| Holz | splittert, bricht entlang Faser | Holzspäne, feiner Staub | **brennt**: Glut → Flammen → verkohlt → zerfällt |
| Glas | zerspringt in Scherben | Glitzer | schnell, viele kleine Teile |
| Metall | verbeugt sich (Vertex-Deformation), reißt bei viel Kraft | Funken | kein Zerfall, nur Verformung |
| Erde/Boden | **Krater** (Stärke nach `ImpactClass`) | Erdklumpen, brauner Staub | Krater bleiben bis Rundenende |
| Wasser | Spritzer, Wellenring | Gischt | – |

## Umsetzung in Unreal

- **Wände/Objekte**: Chaos *Geometry Collections*, vorfrakturiert (mehrere Bruchstufen). Bruch via
  *Fields* an der Event-Position, Kraft aus `ImpactEvent::force`.
- **Krater**: Decal + Mesh-Stempel (vorgefertigte Kratermeshes in 3 Größen) + Heightfield-Deformation
  auf PC. Handheld nur Decal + Mesh.
- **Feuer**: Material hat Brennwert. `Ignite`-Event oder Feuer-Techniken entzünden; Feuer breitet sich
  über ein grobes Zellgitter auf Holz aus, Holz verkohlt (Material-Parameter) und zerfällt.
- **Staub**: Niagara, aber **im Anime-Look** (siehe ART_STYLE): handgezeichnete Flipbook-Wolken +
  cel-shaded Volumen-Billows. Staub bleibt länger in der Luft und hängt in der Szene.
- **Rubble-Swap**: Trümmer, die liegen bleiben, werden nach ~2 s durch statische Meshes ersetzt (spart Physik).

## Budgets

| | PC/PS5/Xbox | Steam Deck | Switch 2 |
|---|---|---|---|
| Aktive Physik-Chunks | 600 | 200 | 120 |
| Trümmer-Lebenszeit | 8 s | 4 s | 3 s |
| Staub-Partikel | voll | 50 % | 35 % |
| Krater | Mesh + Heightfield | Mesh | Mesh/Decal |

Optik bleibt auf allen Plattformen gleich „laut“ – Handhelds bekommen weniger, aber größere Teile.
