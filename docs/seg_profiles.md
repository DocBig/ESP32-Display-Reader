# Segment-Profile

> **Navigation:** [📖 Startseite](../README.md) | [📲 Installation](installation.md) | [🖥️ Webinterface](webinterface.md) | [📍 ROI Konfiguration](roi.md) | → **🎛️ Segment-Profile** | [🤖 Home Assistant](homeassistant.md) | [🧪 Test & Debug](test_debug.md)

---

Segment-Profile definieren das **geometrische Layout** der 7 Segmente (a–g) innerhalb einer Ziffer. Da verschiedene Display-Typen unterschiedliche Segmentproportionen haben können, lässt sich das Layout über Profile individuell anpassen.

---

## Was sind Segment-Profile?

Jede Ziffer eines 7-Segment Displays wird in 7 Messbereiche aufgeteilt:

```
 _
|_|
|_|

Segmente:
  a  = oben (horizontal)
  b  = oben rechts (vertikal)
  c  = unten rechts (vertikal)
  d  = unten (horizontal)
  e  = unten links (vertikal)
  f  = oben links (vertikal)
  g  = mitte (horizontal)
```

Ein Segment-Profil legt für jedes dieser 7 Segmente fest, **wo genau der Messbereich innerhalb der Ziffer liegt** – angegeben als relative Koordinaten (0–100 %).

Das ermöglicht eine präzise Anpassung an unterschiedliche Display-Bauformen, ohne die ROI-Größe oder Position ändern zu müssen.

---

![Symbol ROI](../images/10-segment_profil_editor.jpg)

## Standard-Profil

Das Standard-Profil ist für die meisten gängigen 7-Segment Displays geeignet und bereits vorinstalliert.

| Segment | Position | Beschreibung |
|---------|----------|--------------|
| a | oben, horizontal | rx 25 % · ry 5 % · rw 50 % · rh 8 % |
| b | oben rechts, vertikal | rx 78 % · ry 18 % · rw 10 % · rh 26 % |
| c | unten rechts, vertikal | rx 78 % · ry 56 % · rw 10 % · rh 26 % |
| d | unten, horizontal | rx 25 % · ry 88 % · rw 50 % · rh 8 % |
| e | unten links, vertikal | rx 12 % · ry 56 % · rw 10 % · rh 26 % |
| f | oben links, vertikal | rx 12 % · ry 18 % · rw 10 % · rh 26 % |
| g | mitte, horizontal | rx 25 % · ry 46 % · rw 50 % · rh 8 % |

Das Standard-Profil ist **schreibgeschützt** und kann nicht gelöscht oder umbenannt werden.

---

## Eigene Profile erstellen

Wenn das Standard-Profil nicht passt (z. B. bei ungewöhnlichen Segmentverhältnissen oder sehr kompakten Displays), kann ein eigenes Profil erstellt werden.

### Schritt 1 – Neues Profil anlegen

Im [Segment/Profiles Editor](webinterface.md#10--segmentprofiles-editor):

1. Auf **+** klicken → neues Profil wird mit Standard-Werten angelegt
2. Name im Eingabefeld vergeben und mit Enter bestätigen
3. Alternativ: bestehendes Profil über **⧉** duplizieren und anpassen

### Schritt 2 – Segmente anpassen

Ein Segment kann entweder in der **Tabelle** durch Anklicken der Zeile ausgewählt werden, oder direkt durch **Anklicken des Segments im Vorschaubild** — sofern die **Segments**-Checkbox in der Kameravorschau aktiv ist. Das ausgewählte Segment wird rot hervorgehoben. Ein erneuter Klick hebt die Auswahl auf.

In der Tabelle des Editors können für jedes Segment (a–g) vier Werte eingestellt werden:

| Spalte | Bedeutung | Wertebereich |
|--------|-----------|--------------|
| rx % | Horizontale Position des Messbereichs (von links) | 0–100 |
| ry % | Vertikale Position des Messbereichs (von oben) | 0–100 |
| rw % | Breite des Messbereichs | 1–100 |
| rh % | Höhe des Messbereichs | 1–100 |

Alle Werte sind **relativ zur Bounding-Box der jeweiligen Ziffer** angegeben.

> **Beispiel:** `rx=25, ry=5, rw=50, rh=8` bedeutet: Der Messbereich beginnt 25 % von links, 5 % von oben, ist 50 % breit und 8 % hoch.

### Schritt 3 – Profil einer ROI zuweisen

Im ROI-Einstellungsbereich ([8b – Seven-Segment ROI](webinterface.md#8b--seven-segment-roi)) das neue Profil im Dropdown **Segment Profile** auswählen.

### Schritt 4 – Konfiguration speichern

**Save Config** im [Kameravorschau-Bereich](webinterface.md#6--kameravorschau) klicken, damit das Profil dauerhaft gespeichert wird.

---

## Tipps zur Profil-Justierung

**Test Detection nutzen**
Nach jeder Änderung eine Test-Auswertung starten. Im Bereich [Status & Results](webinterface.md#11--status--results) sind die Durchschnittshelligkeit jedes Segments (`a_avg` bis `g_avg`) und der Zustand (0/1) sichtbar.

**Messbereich vergrößern**
Ein zu kleiner Messbereich kann durch einzelne helle oder dunkle Pixel beeinflusst werden. Größere Messbereiche mitteln mehr Pixel und sind stabiler.

**Messbereich verkleinern**
Ein zu großer Messbereich kann in benachbarte Segmente ragen. Bei engen Displays die Bereiche kleiner wählen.

**Überlappungen vermeiden**
Messbereiche benachbarter Segmente sollten sich möglichst nicht überschneiden, da dies zu Fehlerkennungen führen kann.

**Mit Stretch Contrast arbeiten**
Bei niedrigem Bildkontrast kann [Stretch Contrast](roi.md#seven-segment-roi) die Trennbarkeit der Segmente verbessern, ohne das Profil ändern zu müssen.

---

## Profile in der Konfigurationsdatei

Profile werden in der Konfiguration unter `seg_profiles` gespeichert und können über [Export/Import](webinterface.md#12--konfiguration-exportimport) gesichert und auf andere Geräte übertragen werden.

Beispiel-Eintrag in der JSON-Konfiguration:

```json
"seg_profiles": [
  {
    "name": "mein_display",
    "segs": [
      [0.25, 0.05, 0.50, 0.08],
      [0.78, 0.18, 0.10, 0.26],
      [0.78, 0.56, 0.10, 0.26],
      [0.25, 0.88, 0.50, 0.08],
      [0.12, 0.56, 0.10, 0.26],
      [0.12, 0.18, 0.10, 0.26],
      [0.25, 0.46, 0.50, 0.08]
    ]
  }
]
```

Reihenfolge der Segmente: **a, b, c, d, e, f, g**
Werte als Dezimalzahl (0.0–1.0, entspricht 0–100 %).

---

[⬆️ Nach oben](#)

> 📷 Alle Screenshots entstanden im Zusammenspiel mit einer **Buderus WPT260.4 AS** (Logatherm Warmwasser-Wärmepumpe).
