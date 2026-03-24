# TODO.md — sunray-core

Stand: März 2026

---

## A — sunray-core (C++ Rewrite)

✅ **A.1–A.8 abgeschlossen** (Fundament, SerialRobotDriver, Robot+DI, SimulationDriver, Op-State-Machine, Navigation, WebSocket-Server, Konfiguration, GPS-Treiber)

### A.9 Alfred Build-Test ⏸ wartet auf Pi-Zugang

- [ ] Kompilieren auf Raspberry Pi 4B
- [ ] Alfred fährt mit neuem Core identisch wie vorher
- [ ] Alle Unit Tests grün auf Pi

**Vor A.9 zwingend (P0 Blocker):**
- [ ] **STM32 Flashen über Pi:** UART-Bootloader oder SWD/JTAG von Pi aus. Dokumentieren Sie die Hardware-Verkabelung + Flashen-Prozess in `docs/ALFRED_FLASHING.md`. Ohne dies ist A.9 unmöglich!
- [ ] BUG-004 (CRC XOR vs. Summe) + `setMowMotor→setMotorPwm` in GpsWaitFixOp — siehe `docs/STATUS.md`

---

## B — Pico-Driver ⏸ startet erst nach A.9

- [ ] `hal/PicoRobotDriver/PicoRobotDriver.h + .cpp`
- [ ] PWM-Ausgabe (1–20 kHz) für BLDC-Controller
- [ ] Hall-Sensor Odometrie (GPIO-Interrupt)
- [ ] `platform/INA226.h + .cpp` — Strommessung I2C
- [ ] Pico-Firmware (separates Projekt, Pico SDK C++)

---

## C — WebUI

✅ **C.1–C.5 abgeschlossen** (Einstellungen, GeoJSON, WebUI Grundstruktur, Verlauf/Statistiken, Mähzonen, Mähbahnen-Berechnung, MQTT-Client)

### C.2b Mission Service (Phase 2)

- [ ] MissionRunner — Waypoint-Sending (AT+W Batches à 30 Punkte)
- [ ] Dynamisches Nachladen wenn Buffer leer

### C.6 Später (Phase C)

- [ ] Energie-Budget + Rückkehr-Berechnung
- [ ] Dynamisches Replanning bei Hindernissen

### C.7 On-The-Fly Obstacle Detection & Cleanup (Phase 2)

- [ ] **Map-Struktur erweitern:** `OnTheFlyObstacle { center, radius_m, detectedAt_ms, hitCount, persistent }`
- [ ] **EscapeReverseOp:** Bei Bumper-Hit `map->addObstacle(ctx.x, ctx.y)` mit Config-Radius `obstacle_diameter_m`
- [ ] **Obstacle-Metadaten:** Auto-detected-Flag + Timestamp für späteres Cleanup
- [ ] **A*-Pathfinding:** Beachtet On-The-Fly-Obstacles im nächsten Mow-Pass
- [ ] **Cleanup-Logic:** Nach 1h oder beim Übergang zu ChargeOp alle auto-detected Obstacles löschen (persistent bleiben)
- [ ] **Tests:** `tests/test_navigation.cpp` — addObstacle + A*-Umfahrung + Cleanup-Timer

### C.8 Später (Phase C)

- [ ] WebUI auf C++ WebSocket-Server umstellen

---

## Phase 3 — Differentiators (Community + Premium Features)

### P3.1 OTA Updates (Over-The-Air Firmware Updates)

**Vision:** Sicherheitsupdates + Features ohne physischen Zugriff; Mammotion/Segway parity

- [ ] **Staging Server:** `core/ota/OtaManager.h + .cpp` — Download + Verify Binary
- [ ] **Binary Signing:** Ed25519 für Sicherheit (gegen Man-in-Middle)
- [ ] **Rollback:** Bootloader mit dual-slot (Active/Backup) — bei Fehler zurück
- [ ] **Download Resume:** Bei Netzwerk-Dropout weitermachen (Ranges)
- [ ] **Update Check:** Periodic (config `ota_check_interval_hours`, default 24)
- [ ] **WebUI:** Settings → "Check for Updates" + Progress-Bar
- [ ] **Bootloader Update:** RP2040 Pico + STM32 Bootloader versionieren
- [ ] **Tests:** Simulate Corruption, Network Loss, Rollback Scenarios

**Dependencies:** A.9 (Hardware-Test), MQTT Optional (Notification)
**Aufwand:** 3 Wochen | **Phase:** Nach P2 Complete

**Tech Stack:**
- `mbedtls` für Ed25519 (FetchContent)
- Custom Bootloader mit dual-slot (STM32 + Pico)
- WebSocket Endpoint: `POST /api/ota/check`, `GET /api/ota/download`

---

### P3.2 Sensor Fusion (GPS + Odometry + Gyro + Barometer)

**Vision:** mm-Level Präzision, auch bei GPS-Loss; besser als Mammotion (nur Lidar)

- [ ] **StateEstimator Rewrite:** Kalman-Filter (anstatt einfach Dead-Reckoning)
  - [ ] Odometry (encoder) — Hauptquell
  - [ ] GPS (RTK-Fix, Float) — Korrektur
  - [ ] IMU Gyro — Heading-Stabilität (falls vorhanden)
  - [ ] Optional: Barometer → Elevation (für Hanglagen)

- [ ] **Covariance Matrix:** Q (process noise) + R (measurement noise) konfigurierbar
- [ ] **GPS-Failover:** Nahtlos zu Odometry-Only wenn GPS dropout
- [ ] **Kidnap Detection:** Wenn GPS + Odo massiv divergieren → GpsWaitFixOp
- [ ] **Sensor Diagnostics:** WebUI zeigt Fusion-Health (Innovation Gate, DOP)
- [ ] **Tests:** Synthetic GPS-Glitches, Encoder-Noise, Slow Drift

**Dependencies:** A.5 (Navigation exists), IMU Optional für Phase 3.5
**Aufwand:** 4 Wochen | **Phase:** P2.5 (nach On-The-Fly Obstacles)

**Tech Stack:**
- Eigen3 für Matrix-Math (FetchContent)
- UKF (Unscented Kalman Filter) oder EKF (Extended KF)
- New Config Keys: `kf_process_noise`, `kf_measurement_noise`, `gps_outlier_threshold_m`

---

### P3.3 Adaptive Learning (ML-basierte Mähoptimization)

**Vision:** Robot lernt seine Effizienz mit jedem Mähgang; nach 10 Sessions +25% faster

- [ ] **Session-Metrics Sampling:** Nach jeder Mäh-Session speichern:
  - Zones mit: Zeit, Strecke, Hindernis-Count, Battery-Drain-Rate
  - Bodenfeuchte (via Config-Sensor oder WebUI-Input)
  - Mähbahn-Pattern (welche Zone, welcher Winkel)

- [ ] **Mini-ML-Model:** TensorFlow Lite Model (100KB max)
  - Input: [zone_size, soil_moisture, previous_speed, obstacle_density]
  - Output: optimal_speed_factor (0.8–1.2)
  - Inference: 5ms auf Pi (kein Bottleneck)

- [ ] **Online Training:** Nach Session 5, 15, 25: Model neu trainieren (offline, Pi-local)
- [ ] **A/B Testing:** Zwei Zonen: Alt-Pattern vs. ML-optimiert, vergleichen
- [ ] **Persistenz:** Trainiertes Model in `config.json` serialisiert (base64)
- [ ] **WebUI Dashboard:** "Mowing Efficiency" Chart über Sessions

**Dependencies:** P3.2 (Sensor Fusion für bessere Metriken)
**Aufwand:** 6 Wochen (TensorFlow Setup + Model Training Infrastructure) | **Phase:** P3

**Tech Stack:**
- TensorFlow Lite C++ (FetchContent)
- NumPy für Preprocessing (Python Helper Script)
- Config Storage: Base64-encoded ONNX Model

---

### P3.4 Mobile Native App (iOS + Android)

**Vision:** Nicht nur WebUI (PWA), sondern echte Native App; Offline-Fähigkeit, Push-Notifications

**Architektur:** Native UI + Shared Networking (via WebSocket zu Core)

#### **iOS (Swift)**

- [ ] **SwiftUI App:** Dashboard, Map, Zone-Editor, Settings
- [ ] **Background Modes:** Location (für Session-Tracking) + Network
- [ ] **HealthKit Integration:** Mowing-Sessions als Bewegungs-Log (gamification?)
- [ ] **Push Notifications:** Via MQTT/FCM wenn Robot dockt/Error

#### **Android (Kotlin)**

- [ ] **Jetpack Compose:** Modern UI
- [ ] **Location Services:** Background location tracking
- [ ] **WorkManager:** Periodic task checks (battery status)
- [ ] **Firebase Cloud Messaging:** Push Notifications

#### **Shared Components**

- [ ] **Map Rendering:** Mapbox GL Native (iOS/Android)
- [ ] **WebSocket Client:** Swift/Kotlin Async + Reconnect Logic
- [ ] **Local Database:** SQLite für Cache + History (offline browsing)
- [ ] **Geofencing:** OS-native Geofence + App-Alerting

**Dependencies:** C# Core API stabil, WebSocket API frozen
**Aufwand:** 8 Wochen (2 Monate Vollzeit Mobile-Team) | **Phase:** P3, parallel zu anderen

**Differentiator:**
- Offline-Fähigkeit (PWA ist nicht offline)
- Native Performance
- Hardware-Integration (Camera, Bluetooth für future Sensors)
- App Store Visibility (vs. Website nur)

---

## Phase 3 Prioritization & Dependencies

```
Timeline:
┌─────────────────────────────────────────────────────────┐
│ Phase 2 (nach A.9): A*, On-The-Fly, Energy Budget (8 Wo)│
└──────────────┬──────────────────────────────────────────┘
               │
         ┌─────▼─────┬──────────────┬─────────────┐
         │            │              │             │
    P3.2 Sensor    P3.1 OTA      P3.3 Learning   │
    Fusion         Updates       (6 Wo, ab Wo8)  │
    (4 Wo)         (3 Wo)                        │
    (ab Wo8)       (ab Wo7)                      │
         │            │              │            │
         └────────┬───┴──────────────┴────────────┘
                  │
         P3.4 Mobile Native (8 Wo)
         (parallel ab Wo6)
```

**Start Phase 3:** Nach A.9 erfolgreich + P2-Core-Features
**Parallel:** Mobile-App-Team kann ab Wo6 starten (while P2 still in progress)
**First Release:** Nach P3.2 + P3.1 (Wo 20 = Wo 6 + 14)

---

## Technische Synergien

| Feature | Nutzt | Gibt an |
|---------|-------|---------|
| **OTA Updates** | ← API-Stabilität | → Kann P3.2/3.3 remote testen |
| **Sensor Fusion** | ← Session-Metrics (P3.3) | → Effizientere A* Replanning |
| **Adaptive Learning** | ← Sensor-Fusion-Metriken | → Mobile-App zeigt Insights |
| **Mobile App** | ← Alle APIs | → User-Feedback für Learning-Training |

---

## Success Metrics

| Feature | Benchmark | Ziel | Messung |
|---------|-----------|------|---------|
| **OTA** | Mammotion (1 Update/Monat) | ≥1 Update/Woche möglich | Update-Success-Rate >99.5% |
| **Fusion** | GPS Σ-Error 5cm | <2cm Position-RMSE | Simulated GPS-Glitch Recovery |
| **Learning** | Fixed Pattern (Mammotion) | +25% Efficiency nach 10 Sessions | Time-to-Completion Trending |
| **Mobile** | PWA (JavaScript) | Native Performance | Frame-Rate 60fps, <100ms Latency |

---

## Legende

- [x] Erledigt | [ ] Offen | ⏸ Blockiert

---

## D — Offene Fragen

- [ ] Q1: StateEstimator Fehler-Propagation bei dauerhaft ungültigem GPS (globale Flag?)
- [ ] Q3: GPS no-motion Schwellenwert 0.05m — ausreichend für RTK-Float?
- [ ] Q5: missionAPI.run() Unix-Socket wirklich non-blocking?
- [ ] Q7: Pi-Watchdog (15s) und STM32-Watchdog (6s) koordiniert? (Risiko: STM32 geht in Failsafe wenn Pi-Shutdown >6s)

---

## Legende

- [x] Erledigt | [ ] Offen | ⏸ Blockiert
