# Artstyle: „Jeder Frame ein Screenshot“

Ziel: So 2D wie möglich. Pausiere irgendwann → es soll aussehen wie ein Key-Frame aus einem Top-Anime.

## Rendering

- **Cel-Shading** mit 2–3 harten Tonstufen, kein PBR, kein realistisches Licht.
- **Eigene Licht-Richtung pro Charakter** (Material Parameter Collection) statt Stage-Licht →
  Gesichter immer schön schattiert.
- **Schatten-Kontrolle**: bearbeitete Vertex-Normalen im Gesicht + Shadow-Mask-Textur
  (wo Schatten erlaubt ist), damit nie „3D-Schatten“ entstehen.
- **Outlines**: Inverted-Hull für Charaktere, Linienstärke über Vertex-Color steuerbar
  (dünner an Spitzen). Innere Linien in die Textur gemalt. Umgebung: Post-Process-Lines (Tiefe/Normalen).
- **Kein** Lumen, keine Virtual Shadow Maps, kein Motion Blur → nebenbei **mega handheld-freundlich**.

## Animation

- **Stepped Animation**: Visuelle Posen laufen „auf 2s/3s“ (Keyframes gehalten), wie handgezeichnet.
  Die Simulation bleibt bei 60 Hz – nur die Darstellung springt.
- **Smear-Frames** & Mehrfach-Gliedmaßen bei schnellen Schlägen.
- Modell-Anpassungen pro Pose erlaubt (Arm länger, Faust größer) – Perspektive wird übertrieben.

## Impact

- **Impact-Frames**: Bei `Heavy`/`Crushing` 2–4 Frames invertiertes Schwarz/Weiß-Bild mit dicken Tusche-Linien.
- **Hitstop** (kommt aus der Simulation) + Kamera-Shake + Speedlines als Screen-Overlay.
- Kurzer Farb-Flash in der Technik-Farbe des Charakters.

## Effekte

- Alle VFX als **handgezeichnete Flipbooks** oder cel-shaded Meshes – keine realistischen Partikel.
- Staub, Rauch, Feuer, Trümmer: alle mit Outline/Toon-Shading, damit sie zur Szene passen.
- Techniken haben eine klare Farb-Identität (Signaturfarbe pro Charakter).

## Kamera

- Niedriges FOV (~30–40°) für flachen, „gezeichneten“ Look.
- Cinematic-Kameras für Techniken & Realm-Öffnung (Spezialaufnahmen, Dutch Angles, Close-ups).

## Hintergründe

- Gemalter Look: Texturen mit Pinsel-Struktur, vereinfachte Formen, Atmosphärische Tiefe (Farbverlauf statt Nebel).
- Colorgrading pro Stage + leichtes Film-Grain.

## Screenshot-Check (für jedes Asset)

1. Sieht ein zufälliger Frame wie ein Anime-Still aus?
2. Gibt es irgendwo „3D-Glanz“, weiche Verläufe oder realistische Schatten? → raus.
3. Sind Silhouetten auch auf 7" (Handheld) klar lesbar?
