# KI-Prompt: Implementierung der fehlenden Striping-Winkel Features
## Für deinen Entwickler oder deine KI-basierte Code-Generation

---

## 🎯 AUFGABE FÜR DIE KI

**Generiere C++17 Code für die fehlenden Striping-Winkel Features in sunray-core.**

### Gegeben:
- `MowRoutePlanner.cpp` existiert bereits
- `buildStripeSegments()` funktioniert korrekt
- Rotation-Mathematik ist implementiert
- **FEHLT:** Session-Persistierung, Auto-Rotation, API-Kontrolle

### Zu erstellen:
1. **`StripeAngleManager` Klasse** — Verwaltung des Striping-Winkels
2. **Persistierungs-Layer** — Flash-Speicherung
3. **API-Endpoint** — WebUI Kontrolle
4. **MQTT-Integration** — Home Assistant Kontrolle
5. **Automatische Rotation** — Täglich +45°

---

## 📋 DETAILLIERTE REQUIREMENTS

### 1️⃣ STRIPE ANGLE MANAGER (stripe_angle_manager.h / .cpp)

**Signatur:**
```cpp
class StripeAngleManager {
public:
    // Lade aktuelle Konfiguration aus Flash
    bool load_config();
    
    // Speichere Konfiguration in Flash
    bool save_config();
    
    // Gib den Winkel für heute zurück (mit Auto-Rotation)
    float get_angle_for_today();
    
    // Setze Winkel manuell (mit Validierung + Speicherung)
    bool set_stripe_angle(float angle_deg);
    
    // Gib aktuellen Winkel
    float get_current_angle() const;
    
    // Debug: Zeige Winkel-Historie
    void print_angle_history() const;
    
private:
    struct Config {
        float current_angle_deg = 0.0f;
        float rotation_step_deg = 45.0f;  // Z.B. täglich +45°
        uint32_t last_session_timestamp = 0;
        bool rotation_enabled = true;
        std::vector<std::pair<uint32_t, float>> angle_history;  // (timestamp, angle)
    };
    
    Config config_;
    const std::string CONFIG_FILE = "/data/stripe_angle_config.json";
    
    bool is_new_session();
    float rotate_angle(float current, float step);
};
```

**Anforderungen:**

```
1. load_config()
   - Lade /data/stripe_angle_config.json
   - Falls nicht vorhanden: Erstelle mit Default-Werten
   - Return: true = OK, false = Fehler

2. save_config()
   - Speichere config_ in /data/stripe_angle_config.json
   - Format: JSON (nlohmann/json)
   - Atomar speichern (kein Corruption bei Stromausfall)

3. get_angle_for_today()
   - Prüfe: is_new_session()?
   - Falls JA: Rotiere Winkel um rotation_step_deg
             Speichere neuen Timestamp
             Speichere in Flash
             Log: "Stripe angle rotated from 0° to 45°"
   - Return: aktueller Winkel

4. set_stripe_angle(float angle)
   - Validierung: 0 <= angle < 360
   - Speichere in config_.current_angle_deg
   - Speichere neuen Timestamp
   - Schreibe in Flash
   - Return: true = OK

5. Validierung:
   - Winkel-Range: 0-359.99°
   - Falls ungültig: Error-Logging, return false
   - Kein Silent Fail!
```

---

### 2️⃣ PERSISTIERUNGS-LAYER (Flash-Speicher)

**Anforderung an KI:**
```
"Implementiere Funktionen für JSON Speicherung in Flash:

bool save_json_to_flash(const std::string &path, const nlohmann::json &data);
bool load_json_from_flash(const std::string &path, nlohmann::json &data);

Anforderungen:
- Atomic write (kein Corruption bei Stromausfall)
- Wear Leveling (Flash hat nur ~100k Schreib-Zyklen)
- Fallback auf Default-Werte, falls Datei beschädigt
- Logging für Debug
- Zielverzeichnis: /data/ (oder /mnt/data/ - abstraktionieren!)

Beispiel: Wenn stripe_angle_config.json beschädigt ist:
  → Stelle Standard-Config wieder her
  → Log: 'WARNING: Corrupted stripe_angle_config.json, restored defaults'
  → Starte Mähen trotzdem mit angle=0°"
```

---

### 3️⃣ API-ENDPOINT FÜR WEBUI

**Integration in bestehende WebUI-API:**

```cpp
// core/api/stripe_angle_handler.cpp (neue Datei)

class StripeAngleHandler {
public:
    // GET /api/stripe_angle
    void handle_get_angle(Request req, Response res) {
        float angle = angle_manager_.get_current_angle();
        res.json({
            {"angle_deg", angle},
            {"rotation_enabled", config_.rotation_enabled},
            {"rotation_step_deg", config_.rotation_step_deg},
            {"last_session_timestamp", config_.last_session_timestamp}
        });
    }
    
    // POST /api/stripe_angle
    void handle_set_angle(Request req, Response res) {
        float angle = req.json()["angle"];
        
        if (!angle_manager_.set_stripe_angle(angle)) {
            res.status(400).json({
                "error": "Invalid angle",
                "valid_range": "0-359.99"
            });
            return;
        }
        
        // Trigger Mission Replan
        trigger_mission_replan();
        
        res.json({
            "angle_deg": angle,
            "status": "ok",
            "message": "Stripe angle updated and mission replanned"
        });
    }
};
```

**Svelte WebUI Component:**
```svelte
<!-- src/components/StripeAngleControl.svelte -->
<div class="stripe-control">
  <label>Striping Angle: {angle}°</label>
  <input type="range" min="0" max="360" step="1" bind:value={angle} 
         on:change={saveAngle} />
  
  <button on:click={rotateNow}>Rotate Now (+{rotationStep}°)</button>
  
  <checkbox bind:checked={rotationEnabled}>
    Enable Auto-Rotation
  </checkbox>
</div>

<script>
  export let angle = 0;
  export let rotationStep = 45;
  export let rotationEnabled = true;
  
  async function saveAngle() {
    const res = await fetch('/api/stripe_angle', {
      method: 'POST',
      body: JSON.stringify({ angle })
    });
    const data = await res.json();
    if (res.ok) {
      console.log('Stripe angle updated:', data);
    } else {
      console.error('Error:', data.error);
    }
  }
  
  async function rotateNow() {
    angle = (angle + rotationStep) % 360;
    await saveAngle();
  }
</script>
```

---

### 4️⃣ MQTT-INTEGRATION

**Topics und Payload:**

```cpp
// core/mqtt/stripe_angle_mqtt.cpp

class StripeAngleMqtt {
private:
    const std::string TOPIC_CONFIG = "mower/config/stripe_angle";
    const std::string TOPIC_STATE = "mower/state/stripe_angle";
    const std::string TOPIC_ROTATION = "mower/config/stripe_rotation_enabled";
    
public:
    void subscribe(MqttClient &mqtt) {
        // Subscribe to config topics with QoS=2
        mqtt.subscribe(TOPIC_CONFIG, [this](const std::string &payload) {
            handle_angle_update(payload);
        }, QoS::Level_2);
        
        mqtt.subscribe(TOPIC_ROTATION, [this](const std::string &payload) {
            handle_rotation_toggle(payload);
        }, QoS::Level_2);
    }
    
    void publish_state(MqttClient &mqtt) {
        float angle = angle_manager_.get_current_angle();
        
        // Publish current angle
        mqtt.publish(TOPIC_STATE, 
                    std::to_string(angle), 
                    QoS::Level_2);
    }
    
private:
    void handle_angle_update(const std::string &payload) {
        // Payload: "45.0"
        try {
            float angle = std::stof(payload);
            if (angle_manager_.set_stripe_angle(angle)) {
                trigger_mission_replan();
                LOG_INFO("Stripe angle updated via MQTT: " << angle);
            } else {
                LOG_ERROR("Invalid angle from MQTT: " << angle);
            }
        } catch (const std::exception &e) {
            LOG_ERROR("MQTT parse error: " << e.what());
        }
    }
    
    void handle_rotation_toggle(const std::string &payload) {
        // Payload: "true" oder "false"
        bool enabled = (payload == "true" || payload == "1");
        // TODO: Speichere in config_
    }
};
```

**Home Assistant Automation Beispiel:**
```yaml
# configuration.yaml
automation:
  - alias: "Rotate mowing stripes daily"
    trigger:
      platform: time
      at: "08:00:00"
    action:
      - service: mqtt.publish
        data:
          topic: "mower/config/stripe_angle"
          payload: >
            {{ ((((as_timestamp(now()) // 86400) | int) % 8) * 45) | string }}
          qos: 2

  - alias: "Display current stripe angle in HA"
    trigger:
      platform: mqtt
      topic: "mower/state/stripe_angle"
    action:
      - service: input_number.set_value
        data:
          entity_id: "input_number.mower_stripe_angle"
          value: "{{ trigger.payload }}"
```

---

### 5️⃣ AUTOMATISCHE ROTATION (täglich)

**Implementierung in Main Loop:**

```cpp
// core/main.cpp oder core/mower.cpp

class Mower {
private:
    StripeAngleManager angle_manager_;
    
public:
    void start_mowing_session() {
        // Vor jeder Mäh-Session:
        float angle = angle_manager_.get_angle_for_today();
        
        // Erstelle Mission mit aktuellem Winkel
        nlohmann::json mission = create_mission_json();
        mission["overrides"]["z1"]["angle"] = angle;  // ← Nutze Auto-Rotation Winkel
        
        // Starte Mission
        route_planner_.build_route(map_, mission);
        
        LOG_INFO("Starting mowing with stripe angle: " << angle << "°");
    }
};
```

---

## 🧪 TEST-SZENARIEN FÜR KI

Gib der KI folgende Test-Cases zur Validierung:

```cpp
// tests/test_stripe_angle_manager.cpp

TEST(StripeAngleManager, LoadSaveConfig) {
    StripeAngleManager mgr;
    mgr.set_stripe_angle(45.0f);
    ASSERT_TRUE(mgr.save_config());
    
    StripeAngleManager mgr2;
    ASSERT_TRUE(mgr2.load_config());
    ASSERT_EQ(mgr2.get_current_angle(), 45.0f);
}

TEST(StripeAngleManager, AutoRotation) {
    StripeAngleManager mgr;
    float angle1 = mgr.get_angle_for_today();
    ASSERT_EQ(angle1, 0.0f);  // Erster Tag
    
    // Simuliere neuer Tag
    mgr.set_current_timestamp(mgr.get_timestamp() + 86400);
    float angle2 = mgr.get_angle_for_today();
    ASSERT_EQ(angle2, 45.0f);  // Nächster Tag: +45°
}

TEST(StripeAngleManager, InvalidAngle) {
    StripeAngleManager mgr;
    ASSERT_FALSE(mgr.set_stripe_angle(-1.0f));
    ASSERT_FALSE(mgr.set_stripe_angle(400.0f));
    ASSERT_TRUE(mgr.set_stripe_angle(0.0f));
    ASSERT_TRUE(mgr.set_stripe_angle(359.99f));
}

TEST(StripeAngleManager, RotationWrapsAround) {
    StripeAngleManager mgr;
    mgr.set_stripe_angle(315.0f);
    
    // Simuliere neuer Tag
    mgr.set_current_timestamp(mgr.get_timestamp() + 86400);
    float angle = mgr.get_angle_for_today();
    ASSERT_EQ(angle, 0.0f);  // (315 + 45) % 360 = 0
}
```

---

## 📝 INTEGRATION IN BESTEHENDE CODEBASIS

**Wo die neuen Files eingebunden werden:**

```
core/
├── navigation/
│   ├── MowRoutePlanner.cpp          (bestehend — keine Änderungen)
│   ├── BoustrophedonPlanner.cpp     (bestehend — keine Änderungen)
│   └── stripe_angle_manager.cpp     (NEU)  ← hier
│
├── api/
│   ├── api_handlers.cpp             (bestehend)
│   └── stripe_angle_handler.cpp     (NEU)  ← hier
│
├── mqtt/
│   └── stripe_angle_mqtt.cpp        (NEU)  ← hier
│
├── storage/
│   └── json_flash_storage.cpp       (NEU)  ← hier
│
└── mower.cpp                        (ändern: rufe angle_manager_.get_angle_for_today())
```

**In `MowRoutePlanner.cpp` — KEINE Änderungen nötig!** Der Code funktioniert bereits:
```cpp
buildMissionMowRoutePreview(map, mission);  // mission["overrides"]["z1"]["angle"] kommt von StripeAngleManager
```

---

## ✅ AKZEPTANZ-KRITERIEN FÜR KI

Die Implementierung ist komplett, wenn:

- [ ] `StripeAngleManager` speichert/lädt Winkel aus Flash
- [ ] Auto-Rotation funktioniert: täglich +45° bis 360°
- [ ] API-Endpoint POST /api/stripe_angle funktioniert
- [ ] Svelte WebUI hat Slider für Winkel 0-360°
- [ ] MQTT Topic `mower/config/stripe_angle` funktioniert mit QoS=2
- [ ] Home Assistant kann Winkel setzen und lesen
- [ ] Tests für alle Test-Cases durchlaufen
- [ ] Logging für alle Änderungen vorhanden
- [ ] Error-Handling für ungültige Werte
- [ ] Dokumentation für API-Endpoints

---

## 🎯 ÜBERGABE AN KI

**Beispiel-Prompt für deine KI:**

```
"Implementiere die fehlenden Striping-Winkel Features in sunray-core:

1. StripeAngleManager Klasse
   - load/save config aus Flash
   - Auto-Rotation täglich +45°
   - Set-Methode mit Validierung

2. Persistierungs-Layer
   - JSON Speicherung in /data/stripe_angle_config.json
   - Atomic writes, Wear Leveling
   - Fallback auf Default-Werte

3. API-Endpoint
   - GET /api/stripe_angle
   - POST /api/stripe_angle
   - Trigger mission replan bei Änderung

4. MQTT-Integration
   - Topic: mower/config/stripe_angle (QoS=2)
   - Topic: mower/state/stripe_angle (QoS=2)
   - Home Assistant kompatibel

5. Svelte WebUI Component
   - Slider 0-360°
   - Rotation Enable/Disable Toggle
   - Real-time Updates

Code-Stil: C++17, nlohmann::json, std::optional, Modern C++
Tests: Gtest für alle Funktionen

Basis: MowRoutePlanner.cpp funktioniert bereits, keine Änderungen nötig."
```

---

**Fertig! Gib diesen Prompt deiner KI/deinem Entwickler. 🚀**
