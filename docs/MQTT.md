# MQTT

Last updated: 2026-03-31

## Übersicht

Sunray-Core bietet optionale MQTT-Integration für Robot-Telemetrie und Fernsteuerung. Der MQTT-Client publiziert Robot-Zustand und GPS-Daten in Echtzeit und empfängt Steuerbefehle über MQTT-Topics.

## Voraussetzungen

- MQTT-Broker (z.B. Mosquitto)
- `libmosquitto-dev` Paket für den Build
- Aktiviert über `mqtt_enabled: true` in der Konfiguration

## Konfiguration

```json
{
  "mqtt_enabled": true,
  "mqtt_host": "localhost",
  "mqtt_port": 1883,
  "mqtt_keepalive_s": 60,
  "mqtt_topic_prefix": "sunray",
  "mqtt_user": "",
  "mqtt_pass": ""
}
```

### Parameter

- `mqtt_enabled`: Aktiviert/deaktiviert MQTT (Standard: false)
- `mqtt_host`: Broker-Hostname oder IP
- `mqtt_port`: Broker-Port (Standard: 1883)
- `mqtt_keepalive_s`: Keepalive-Intervall in Sekunden
- `mqtt_topic_prefix`: Basis-Topic-Präfix (z.B. "sunray")
- `mqtt_user`: Optionaler MQTT-Benutzername
- `mqtt_pass`: Optionales MQTT-Passwort

## Publish Topics

Der Client publiziert alle 100ms (10 Hz) unter dem konfigurierten Präfix:

### `{prefix}/state`
Vollständige Telemetrie als JSON (gleiche Daten wie WebSocket):

```json
{
  "op": "Mow",
  "x": 1.23,
  "y": 4.56,
  "heading": 90.0,
  "battery_v": 24.5,
  "charge_v": 28.0,
  "gps_sol": "RTK Fix",
  "gps_text": "RTK Fix",
  "gps_acc": 0.02,
  "gps_lat": 52.123456,
  "gps_lon": 13.123456,
  "bumper_l": false,
  "bumper_r": false,
  "lift": false,
  "motor_err": false,
  "uptime_s": 3600,
  "mcu_v": 3.3,
  "pi_v": 5.0,
  "imu_h": 90.0,
  "imu_r": 0.0,
  "imu_p": 0.0,
  "diag_active": false,
  "diag_ticks": 0,
  "ekf_health": 1.0,
  "ts_ms": 1640995200000,
  "state_since_ms": 1000,
  "state_phase": "normal",
  "resume_target": null,
  "event_reason": "",
  "error_code": 0
}
```

### `{prefix}/op`
Aktuelle Operation als String:
- `Idle`, `Undock`, `NavToStart`, `Mow`, `Dock`, `Charge`, `WaitRain`, `GpsWait`, `EscapeReverse`, `EscapeForward`, `Error`

### `{prefix}/gps/sol`
GPS-Lösung als String:
- `No Fix`, `2D Fix`, `3D Fix`, `RTK Fix`, etc.

### `{prefix}/gps/pos`
GPS-Position als "lat,lon" (0.0,0.0 wenn GPS nicht verfügbar)

## Subscribe Topics

### `{prefix}/cmd`
Steuerbefehle als String-Payload:

- `"start"`: Startet die aktuelle Operation
- `"stop"`: Stoppt den Roboter
- `"dock"`: Fährt zur Ladestation
- `"charge"`: Aktiviert Ladung

Befehle werden thread-sicher an die Robot-State-Machine weitergeleitet.

## Implementierung

- **Klasse**: `MqttClient` in `core/MqttClient.h/.cpp`
- **Bibliothek**: libmosquitto
- **Threading**: Eigener Publish-Thread (10 Hz) + Mosquitto-Netzwerk-Thread
- **Fallback**: No-op Stubs wenn libmosquitto nicht verfügbar
- **Integration**: Optionaler Pointer in `Robot` Klasse

## Beispiel

Mit Standard-Konfiguration (`prefix = "sunray"`):

```bash
# Telemetrie abonnieren
mosquitto_sub -t "sunray/#"

# Befehl senden
mosquitto_pub -t "sunray/cmd" -m "start"
```

## Home Assistant Autodiscovery

Sunray-Core kann sich automatisch in Home Assistant über MQTT bekannt machen. Dabei werden relevante Entitäten wie Batterie-Status, GPS-Position und Steuerbefehle als HA-Entitäten registriert.

### Aktivierung

```json
{
  "mqtt_enabled": true,
  "mqtt_homeassistant_discovery": true,
  "mqtt_homeassistant_prefix": "homeassistant"
}
```

### Zusätzliche Parameter

- `mqtt_homeassistant_discovery`: Aktiviert Autodiscovery (Standard: false)
- `mqtt_homeassistant_prefix`: HA Discovery-Präfix (Standard: "homeassistant")

### Registrierte Entitäten

Nach der MQTT-Verbindung werden folgende Entitäten in Home Assistant erstellt:

#### Binary Sensor: Mowing Status
- **Topic**: `homeassistant/binary_sensor/sunray/mowing/config`
- **State Topic**: `{prefix}/op`
- **Payload**: `"Mow"` = ON, andere = OFF
- **Icon**: `mdi:robot-mower`

#### Sensor: Battery Level
- **Topic**: `homeassistant/sensor/sunray/battery/config`
- **State Topic**: `{prefix}/state`
- **Value Template**: `{{ value_json.battery_v | float / 29.4 * 100 | round(1) }}`
- **Unit**: `%`
- **Device Class**: `battery`

#### Sensor: GPS Accuracy
- **Topic**: `homeassistant/sensor/sunray/gps_accuracy/config`
- **State Topic**: `{prefix}/state`
- **Value Template**: `{{ value_json.gps_acc | float * 100 | round(2) }}`
- **Unit**: `cm`
- **Icon**: `mdi:crosshairs-gps`

#### Device Tracker: Position
- **Topic**: `homeassistant/device_tracker/sunray/position/config`
- **JSON Attributes Topic**: `{prefix}/state`
- **JSON Attributes Template**: `{"latitude": {{ value_json.gps_lat }}, "longitude": {{ value_json.gps_lon }}, "gps_accuracy": {{ value_json.gps_acc }}}`

#### Button: Start Mowing
- **Topic**: `homeassistant/button/sunray/start_mowing/config`
- **Command Topic**: `{prefix}/cmd`
- **Payload Press**: `start`
- **Icon**: `mdi:play`

#### Button: Stop
- **Topic**: `homeassistant/button/sunray/stop/config`
- **Command Topic**: `{prefix}/cmd`
- **Payload Press**: `stop`
- **Icon**: `mdi:stop`

#### Button: Return to Dock
- **Topic**: `homeassistant/button/sunray/dock/config`
- **Command Topic**: `{prefix}/cmd`
- **Payload Press**: `dock`
- **Icon**: `mdi:home-import-outline`

### Device-Information

Alle Entitäten werden unter einem gemeinsamen Device "Sunray Robot" gruppiert mit:

- **Name**: `Sunray Robot`
- **Model**: `Sunray Core`
- **Manufacturer**: `Ardumower`
- **SW Version**: Aus Build-Info
- **Identifiers**: `sunray-core-{hostname}`

### Beispiel Autodiscovery-Nachricht

```json
{
  "name": "Sunray Robot Mowing",
  "unique_id": "sunray_mowing",
  "state_topic": "sunray/op",
  "payload_on": "Mow",
  "payload_off": "Idle",
  "device": {
    "identifiers": ["sunray-core-raspberrypi"],
    "name": "Sunray Robot",
    "model": "Sunray Core",
    "manufacturer": "Ardumower",
    "sw_version": "1.0.0"
  },
  "icon": "mdi:robot-mower"
}
```

### Voraussetzungen

- Home Assistant mit MQTT-Integration
- `mqtt_homeassistant_discovery: true` in der Konfiguration
- MQTT-Broker muss von HA erreichbar sein

### Troubleshooting

- **Entitäten nicht sichtbar**: HA MQTT-Integration prüfen und `mqtt_homeassistant_prefix` bestätigen
- **Device nicht erkannt**: `mqtt_homeassistant_discovery` aktivieren und Roboter neu starten
- **State-Updates fehlen**: Topics und Templates in HA prüfen</content>
<parameter name="filePath">/mnt/LappiDaten/Projekte/sunray-core/docs/MQTT.md