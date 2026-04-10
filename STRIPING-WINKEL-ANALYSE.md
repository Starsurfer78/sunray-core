# ⚠️ STRIPING-WINKEL ANALYSE — ERGEBNISSE AUS CODE-REVIEW

> **Basierend auf:** Code-Analyse von `MowRoutePlanner.cpp` & `BoustrophedonPlanner.cpp` (April 2026)

---

## 🔴 KRITISCHE BEFUNDE

### 1️⃣ WINKEL WIRD NICHT PERSISTIERT ❌

**Problem:**

```
Der Striping-Winkel wird NICHT in der Map-Datei (map.json) gespeichert!
```

**Aktueller Datenfluss:**

```
Mission-JSON (von außen)
    ↓
applyMissionOverrides()
    ↓
ZoneSettings.angle = 45.0
    ↓
buildStripeSegments(zone, perimeter, ..., angle=45.0)
    ↓
Stripen werden erzeugt
    ↓
ZonePlanSettingsSnapshot speichert den Winkel (nur für diese Route!)
    ↓
❌ Nirgends wird der Winkel in Flash/Config persistiert!
```

**Konsequenz:**

```
Tag 1: Mission mit angle=45° starten
       → Stripen werden mit 45° erzeugt
       → Snapshot speichert angle=45°

Tag 2: Neue Mission mit Mission-JSON
       → Mission-JSON hat angle=0° (Standard)
       → Stripen werden wieder mit 0° erzeugt
       → Der Winkel wurde NICHT beibehalten!
```

---

### 2️⃣ KEINE AUTOMATISCHE WINKEL-ROTATION ❌

**Problem:**

```cpp
// Es gibt keinen Code wie:
auto rotate_angle_for_today = [](float yesterday_angle) {
    return (yesterday_angle + 45) % 360;  // ← NICHT VORHANDEN
};
```

**Wo der Code suchen würde (und NICHT findet):**

- ❌ `MowRoutePlanner.cpp` — keine Funktion `calculateOptimalAngle()`
- ❌ `Map.cpp` — kein `rotateStripeAngleDaily()`
- ❌ `core/config/*` — kein `AngleScheduler` oder `RotationPolicy`
- ❌ `core/api/*` — kein Endpoint für `PUT /stripe_angle`

**Einziger Ort, wo der Winkel "gespeichert" ist:**

```cpp
// MowRoutePlanner.cpp, Zeile 1247
struct ZonePlanSettingsSnapshot {
    float stripWidth_m = 0.0f;
    float angle_deg = 0.0f;      // ← Nur im Memory, nicht persistiert!
    bool edgeMowing = false;
    // ...
};
```

**Problem:** Der Snapshot wird nur in der aktuellen Route gespeichert, nicht in Flash!

---

### 3️⃣ WINKEL KOMMT NUR AUS MISSION-JSON ⚠️

**Einzige Quelle für den Winkel:**

```cpp
// MowRoutePlanner.cpp, Zeile 839
static void applyMissionOverrides(
    ZoneSettings &settings,
    const nlohmann::json &overrides)
{
    if (overrides.contains("angle")) {
        settings.angle = overrides.value("angle", 0.0f);  // ← NUR QUELLE
    }
    if (overrides.contains("stripWidth")) {
        settings.stripWidth = overrides.value("stripWidth", 0.18f);
    }
    // ...
}
```

**Beispiel Mission-JSON:**

```json
{
  "zoneIds": ["z1"],
  "overrides": {
    "z1": {
      "angle": 45.0,
      "stripWidth": 0.25
    }
  }
}
```

**Wenn dieser JSON **nicht** den `angle` enthält:**

```json
{
  "zoneIds": ["z1"],
  "overrides": {
    "z1": {
      "stripWidth": 0.25
      // ← angle fehlt!
    }
  }
}
```

**Dann wird der Default-Wert verwendet:**

```cpp
ZoneSettings default_settings;
default_settings.angle = 0.0f;  // ← DEFAULT!
```

---

### 4️⃣ KEINE WINKEL-KONFIGURATION IN MAP.JSON ❌

**Die Map-Datei speichert:**

```json
{
  "name": "Garten",
  "perimeter": { ... },
  "zones": [
    {
      "name": "Zone 1",
      "polygon": [ [0,0], [10,0], [10,10], [0,10] ]
      // ← KEIN "angle" Feld!
    }
  ],
  "exclusions": [ ... ]
}
```

**Code-Beweis:**

```cpp
// MowRoutePlanner.cpp, Zeile 797
bool applyZoneJson(Zone &zone, const nlohmann::json &zoneJson) {
    zone.name = zoneJson.value("name", "");
    zone.polygon = polygonFromJson(zoneJson["polygon"]);
    // ← Kein angle-Feld wird gelesen!
    // stripWidth ist auch nicht hier
    // Alles kommt vom Mission-Overrides!
}
```

---

## ✅ WAS DER CODE RICHTIG MACHT

### Rotations-Mathematik ✅

```cpp
// MowRoutePlanner.cpp, Zeile 54-61
static PolygonPoints rotatePolygon(const PolygonPoints &poly, float angleRad) {
    const float cosA = std::cos(angleRad);
    const float sinA = std::sin(angleRad);
    for (const auto &p : poly)
        out.push_back(rotatePoint(p, cosA, sinA));
}

static Point rotatePoint(const Point &p, float cosA, float sinA) {
    return {
        p.x * cosA - p.y * sinA,   // ← Korrekte Matrix
        p.x * sinA + p.y * cosA    // ← x' = x·cos(θ) − y·sin(θ)
    };                               // y' = x·sin(θ) + y·cos(θ)
}
```

**Ablauf:**

1. Rotiere Polygon um `-angle` ins "gerade" Koordinatensystem
2. Erzeuge horizontale Scanlines (y = const)
3. Rotiere Ausgabepunkte um `+angle` zurück
4. Alle Punkte sind mathematisch exakt in Welt-Koordinaten

### Parallele Stripen ✅

```cpp
// MowRoutePlanner.cpp, Zeile 628
for (float y = minY + stripWidth * 0.5f;
     y <= maxY + stripWidth * 0.01f;
     y += stripWidth, ++rowIdx)
{
    // Alle Bahnen sind im rotierten System horizontal
    // Im Welt-Koordinatensystem sind sie parallel mit dem gegebenen Winkel
}
```

### Verschiedene Winkel per Mission ✅

```cpp
// Wird aufgerufen mit:
buildMissionMowRoutePreview(map,
    nlohmann::json{
        {"zoneIds", {"z1"}},
        {"overrides", nlohmann::json{
            {"z1", nlohmann::json{
                {"angle", 45.0}  // ← FUNKTIONIERT!
            }}
        }}
    }
);
```

**Das bedeutet:** Du **kannst** pro Mission einen anderen Winkel setzen — aber du musst es manuell im JSON tun!

---

## 🛠️ WAS FEHLT — FEATURE GAPS

### Gap 1: Session-Persistierung ❌

```
Schritt 1: Benutzer startet Mission mit angle=45°
Schritt 2: Nach 2h Stromausfall
Schritt 3: Neustart — Winkel wird auf 0° (Default) zurückgesetzt!

Fehlendes Feature:
- "Letzte verwendete Winkel speichern"
- "Winkel-Rotation nach jedem Session speichern"
```

### Gap 2: Automatische Rotation ❌

```
Fehlendes Feature:
function get_stripe_angle_for_today() {
    if (is_new_session()) {
        last_angle = load_from_flash();
        new_angle = (last_angle + 45) % 360;
        save_to_flash(new_angle);
        return new_angle;
    }
    return load_from_flash();
}
```

### Gap 3: Externe Kontrolle (WebUI / MQTT) ❌

```
Fehlendes Feature:
POST /api/stripe_angle?angle=45
{
    if (is_valid_angle(angle)) {
        settings.stripe_angle = angle;
        save_to_flash(angle);  // ← Persistiere!
        trigger_mission_replan();
    }
}
```

### Gap 4: Winkel in Map-Konfiguration ❌

```json
// So sollte es sein:
{
  "zones": [
    {
      "name": "Zone 1",
      "polygon": [...],
      "default_stripe_angle": 45.0  // ← FEHLT!
    }
  ]
}
```

---

## 📊 VERGLEICH: Aktuell vs. Ideal

| Feature                                | Aktuell                    | Ideal                   | Gap          |
| -------------------------------------- | -------------------------- | ----------------------- | ------------ |
| Verschiedene Winkel pro Mission        | ✅ Ja (via JSON overrides) | ✅ Ja                   | ✅ Erfüllt   |
| Automatische Winkel-Rotation           | ❌ Nein                    | ✅ Ja (+45° pro Tag)    | 🔴 FEHLT     |
| Winkel-Persistierung zwischen Sessions | ❌ Nein                    | ✅ Ja (Flash)           | 🔴 FEHLT     |
| WebUI/MQTT Control                     | ❌ Nein                    | ✅ Ja                   | 🔴 FEHLT     |
| Optimale Winkel-Berechnung             | ❌ Nein                    | ✅ Ja (längste Stripen) | 🔴 FEHLT     |
| Winkel in Map-Datei                    | ❌ Nein                    | ✅ Optional             | 🔴 FEHLT     |
| Snapshot der verwendeten Winkel        | ✅ Ja (Memory only)        | ✅ Ja (persistiert)     | ⚠️ Teilweise |

---

## 🔧 WIE MAN ES BEHEBEN KÖNNTE

### Lösung 1: Config-Datei für Striping-Winkel

**Datei:** `config/stripe_rotation.json`

```json
{
  "current_angle_deg": 45.0,
  "rotation_step_deg": 45.0,
  "last_session_timestamp": 1712500000,
  "rotation_enabled": true,
  "angle_history": [
    { "timestamp": 1712400000, "angle_deg": 0.0 },
    { "timestamp": 1712500000, "angle_deg": 45.0 }
  ]
}
```

**Code (pseudo):**

```cpp
class StripeAngleManager {
private:
    std::string CONFIG_FILE = "/data/stripe_rotation.json";

public:
    float get_angle_for_today() {
        auto config = load_json(CONFIG_FILE);

        if (is_new_session(config["last_session_timestamp"])) {
            float new_angle = rotate_angle(
                config["current_angle_deg"],
                config["rotation_step_deg"]
            );
            config["current_angle_deg"] = new_angle;
            config["last_session_timestamp"] = now();
            save_json(CONFIG_FILE, config);
            return new_angle;
        }

        return config["current_angle_deg"];
    }
};
```

### Lösung 2: API-Endpoint für Winkel-Kontrolle

```cpp
// In der WebUI-API:
router.post("/api/stripe_angle", [](Request req, Response res) {
    float angle = req.json()["angle"];

    if (angle < 0 || angle >= 360) {
        res.status(400).json({"error": "Invalid angle"});
        return;
    }

    angle_manager.set_angle(angle);  // ← Speichert in Flash
    trigger_mission_replan();         // ← Plant Route neu

    res.json({"angle": angle, "status": "ok"});
});
```

### Lösung 3: MQTT Topic für Winkel

```cpp
// MQTT Listener:
mqtt.subscribe("mower/config/stripe_angle", [](const std::string &payload) {
    float angle = std::stof(payload);  // "45.0"
    angle_manager.set_angle(angle);
    trigger_mission_replan();
});

// Publisher:
void publish_angle() {
    mqtt.publish("mower/state/stripe_angle",
                 std::to_string(angle_manager.get_angle()),
                 QoS::Level_2);
}
```

---

## 📋 EMPFOHLENE MASSNAHMEN

### Für dich (als Nutzer):

1. **Frage deinen Entwickler:**

   ```
   "Wird der Striping-Winkel zwischen Sessions persistiert?
    Aktuell: NEIN (nur im Mission-JSON)
    Gewünscht: JA (in Flash speichern, täglich rotieren)"
   ```

2. **Workaround bis es behoben ist:**
   - Verwende ein Scheduling-System außerhalb von sunray-core
   - z.B. Home Assistant Automation:
     ```yaml
     automation:
       - alias: "Rotate mowing stripes daily"
         trigger:
           platform: time
           at: "08:00:00"
         action:
           - service: mower.start_mission
             data:
               angle: >
                 {{ (((as_timestamp(now()) // 86400) % 8) * 45) | int }}
     ```

3. **Feature-Request für Entwickler:**
   - [ ] StripeAngleManager-Klasse erstellen
   - [ ] Flash-Persistierung für Winkel
   - [ ] MQTT Topic `mower/config/stripe_angle`
   - [ ] WebUI Slider für Winkel-Control
   - [ ] Auto-Rotation mit konfigurierbarem Step

---

## 🎯 ZUSAMMENFASSUNG

| Frage                                | Antwort                           | Status             |
| ------------------------------------ | --------------------------------- | ------------------ |
| Verschiedene Striping-Winkel?        | Ja, via Mission-JSON overrides    | ✅ Funktioniert    |
| Automatische Winkel-Optimierung?     | Nein                              | ❌ Nicht vorhanden |
| Winkel-Änderungen zwischen Sessions? | Nur wenn Mission-JSON sich ändert | ⚠️ Manuell         |
| Winkel persistiert in Flash?         | Nein                              | ❌ Nicht vorhanden |
| API-Kontrolle (WebUI/MQTT)?          | Nein                              | ❌ Nicht vorhanden |

**Fazit:** Der Code **kann** verschiedene Winkel pro Mission nutzen, aber es gibt **keine Session-Persistierung oder Automation** für täglich wechselnde Winkel.
