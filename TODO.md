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

## D — Offene Fragen

- [ ] Q1: StateEstimator Fehler-Propagation bei dauerhaft ungültigem GPS (globale Flag?)
- [ ] Q3: GPS no-motion Schwellenwert 0.05m — ausreichend für RTK-Float?
- [ ] Q5: missionAPI.run() Unix-Socket wirklich non-blocking?
- [ ] Q7: Pi-Watchdog (15s) und STM32-Watchdog (6s) koordiniert? (Risiko: STM32 geht in Failsafe wenn Pi-Shutdown >6s)

---

## Legende

- [x] Erledigt | [ ] Offen | ⏸ Blockiert
