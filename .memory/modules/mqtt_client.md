# core/MqttClient

**Status:** ✅ Complete (A.8)
**Files:** `core/MqttClient.h`, `core/MqttClient.cpp`
**Purpose:** Optional telemetry push + remote command reception via MQTT broker

**Public API:** `connect()`, `loop()`, `publishState()`, `onCommand(callback)`, `disconnect()`
**Topics:** Publishes `{prefix}/op`, `{prefix}/state`, `{prefix}/gps`; subscribes `{prefix}/cmd`

**Config keys:** `mqtt_enabled` (bool, default: false), `mqtt_host`, `mqtt_port`, `mqtt_keepalive_s`, `mqtt_topic_prefix`, `mqtt_user`, `mqtt_pass`

**Notes:**
- Optional — enabled via `mqtt_enabled` config key.
- Parallel to WebSocket (not a replacement).
- Background connection thread, mutex-guarded state.

**Tests:** `tests/test_mqtt_client.cpp` — 6 tests: connection lifecycle, publish, subscribe
