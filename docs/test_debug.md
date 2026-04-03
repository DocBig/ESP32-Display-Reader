# Test und Fehlersuche

> **Navigation:** [📖 Startseite](../README.md) | [📲 Installation](installation.md) | [🖥️ Webinterface](webinterface.md) | [📍 ROI Konfiguration](roi.md) | [🎛️ Segment-Profile](seg_profiles.md) | [🤖 Home Assistant](homeassistant.md) | → **🧪 Test & Debug**

---

## Überblick

Dieser Bereich beschreibt die verfügbaren Test- und Debug-Funktionen zur Überprüfung der Funktionalität des ESP32 Display Readers.

---

## 🧪 Test Detection (Testfunktion)

Im Webinterface kann unter den Schaltflächen der Vorschau eine **"Test Detection"** Funktion verwendet werden.

Diese Funktion:
* Nimmt ein **neues Livebild** auf — es wird **nicht** das angezeigte Vorschaubild verwendet
* Bewertet alle konfigurierten ROIs in **Echtzeit**
* Zeigt das Ergebnis im **Detection Result** Bereich an
* Hilft bei der Überprüfung und Optimierung der ROI-Einstellungen

### Verwendung:

1. Positioniere die Kamera auf das Display
2. Konfiguriere mindestens eine ROI (Regions of Interest)
3. Klicke auf **"Test Detection"**
4. Das Ergebnis wird als JSON angezeigt

---

## 📊 Detection Result

Das Ergebnis wird im **Detection Result** Fenster angezeigt (unten rechts im Webinterface).

# Debug-Ausgabe – Heizen und Isttemperatur

## Beispiel JSON (reduziert)

```json
{
  "ok": true,
  "result": {
    "valid": true,
    "Heizen": true,
    "Isttemperatur": 56
  },
  "debug": {
    "symbols": [
      {
        "id": "Heizen",
        "avg": 100,
        "state": true,
        "threshold_on": 80,
        "threshold_off": 70,
        "invert_logic": false
      }
    ],
    "sevenseg": [
      {
        "id": "Isttemperatur",
        "digits": 2,
        "decimal_places": 0,
        "geometry_ok": true,
        "digit_w": 68,
        "d0": {
          "segments": {
            "a": 1,
            "b": 0,
            "c": 1,
            "d": 1,
            "e": 0,
            "f": 1,
            "g": 1,
            "a_avg": 255,
            "b_avg": 66,
            "c_avg": 254,
            "d_avg": 255,
            "e_avg": 55,
            "f_avg": 255,
            "g_avg": 250,
            "threshold": 105,
            "invert_logic": false,
            "bits": 109
          },
          "value": 5
        },
        "d1": {
          "segments": {
            "a": 1,
            "b": 0,
            "c": 1,
            "d": 1,
            "e": 1,
            "f": 1,
            "g": 1,
            "a_avg": 255,
            "b_avg": 43,
            "c_avg": 253,
            "d_avg": 254,
            "e_avg": 254,
            "f_avg": 255,
            "g_avg": 248,
            "threshold": 105,
            "invert_logic": false,
            "bits": 125
          },
          "value": 6
        }
      }
    ]
  }
}
```

---

# Bedeutung der Felder

## Allgemeiner Status

| Feld    | Bedeutung                                      |
| ------- | ---------------------------------------------- |
| `ok`    | Verarbeitung des Bildes war erfolgreich        |
| `valid` | Ergebnis konnte eindeutig interpretiert werden |

---

# Ergebniswerte

## Heizen

```json
"Heizen": true
```

Bedeutung:

* Das Heizsymbol wurde als **aktiv** erkannt.

## Isttemperatur

```json
"Isttemperatur": 56
```

Bedeutung:

* Die 7-Segmentanzeige wurde als **56°C** interpretiert.

---

# Debugbereich

Der `debug` Bereich enthält **Rohdaten der Bildanalyse** und ist hauptsächlich für Diagnose und Kalibrierung gedacht.

---

# Debug – Symbolerkennung

```json
{
  "id": "Heizen",
  "avg": 100,
  "state": true,
  "threshold_on": 80,
  "threshold_off": 70
}
```

| Feld            | Bedeutung                                          |
| --------------- | -------------------------------------------------- |
| `avg`           | Durchschnittliche Helligkeit des Symbolbereichs    |
| `threshold_on`  | Wert ab dem das Symbol als **AN** erkannt wird     |
| `threshold_off` | Wert unter dem das Symbol als **AUS** erkannt wird |
| `state`         | erkannter Zustand                                  |

### Interpretation

```
avg = 100
threshold_on = 80
```

→ Das Symbol liegt **klar über der Einschalt-Schwelle** und wird daher als **aktiv** erkannt.

---

# Debug – 7-Segment Anzeige

Die Isttemperatur besteht aus zwei Ziffern.

```
56
```

```
d0 = 5
d1 = 6
```

---

# Segmentstatus

Beispiel Digit `5`

```json
"a": 1,
"b": 0,
"c": 1,
"d": 1,
"e": 0,
"f": 1,
"g": 1
```

| Wert | Bedeutung         |
| ---- | ----------------- |
| `1`  | Segment ist aktiv |
| `0`  | Segment ist aus   |

Segmentmuster:


```
 -- a --
|       |
f       b
|       |
 -- g --
|       |
e       c
|       |
 -- d --
```
---

# Helligkeitswerte der Segmente

Beispiel:

```json
"a_avg": 255
"b_avg": 66
"e_avg": 55
```

| Bereich  | Bedeutung     |
| -------- | ------------- |
| ~240-255 | Segment aktiv |
| ~0-100   | Segment aus   |

Schwellwert:

```
threshold = 105
```

Interpretation:

```
a_avg = 255 > 105 → Segment AN
b_avg = 66 < 105 → Segment AUS
```

---

# Digit-Bitmaske

```json
bits: 109
```

Das ist eine **binäre Segmentrepräsentation**, die intern zur schnellen Ziffernidentifikation verwendet wird.

---

# Debugging nutzen

Die Debugdaten helfen bei folgenden Problemen:

## 1. Schlechte Beleuchtung

Symptom:

```
ON Segmente < 150
```

→ Kamera dunkler oder ROI falsch.

---

## 2. Schwellwert falsch

Symptom:

```
OFF Segmente nahe threshold
```

Beispiel:

```
g_avg = 102
threshold = 105
```

→ Risiko für Fehlinterpretation.

Lösung:

```
threshold leicht senken
```

---

## 3. Falsche Segmenterkennung

Symptom:

```
geometry_ok = false
```

→ Digit-Bereich wurde falsch erkannt.

Mögliche Ursachen:

* Kamera verschoben
* Display nicht exakt im ROI
* Perspektivfehler

---

# Typische stabile Werte

Empfehlung für zuverlässige Erkennung:

```
ON  Segmente ≈ 240-255
OFF Segmente ≈ 0-100
threshold ≈ 90-110
```

Großer Abstand zwischen ON und OFF sorgt für robuste Erkennung.

---

# Zusammenfassung

Das System erkennt:

```
Heizen        = aktiv
Isttemperatur = 56°C
```

Die Debugdaten zeigen:

* klare Helligkeitsunterschiede
* stabile Segmenterkennung
* keine Geometrieprobleme

Damit arbeitet die Anzeigeerkennung zuverlässig.


---

## 🔧 Debug Level

Der Debug-Level kann im **Debug** Bereich des Webinterfaces eingestellt werden:

| Level | Beschreibung |
|-------|-------------|
| 0 | Nur Fehler (Errors) |
| 1 | Fehler + Informationen (Info) |
| 2 | Fehler + Informationen + Debug-Meldungen (Debug) |

**Tipp:** Level 2 für detailliertes Debugging verwenden. Die Ausgabe kann im seriellen Monitor beobachtet werden.

---

## 🔍 Serielle Konsole

Für erweiterte Debugging verwende einen **seriellen Monitor**:

```bash
picocom /dev/ttyUSB0 -b 115200
# oder
screen /dev/ttyUSB0 115200
```

**Wichtige Log-Ausgaben:**

* `Camera start failed` - Kamerafehler beim Boot
* `Config loaded from /config.json` - Konfiguration erfolgreich geladen
* `MQTT connected` - MQTT-Verbindung erfolgreich
* `median symbol roi=...` - Ergebnis der ROI-Analyse
* `Camera restart` - Kamera-Neustart (bei wiederholten Ausfällen)

---

## 🔄 Neustart

Das Gerät kann über die **Debug** Sektion neu gestartet werden:

* Klick auf **"Reboot Device"**
* Das Gerät startet neu
* Die Webseite lädt sich automatisch neu

---

## 💡 Häufige Probleme

### Kamera wird nicht erkannt
- PSRAM auf dem Board aktiviert?
- Kabel-Verbindung überprüfen
- Debug-Output in der seriellen Konsole prüfen

### ROI-Erkennung funktioniert nicht
- Lichtverhältnisse optimieren
- ROI-Größe und Position überprüfen
- **Test Detection** verwenden zum Testen
- `Invert Detection Logic` aktivieren, wenn nötig

### MQTT verbindet nicht
- Konfiguration überprüfen (Host, Port, Benutzername)
- Firewall-Regeln prüfen
- MQTT-Server erreichbar?

---

## 📝 Tipps zum Debugging

1. **Snapshot-Funktion nutzen** - "Capture" für aktuelles Bild
2. **Test Detection verwenden** - vor Speichern der Config
3. **Debug-Level erhöhen** - für mehr Informationen
4. **Serielle Ausgabe checken** - wertvolle Fehlerhinweise
5. **Zeitstempel beobachten** - Verzögerungen erkennen

---

[⬆️ Nach oben](#test-und-fehlersuche)

> 📷 Alle Screenshots entstanden im Zusammenspiel mit einer **Buderus WPT260.4 AS** (Logatherm Warmwasser-Wärmepumpe).
