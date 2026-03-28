# sunray-core Architektur

Letzte Aktualisierung: 2026-03-24 (vollständiger Code-Scan)

---

## Inhaltsverzeichnis

1. [System-Überblick](#1-system-überblick)
2. [Module](#2-module)
3. [WebSocket API](#3-websocket-api)
4. [Konfiguration](#4-konfiguration)
5. [Eingeschränkte Bereiche (Phase-2-TODOs)](#5-eingeschränkte-bereiche-phase-2-todos)
6. [Bekannte Bugs](#6-bekannte-bugs)

---

## 1. System-Überblick

### Schichtendiagramm

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Browser / WebUI                             │
│   Vue 3 + Vite + Tailwind   ws://host/ws/telemetry   /api/*        │
└───────────────────────────────────────┬─────────────────────────────┘
                                        │ WebSocket + HTTP
┌───────────────────────────────────────▼─────────────────────────────┐
│                       Crow HTTP/WebSocket Server                    │
│                    core/WebSocketServer.h/.cpp                      │
└───────────────────────────────────────┬─────────────────────────────┘
                                        │ TelemetryData / Commands
┌───────────────────────────────────────▼─────────────────────────────┐
│                           Robot (50 Hz)                             │
│   Config · Logger · OpManager · StateEstimator · Map · LineTracker  │
└───────────┬────────────────────────────────────────────────────┬────┘
            │ HardwareInterface (abstract)                        │ GpsDriver (optional)
   ┌────────▼────────┐                               ┌───────────▼───────────┐
   │ SerialRobot-    │   oder   SimulationDriver     │  UbloxGpsDriver       │
   │ Driver          │          (--sim Modus)         │  (ZED-F9P via USB)    │
   │ (Alfred/STM32)  │                               └───────────────────────┘
   └────────┬────────┘
            │ UART AT-Frames (50 Hz)
   ┌────────▼────────┐
   │   STM32 Alfred  │ Motortreiber · Encoder · Bumper · Batterie
   └─────────────────┘
```

### Kommunikationswege

```
C++ Core → WebSocket-Server:  Robot::run() → ws_->pushTelemetry()       (50 Hz)
WebSocket-Server → Browser:   JSON push {"type":"state", ...}            (10 Hz, 100 ms Intervall)
Browser → WebSocket-Server:   JSON command {"cmd":"start|stop|..."}      (bei Bedarf)
WebSocket-Server → C++ Core:  WebSocketServer::onMessage() → Robot.*()

Robot::run() → GPS-Driver:    lastGps_ = gpsDriver_->getData()           (50 Hz poll)
GPS-Driver → NMEA-Push:       ws_->broadcastNmea(line)                   (bei jedem NMEA-Frame)

SerialRobotDriver → STM32:    AT+M (50 Hz), AT+S (2 Hz), AT+V (einmalig)
STM32 → SerialRobotDriver:    CSV-Response mit CRC-Suffix *XX
```

### Namespace-Abhängigkeiten (keine Zyklen)

```
platform  ←  hal  ←  core  ←  main.cpp
```

---

## 2. Module

### platform::Serial

**Zweck:** POSIX-termios-Seriell-Port-Wrapper. Ersetzt Arduino `HardwareSerial` und `LinuxSerial.cpp`.

**Datei:** `platform/Serial.h` / `platform/Serial.cpp`

**Öffentliche API:**

| Methode | Signatur | Beschreibung |
|---------|----------|--------------|
| Konstruktor | `Serial(port, baud)` | Wirft `std::runtime_error` bei Fehler |
| `read` | `int read(uint8_t* buf, size_t maxLen)` | Nicht-blockierend; 0 = kein Datum, -1 = Fehler |
| `write` | `bool write(const uint8_t* buf, size_t len)` | Schreibt mit Retry bei EAGAIN |
| `writeStr` | `bool writeStr(const char* str)` | Null-terminierter String |
| `available` | `int available()` | Bytes im Kernel-RX-Puffer (FIONREAD) |
| `flush` | `void flush()` | Verwirft alle ungelesenen/ungesendeten Bytes (TCIOFLUSH) |
| `isOpen` | `bool isOpen() const` | true wenn fd ≥ 0 |
| `port` | `const std::string& port() const` | Gerätepfad |

**Abhängigkeiten:** Keine (nur POSIX/Linux-Kernel-API)

**Besonderheiten:** Raw 8N1, nicht-blockierend (VMIN=0, VTIME=0). Alle `c_iflag/c_lflag/c_oflag` explizit auf 0. Move-only (nicht kopierbar).

---

### platform::I2C

**Zweck:** Linux i2cdev-Bus-Wrapper. Ersetzt Arduino `Wire.h`.

**Datei:** `platform/I2C.h` / `platform/I2C.cpp`

**Öffentliche API:**

| Methode | Signatur | Beschreibung |
|---------|----------|--------------|
| Konstruktor | `I2C(const std::string& bus)` | Wirft bei Fehler |
| `write` | `bool write(uint8_t addr, const uint8_t* buf, size_t len)` | Schreibe an 7-Bit-Adresse |
| `read` | `bool read(uint8_t addr, uint8_t* buf, size_t len)` | Lese von 7-Bit-Adresse |
| `writeRead` | `bool writeRead(addr, tx, txLen, rx, rxLen)` | Atomares Write+Read via I2C_RDWR (Repeated-START) |
| `isOpen` | `bool isOpen() const` | — |
| `busPath` | `const std::string& busPath() const` | — |

**Abhängigkeiten:** Keine

**Alfred-Gerätekarte (Bus `/dev/i2c-1`):**

| Adresse | Gerät | Funktion |
|---------|-------|----------|
| `0x20` | PCA9555 EX2 | Buzzer (IO1.1) |
| `0x21` | PCA9555 EX1 | IMU-Power, Fan, ADC-Mux |
| `0x22` | PCA9555 EX3 | LED1/2/3 (grün/rot) |
| `0x50` | BL24C256A EEPROM | Persistenter Speicher |
| `0x68` | MCP3421 ADC | Batteriespannungsmessung |
| `0x70` | TCA9548A Mux | Sub-Bus-Selektor |

---

### platform::PortExpander

**Zweck:** PCA9555 16-Bit-I/O-Port-Expander-Treiber (LEDs, Buzzer, Fan, IMU-Power).

**Datei:** `platform/PortExpander.h` / `platform/PortExpander.cpp`

**Öffentliche API:**

| Methode | Beschreibung |
|---------|--------------|
| `setPin(port, pin, level)` | Ausgabe konfigurieren und setzen (Read-Modify-Write) |
| `getPin(port, pin)` | Eingang lesen |
| `setOutputPort(port, val)` | Ganzes 8-Bit-Output-Register setzen |
| `setConfigPort(port, mask)` | Richtungsregister setzen (0=Ausgang, 1=Eingang) |
| `getInputPort(port, val&)` | Ganzes 8-Bit-Input-Register lesen |
| `address()` | I2C-Adresse |

**Abhängigkeiten:** `platform::I2C`

---

### HardwareInterface

**Zweck:** Abstrakte Basisklasse — einzige Grenze zwischen Core und Hardware. Alle Treiber implementieren dieses Interface.

**Datei:** `hal/HardwareInterface.h`

**Datenstrukturen:**

| Struktur | Felder |
|----------|--------|
| `OdometryData` | `leftTicks`, `rightTicks`, `mowTicks` (int), `mcuConnected` (bool) |
| `SensorData` | `bumperLeft`, `bumperRight`, `lift`, `rain`, `stopButton`, `motorFault`, `nearObstacle` (bool) |
| `BatteryData` | `voltage`, `chargeVoltage`, `chargeCurrent`, `batteryTemp` (float), `chargerConnected` (bool) |
| `LedId` | `LED_1` (WiFi), `LED_2` (Status), `LED_3` (GPS) |
| `LedState` | `OFF`, `GREEN`, `RED` |

**Öffentliche API:**

| Methode | Beschreibung |
|---------|--------------|
| `bool init()` | Hardware öffnen, konfigurieren. `false` → Robot bricht ab. |
| `void run()` | Periodischer Tick (non-blocking, jede Kontrollloop-Iteration) |
| `void setMotorPwm(int left, int right, int mow)` | PWM-Bereich: −255…+255 |
| `void resetMotorFault()` | Latched Motor-Fault löschen |
| `OdometryData readOdometry()` | Encoder-Deltas seit letztem Aufruf |
| `SensorData readSensors()` | Momentaufnahme aller Sensor-Zustände |
| `BatteryData readBattery()` | Energie-/Lade-Momentaufnahme |
| `void setBuzzer(bool on)` | Buzzer ein/aus |
| `void setLed(LedId, LedState)` | Panel-LED setzen |
| `void keepPowerOn(bool)` | `false` → Plattform-Shutdown-Sequenz |
| `float getCpuTemperature()` | CPU-Temperatur (°C), −9999 wenn nicht verfügbar |
| `std::string getRobotId()` | Eindeutige ID (eth0-MAC auf Alfred) |
| `std::string getMcuFirmwareName()` | AT+V Firmware-Name |
| `std::string getMcuFirmwareVersion()` | AT+V Firmware-Version |

**Abhängigkeiten:** Keine (Interface-only)

---

### Config

**Zweck:** JSON-basierte Laufzeit-Konfiguration. Ersetzt Arduino `config.h`-Makros.

**Datei:** `core/Config.h` / `core/Config.cpp`

**Öffentliche API:**

| Methode | Beschreibung |
|---------|--------------|
| `Config(path)` | Lädt Datei; fällt bei fehlendem/korruptem JSON auf Defaults zurück |
| `T get<T>(key, fallback)` | Liest Wert; gibt Fallback bei fehlendem Key oder Typfehler zurück |
| `void set<T>(key, value)` | Schreibt in In-Memory-Dokument (nicht auf Disk) |
| `void save()` | Persistiert Dokument (pretty-printed, 4 Leerzeichen). Wirft bei Schreibfehler |
| `void reload()` | Verwirft ungespeicherte Änderungen, liest von Disk neu |
| `std::string dump()` | Pretty-printed JSON-String des aktuellen Dokuments |
| `const path& path()` | Pfad aus Konstruktor |

**Abhängigkeiten:** `nlohmann/json` (via FetchContent)

**Ladereihenfolge:** Eingebaute Defaults → Datei-Werte überschreiben Key für Key. Unbekannte Datei-Keys werden akzeptiert (Forward-Kompatibilität).

---

### Robot

**Zweck:** Haupt-Roboter-Klasse — DI-Root, 50-Hz-Kontrollloop, State-Machine-Orchestrator.

**Datei:** `core/Robot.h` / `core/Robot.cpp`

**Konstruktor:**
```cpp
Robot(std::unique_ptr<HardwareInterface> hw,
      std::shared_ptr<Config>            config,
      std::shared_ptr<Logger>            logger)
// Wirft std::invalid_argument wenn ein Argument nullptr ist
```

**Lifecycle:**

| Methode | Beschreibung |
|---------|--------------|
| `bool init()` | Hardware öffnen, LEDs zurücksetzen. `false` bei Hardware-Fehler. |
| `void loop()` | Kontrollloop bis `stop()`. Schläft, um 50 Hz zu halten. |
| `void run()` | Einzelne Kontrollloop-Iteration (für Tests exponiert). |
| `void stop()` | Graceful-Shutdown anfordern (thread-safe). |

**Operator-Commands:**

| Methode | Beschreibung |
|---------|--------------|
| `startMowing()` | "Mow" an OpManager dispatchen |
| `startDocking()` | "Dock" dispatchen |
| `emergencyStop()` | Motoren stoppen + "Idle" dispatchen |
| `loadMap(path)` | Map-JSON laden |
| `setPose(x, y, heading)` | StateEstimator-Pose überschreiben |
| `setWebSocketServer(ws*)` | WS-Server anhängen (optional, nicht im Konstruktor) |
| `setGpsDriver(driver*)` | GPS-Treiber anhängen (optional) |

**Kontrollloop-Sequenz (eine `run()`-Iteration):**

```
1.  hw_->run()                    — Treiber-Tick (AT-Frames, LEDs, Fan, WiFi)
2.  readOdometry/Sensors/Battery  — Sensor-Momentaufnahme
3.  stateEst_.update(odo, dt_ms)  — Odometrie-Dead-Reckoning
4.  GPS-Poll (wenn gesetzt)       — lastGps_ = gpsDriver_->getData()
5.  stateEst_.updateGps(...)      — GPS-Update (Phase-2-Stub in Phase 1)
6.  OpContext aufbauen            — alle Felder aus aktuellem Zustand befüllen
7.  checkBattery()                — Niederspannungs-Events auslösen
8.  opMgr_.tick(ctx)              — Op-State-Machine-Schritt
9.  Safety-Stop                   — bumper/lift/motorFault → setMotorPwm(0,0,0)
10. updateStatusLeds()            — LED_2 (Status), LED_3 (GPS)
11. ws_->pushTelemetry()          — WebSocket-Telemetrie (wenn WS angehängt)
12. ++controlLoops_
```

**Abhängigkeiten:** `HardwareInterface`, `Config`, `Logger`, `OpManager`, `StateEstimator`, `Map`, `LineTracker`, `WebSocketServer` (optional), `GpsDriver` (optional)

---

### OpManager + Op-State-Machine

**Zweck:** Betriebszustands-Automat — steuert alle Betriebsmodi des Roboters.

**Dateien:** `core/op/Op.h`, `core/op/Op.cpp`, `core/op/IdleOp.cpp`, `core/op/MowOp.cpp`, `core/op/DockOp.cpp`, `core/op/ChargeOp.cpp`, `core/op/EscapeReverseOp.cpp`, `core/op/GpsWaitFixOp.cpp`, `core/op/ErrorOp.cpp`

**Öffentliche API (OpManager):**

| Methode | Beschreibung |
|---------|--------------|
| `void tick(OpContext&)` | Einen State-Machine-Schritt ausführen |
| `Op* activeOp()` | Aktuell aktiver Op (kann nullptr sein — guard prüfen!) |
| `void setPending(Op*, priority, returnBack, caller)` | Transition vormerken |

**OpContext (jede Iteration an jeden Op übergeben):**

| Feld | Typ | Quelle |
|------|-----|--------|
| `hw` | `HardwareInterface&` | Robot-Konstruktor |
| `config` | `Config&` | Robot-Konstruktor |
| `sensors` | `SensorData` | `hw.readSensors()` |
| `battery` | `BatteryData` | `hw.readBattery()` |
| `odometry` | `OdometryData` | `hw.readOdometry()` |
| `x`, `y`, `heading` | `float` | StateEstimator |
| `insidePerimeter` | `bool` | `map.isInsideAllowedArea(x,y)` |
| `gpsHasFix`, `gpsHasFloat` | `bool` | StateEstimator |
| `gpsFixAge_ms` | `unsigned long` | Phase-2-TODO (hardcoded 9 999 999 in Phase 1) |
| `now_ms` | `unsigned long` | Monotone ms seit Robot-Start |
| `stateEst*`, `map*`, `lineTracker*` | Zeiger | Robot-Members |

**Prioritätsstufen:**

| Stufe | Wert | Verwendet für |
|-------|------|---------------|
| `PRIO_LOW` | 10 | — |
| `PRIO_NORMAL` | 50 | Standard `changeOp()` |
| `PRIO_HIGH` | 80 | Error, Charge; Operator-Commands Mow/Dock/Charge |
| `PRIO_CRITICAL` | 100 | Operator-Stop/Error, `onBatteryUndervoltage` |

**Zustandsübergangsdiagramm:**

```
                   ┌─────────┐
        charger    │         │
        connected  │  Idle   │◄── operator "stop" (PRIO_CRITICAL)
      ─────────────┤         │◄── battery undervoltage (PRIO_CRITICAL)
      ↓            └────┬────┘
 ┌─────────┐            │ charger connected >2s
 │  Charge │◄───────────┘
 │         │
 │         │ timetable start + battery OK / operator "Mow"
 │         ├──────────────────────────────────────────────► MowOp
 └────┬────┘
      │ charger disconnected
      ↓
 ┌─────────┐                    ┌──────────────────┐
 │  Idle   │                    │ EscapeReverseOp  │
 └─────────┘                    │ (3s rückwärts)   │
                                └────────┬─────────┘
 ┌─────────┐ obstacle / lift     ↑        │ done (returnBack)
 │  Mow    │─────────────────────┘        ↓
 │         │                    ┌──────────────────┐
 │         │ GPS lost           │ GpsWaitFixOp     │
 │         ├───────────────────►│ (max 2 min)      │
 │         │ (returnBack)       └──────┬───────────┘
 │         │                           │ GPS acquired → returnBack
 │         │ GPS timeout               │
 │         ├──────────────────────────►│
 │         │                       ErrorOp
 │         │ rain / battery low / no waypoints
 │         ├──────────────────────────────────────────────► DockOp
 └─────────┘ operator "Dock"
                                             ┌─────────┐
 ┌─────────┐ charger connected               │  Error  │
 │  Dock   │──────────────────────────────►  │         │ kein autonomer Ausgang
 │         │                          Charge  │         │ nur operator "Idle"
 │         │ obstacle → EscapeReverse (ret.)  └─────────┘
 │         │ routing fails × 5 → ErrorOp
 └─────────┘
```

**Op-Details:**

| Op | Name (frozen) | begin() | run() | Events |
|----|---------------|---------|-------|--------|
| `IdleOp` | `"Idle"` | Motoren stoppen | charger >2s → ChargeOp | — |
| `MowOp` | `"Mow"` | Mähwerk starten (PWM 200), map.startMowing() | lineTracker.track() | obstacle→EscapeRev, GPS lost→GpsWait, timeout→Error, rain/batt/no-waypoints→Dock |
| `DockOp` | `"Dock"` | map.startDocking() | lineTracker.track() | charger→Charge, obstacle→EscapeRev, fail×5→Error |
| `ChargeOp` | `"Charge"` | Motoren stoppen | disconnect nach 3s→Idle; full (≥28.5V + <0.1A für 60s)→ggf. MowOp | timetable start→MowOp |
| `EscapeReverseOp` | `"EscapeReverse"` | Bumper merken, Stopzeit now+3s | −0.1 m/s mit Lenkkorrektur | done→returnBack |
| `GpsWaitFixOp` | `"GpsWait"` | Motoren stoppen | GPS OK→returnBack; >2 min→ErrorOp | — |
| `ErrorOp` | `"Error"` | Motoren stopp, LED_2 ROT, Buzzer-Plan | Motoren stopp halten; Buzzer 500ms/5s | nur operator "Idle" |

**Abhängigkeiten:** `HardwareInterface`, `Config`, `Logger`, `StateEstimator`, `Map`, `LineTracker`

---

### StateEstimator

**Zweck:** Roboter-Positions-Schätzung aus Rad-Encoder-Ticks (Dead-Reckoning). GPS-Fusion als Phase-2-Stub vorbereitet.

**Datei:** `core/navigation/StateEstimator.h` / `core/navigation/StateEstimator.cpp`

**Öffentliche API:**

| Methode | Beschreibung |
|---------|--------------|
| `update(OdometryData, dt_ms)` | Encoder-Deltas in Pose (x, y, heading) integrieren |
| `updateGps(posE, posN, isFix, isFloat)` | GPS-Override (Phase-2-Stub, setzt Flags) |
| `x()`, `y()`, `heading()` | Aktuelle Pose (lokale Meter, Radiant) |
| `groundSpeed()` | m/s, Tiefpass-gefiltert aus Encoder-Deltas (α=0.1) |
| `gpsHasFix()`, `gpsHasFloat()` | GPS-Qualitäts-Flags |
| `setPose(x, y, heading)` | Pose direkt überschreiben |
| `reset()` | Auf Ursprung zurücksetzen (0, 0, 0) |

**Sicherheitsmechanismen:**
- Sanity-Guard: > 0.5 m pro Frame → Tick-Delta verwerfen
- Safety-Clamp: Drift > ±10 km → auf Ursprung zurücksetzen

**Abhängigkeiten:** `Config` (keys: `ticks_per_meter`, `wheel_base_m`)

---

### LineTracker

**Zweck:** Stanley-Pfad-Tracking-Controller — berechnet Lenkbefehle aus Querablage und Kursabweichung.

**Datei:** `core/navigation/LineTracker.h` / `core/navigation/LineTracker.cpp`

**Stanley-Formel:**
```
angular = p × headingError + atan2(k × lateralError, 0.001 + |groundSpeed|)
```

**Öffentliche API:**

| Methode | Beschreibung |
|---------|--------------|
| `reset()` | Rotationsstatus löschen — in Op::begin() aufrufen |
| `track(ctx, map, estimator)` | Eine Kontroll-Iteration: Lenkung berechnen, Events auslösen, Waypoints fortschalten |
| `lateralError()` | Aktuelle Querablage (m) |
| `targetDist()` | Entfernung zum aktuellen Ziel (m) |
| `angleToTargetFits()` | true wenn Kursabweichung < Schwellwert (Stanley-Phase aktiv) |

**Gefeuerte Events:**

| Event | Bedingung |
|-------|-----------|
| `onTargetReached` | Abstand < `TARGET_REACHED_TOLERANCE` (0.2 m) |
| `onNoFurtherWaypoints` | `map.nextPoint()` gab false zurück |
| `onKidnapped(true)` | Abstand von geplantem Pfad > `KIDNAP_TOLERANCE` (3.0 m) |
| `onKidnapped(false)` | Wieder innerhalb 3 m |

**Abhängigkeiten:** `Config` (keys: `stanley_k_normal`, `stanley_p_normal`, `stanley_k_slow`, `stanley_p_slow`, `motor_set_speed_ms`, `dock_linear_speed_ms`), `Map`, `StateEstimator`

---

### Map

**Zweck:** Waypoint- und Polygon-Verwaltung — Perimeter, Exclusion-Zonen, Mähbahnen, Dock-Pfad, Zonen.

**Datei:** `core/navigation/Map.h` / `core/navigation/Map.cpp`

**Datentypen:**

| Typ | Beschreibung |
|-----|--------------|
| `Point` | `{float x, float y}` + `distanceTo(Point)` |
| `WayType` | `PERIMETER`, `EXCLUSION`, `DOCK`, `MOW`, `FREE` |
| `Zone` | `{id, order, polygon, ZoneSettings{name, stripWidth, speed, pattern}}` |
| `MowPoint` | `{Point p, bool rev, bool slow}` — K-Turn-Encoding |

**Öffentliche API:**

| Methode | Beschreibung |
|---------|--------------|
| `load(path)` | JSON-Map-Datei laden (Format: Mission Service) |
| `save(path)` | Map in JSON speichern |
| `startMowing(x, y)` | Ersten Mähpunkt setzen (nächstgelegener Waypoint) |
| `startDocking(x, y)` | Pfad zum Dock setzen |
| `retryDocking(x, y)` | Retry nach fehlgeschlagenem Kontakt |
| `nextPoint(sim, x, y)` | Auf nächsten Waypoint fortschalten. `false` wenn fertig. |
| `isInsideAllowedArea(x, y)` | Innerhalb Perimeter UND außerhalb aller Exclusions |
| `addObstacle(x, y)` | Virtuelles Hindernis markieren (A*-Eingabe) |
| `getDockingPos(x, y, δ, idx)` | Dock-Anfahrts-Position und Heading |
| `mowingCompleted()` | `true` wenn alle Mähpunkte abgefahren |
| `zones()` | `const std::vector<Zone>&` — Zonen-Liste (nach `order` sortiert) |

**Öffentlicher Zustand:**

| Feld | Beschreibung |
|------|--------------|
| `targetPoint` | Aktuelles Navigationsziel |
| `lastTargetPoint` | Vorheriges Ziel (definiert Tracking-Linie) |
| `trackReverse` | `true` = rückwärts fahren |
| `trackSlow` | `true` = reduzierte Geschwindigkeit (Docking-Ansatz) |
| `wayMode` | Aktueller Navigationsmodus |
| `percentCompleted` | Mähfortschritt 0–100% |

**Abhängigkeiten:** `Config`

---

### SerialRobotDriver

**Zweck:** Alfred/STM32-Treiber — implementiert `HardwareInterface` über UART-AT-Frames.

**Datei:** `hal/SerialRobotDriver/SerialRobotDriver.h` / `hal/SerialRobotDriver/SerialRobotDriver.cpp`

**AT-Protokoll:**

| Frame | Rate | Richtung | Inhalt |
|-------|------|----------|--------|
| `AT+M` | 50 Hz | Pi ↔ STM32 | Motor-PWM-Befehl + Encoder/Sensor-Antwort |
| `AT+S` | 2 Hz | Pi ↔ STM32 | Summary: Batterie, Bumper, Regen, Ströme |
| `AT+V` | einmalig | Pi ↔ STM32 | Firmware-Name/Version-Handshake |

**Frame-Format:** CSV-Felder + `*XX` CRC-Suffix (Byte-Summe aller Bytes vor `*`).

> **⚠ BUG-004:** Dokumentation beschreibt XOR, Implementierung verwendet Addition. Vor A.9-Hardware-Test klären.

**Interne Verhaltensweisen:**
- Fan: an wenn CPU > 65°C, aus wenn < 60°C (alle ~60 s geprüft)
- WiFi-LED: `wpa_cli status` alle 7 s gepollt → LED_1
- Batterie-Fallback: bei MCU-Disconnect wird 28 V zurückgegeben
- Shutdown: `keepPowerOn(false)` → 5 s Wartezeit → Fan aus → `shutdown now`

**Angewandte Bug-Fixes:** BUG-05 (Tick-Overflow via `long`-Cast), BUG-07 (PWM/Encoder-Swap — Alfred-PCB-Verkabelung kompensiert), BUG-08 (Pi-seitiger Mähmotor-Clamp entfernt)

**Abhängigkeiten:** `platform::Serial`, `platform::I2C`, `platform::PortExpander`, `Config`

---

### SimulationDriver

**Zweck:** Software-only-Treiber — kein serielles Gerät, kein I2C, keine Hardware nötig.

**Datei:** `hal/SimulationDriver/SimulationDriver.h` / `hal/SimulationDriver/SimulationDriver.cpp`

**Kinematisches Modell:** Differentialantrieb-Unicycle. PWM → Radgeschwindigkeit (m/s) → Dead-Reckoning-Pose. Ticks aus Bogenlänge berechnet.

**Zusätzliche API (nicht in HardwareInterface):**

| Methode | Beschreibung |
|---------|--------------|
| `setBumperLeft/Right(bool)` | Bumper-Kontakt injizieren |
| `setLift(bool)` | Lift-Sensor injizieren |
| `setGpsQuality(FIX/FLOAT/NO_FIX)` | GPS-Qualität setzen |
| `addObstacle(Polygon)` | Polygon-Hindernis (Ray-Casting bei Kollision) |
| `clearObstacles()` | Alle Polygone entfernen |
| `getPose()` | Aktuelle `SimPose {x, y, heading}` |
| `setPose(SimPose)` | Pose direkt setzen |

**Thread-Safety:** Alle Shared-State-Zugriffe durch `mutex_` geschützt.

**Aktivierung:** `--sim`-Flag in `main.cpp` wählt SimulationDriver statt SerialRobotDriver.

**Abhängigkeiten:** Keine Platform-Abhängigkeit

---

### WebSocketServer

**Zweck:** Crow-basierter HTTP/WebSocket-Server — 10-Hz-Telemetrie-Push, Command-Empfang, statisches Webui-Serving.

**Datei:** `core/WebSocketServer.h` / `core/WebSocketServer.cpp`

**Pimpl:** Crow-Headers nur in `.cpp` — nicht in `.h` (verhindert Header-Pollution).

**Öffentliche API:**

| Methode | Beschreibung |
|---------|--------------|
| `WebSocketServer(config, logger)` | Konstruktor |
| `void start()` | Server-Thread starten (Crow I/O + Push-Loop) |
| `void stop()` | Server herunterfahren |
| `void pushTelemetry(TelemetryData)` | Telemetrie in Push-Queue einreihen (thread-safe via mutex) |
| `void broadcastNmea(std::string)` | NMEA-Zeile sofort an alle Clients senden |
| `void setRobot(Robot*)` | Robot-Referenz für Command-Routing |

**Threading:** Crow I/O im eigenen Thread-Pool; Push-Loop in `serverThread_`; Telemetrie-Sharing via Mutex.

**Abhängigkeiten:** `Crow` + `Asio` (via FetchContent), `Config`, `Logger`, `Robot` (via Setter)

---

### Platform-Layer (Serial, I2C, PortExpander)

Siehe je eigene Modul-Beschreibung oben. Zusammenfassung:

```
platform::Serial      → termios, POSIX, raw 8N1
platform::I2C         → Linux /dev/i2c-*, I2C_RDWR ioctl
platform::PortExpander → PCA9555 via I2C, Read-Modify-Write
```

---

## 3. WebSocket API

### Verbindung

```
URL:   ws://host:PORT/ws/telemetry
PORT:  config key "ws_port" (default: 8765)
```

### Telemetrie-Push (Server → Client, 10 Hz)

**Format (eingefroren — nicht ändern ohne Frontend-Update):**

```json
{
  "type":      "state",
  "op":        "Idle",
  "x":         1.23,
  "y":         4.56,
  "heading":   0.785,
  "battery_v": 26.4,
  "charge_v":  0.0,
  "gps_sol":   4,
  "gps_text":  "RTK-Fix 12 SV",
  "gps_lat":   51.234567,
  "gps_lon":   6.789012,
  "bumper_l":  false,
  "bumper_r":  false,
  "motor_err": false,
  "uptime_s":  3600
}
```

**Feldbeschreibung:**

| Feld | Typ | Beschreibung |
|------|-----|--------------|
| `type` | string | Immer `"state"` für Telemetrie |
| `op` | string | Aktiver Op: `"Idle"`, `"Mow"`, `"Dock"`, `"Charge"`, `"Error"`, `"GpsWait"`, `"EscapeReverse"` |
| `x` | float | Lokale Meter Ost (StateEstimator) |
| `y` | float | Lokale Meter Nord |
| `heading` | float | Radiant, 0 = Ost |
| `battery_v` | float | Batteriespannung (V) |
| `charge_v` | float | Ladespannung (V) |
| `gps_sol` | int | 0=None, 4=RTK-Fix, 5=RTK-Float |
| `gps_text` | string | Menschenlesbare GPS-Zusammenfassung |
| `gps_lat` | float | GPS-Breitengrad |
| `gps_lon` | float | GPS-Längengrad |
| `bumper_l` | bool | Linker Bumper aktiv |
| `bumper_r` | bool | Rechter Bumper aktiv |
| `motor_err` | bool | Motor-Fault aktiv |
| `uptime_s` | int | Sekunden seit Robot-Start |

**Keepalive:** `{"type":"ping"}` wenn keine neuen Telemetrie-Daten verfügbar.

### NMEA-Push (Server → Client, bei Bedarf)

```json
{"type": "nmea", "line": "$GNGGA,120000.00,5114.12345,N,...*XX"}
```

Nicht eingefroren — kann jederzeit fehlen oder ergänzt werden.

### Log-Push (Server → Client)

```json
{"type": "log", "text": "Robot::run: loop 1500 — op=Mow"}
```

### Commands (Client → Server)

```json
{"cmd": "start"}    // Mähen starten → Robot::startMowing()
{"cmd": "stop"}     // Stopp → Robot::emergencyStop()
{"cmd": "dock"}     // Docking → Robot::startDocking()
{"cmd": "charge"}   // Laden → ChargeOp
{"cmd": "setpos", "x": 1.0, "y": 2.0, "heading": 0.0}  // Pose setzen
```

### REST-Endpoints

| Methode | Pfad | Beschreibung |
|---------|------|--------------|
| `GET` | `/ws/telemetry` | WebSocket-Upgrade |
| `GET` | `/api/config` | Gesamte Config als JSON |
| `PUT` | `/api/config` | Partial-Update (nur geänderte Keys); sofortiger `config->save()` |
| `GET` | `/api/map` | Map-JSON lesen (`map.json`) |
| `POST` | `/api/map` | Map-JSON schreiben; Robot::loadMap() aufrufen |
| `GET` | `/api/map/geojson` | Map als GeoJSON exportieren |
| `POST` | `/api/map/geojson` | GeoJSON importieren (Perimeter + Exclusions) |
| `POST` | `/api/sim/bumper` | Bumper injizieren (nur `--sim`) |
| `POST` | `/api/sim/gps` | GPS-Qualität setzen (nur `--sim`) |
| `POST` | `/api/sim/lift` | Lift-Sensor injizieren (nur `--sim`) |
| `GET` | `/` | Webui `index.html` (aus `webui/dist/`) |
| `GET` | `/assets/*` | Statische Assets |

---

## 4. Konfiguration

Kopie von `config.example.json` → `/etc/sunray/config.json` für Deployment.

### Treiber & Hardware

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `driver` | string | `"serial"` | `"serial"` = Alfred/STM32, `"sim"` = Simulation |
| `driver_port` | string | `"/dev/ttyS0"` | UART-Gerätepfad |
| `driver_baud` | int | `115200` | UART-Baudrate |
| `i2c_bus` | string | `"/dev/i2c-1"` | I2C-Bus-Gerätepfad |

### GPS (ZED-F9P)

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `gps_port` | string | `/dev/serial/by-id/usb-u-blox_...` | GPS-Gerätepfad |
| `gps_baud` | int | `115200` | GPS-Baudrate |
| `gps_configure` | bool | `false` | `true`: Konfiguriert 5 Hz + UBX-Messages beim Start |
| `gps_config_filter` | bool | `true` | Aktiviert Elevationsfilter für stabiles RTK-Signal |
| `gps_wait_timeout_ms` | int | `600000` | Max. Wartezeit auf GPS-Fix beim Start (ms) |
| `gps_require_valid` | bool | `true` | Startet nicht ohne gültigen GPS-Fix |
| `gps_speed_detection` | bool | `true` | GPS-Geschwindigkeitserkennung |
| `gps_motion_detection` | bool | `true` | Bewegungserkennung via GPS |
| `gps_motion_detection_timeout_s` | int | `5` | Timeout für Bewegungslosigkeit (s) |
| `gps_no_motion_threshold_m` | float | `0.05` | Schwellwert für „keine Bewegung" (m) |

### MQTT

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `mqtt_enabled` | bool | `false` | MQTT-Client aktivieren |
| `mqtt_host` | string | `"localhost"` | Broker-Host |
| `mqtt_port` | int | `1883` | Broker-Port |
| `mqtt_keepalive_s` | int | `60` | Keepalive-Intervall |
| `mqtt_topic_prefix` | string | `"sunray"` | Topic-Präfix für alle Nachrichten |
| `mqtt_user` | string | `""` | Benutzername (leer = anonym) |
| `mqtt_pass` | string | `""` | Passwort |

### NTRIP (RTK-Korrekturen)

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `ntrip_enabled` | bool | `false` | NTRIP-Client aktivieren |
| `ntrip_host` | string | `"www.sapos-nw-ntrip.de"` | NTRIP-Caster-Host |
| `ntrip_port` | int | `2101` | NTRIP-Port |
| `ntrip_mount` | string | `"VRS_3_4G_NW"` | Mountpoint |
| `ntrip_user` | string | `"user"` | Benutzername |
| `ntrip_pass` | string | `"pass"` | Passwort |

### Odometrie & Geometrie

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `ticks_per_revolution` | int | `320` | Hall-Sensor-Ticks pro Radumdrehung |
| `wheel_diameter_m` | float | `0.205` | Raddurchmesser (m) |
| `wheel_base_m` | float | `0.390` | Radabstand Mitte–Mitte (m) |
| `robot_length_m` | float | `0.60` | Roboter-Länge (m) |
| `robot_width_m` | float | `0.43` | Roboter-Breite (m) |
| `gps_offset_x_m` | float | `0.0` | GPS-Antennen-Offset vorwärts (m) |
| `gps_offset_y_m` | float | `0.19` | GPS-Antennen-Offset links (m) |

### Motor-PID

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `motor_pid_lp` | float | `0.0` | Encoder-Tiefpassfilter (0 = deaktiviert) |
| `motor_pid_kp` | float | `0.5` | P-Anteil |
| `motor_pid_ki` | float | `0.01` | I-Anteil |
| `motor_pid_kd` | float | `0.01` | D-Anteil |

### Motor-Stromgrenzen (Fahrmotoren)

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `motor_fault_current_a` | float | `3.0` | Hardware-Fehler → Sofortstopp (A) |
| `motor_overload_current_a` | float | `1.2` | Überlast → verlangsamen/stoppen (A) |
| `motor_too_low_current_a` | float | `0.005` | Blockade-Erkennung (0 = deaktiviert) |
| `motor_overload_speed_ms` | float | `0.1` | Geschwindigkeit bei Überlast (m/s) |
| `enable_fault_detection` | bool | `true` | Hardware-Fault-Erkennung |
| `enable_overload_detection` | bool | `false` | Überlast-Erkennung |
| `enable_fault_obstacle_avoidance` | bool | `true` | Fault als Hindernis behandeln |
| `fault_max_successive_count` | int | `5` | Max. aufeinanderfolgende Faults vor Stop |

### Mähmotor-Stromgrenzen

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `mow_fault_current_a` | float | `8.0` | Mähwerk-Fault-Schwelle (A) |
| `mow_overload_current_a` | float | `2.0` | Mähwerk-Überlast-Schwelle (A) |
| `mow_too_low_current_a` | float | `0.005` | Mähwerk-Blockade-Erkennung (A) |
| `mow_toggle_dir` | bool | `true` | Drehrichtung bei jedem Start wechseln |
| `enable_mow_motor` | bool | `true` | `false` = testen ohne Messer |

### Navigation (Stanley-Regler)

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `stanley_k` | float | `0.5` | Querablage-Gain (allgemein) |
| `stanley_k_normal` | float | `0.1` | Querablage-Gain (normal) |
| `stanley_p_normal` | float | `1.1` | Heading-Gain (normal) |
| `stanley_k_slow` | float | `0.1` | Querablage-Gain (langsam/Docking) |
| `stanley_p_slow` | float | `1.1` | Heading-Gain (langsam/Docking) |
| `motor_set_speed_ms` | float | `0.3` | Vorwärtsgeschwindigkeit beim Tracking (m/s) |
| `dock_linear_speed_ms` | float | `0.1` | Vorwärtsgeschwindigkeit beim Docking (m/s) |
| `target_reached_tolerance_m` | float | `0.1` | Ziel als erreicht gelten ab Abstand (m) |
| `kidnap_detect` | bool | `true` | Entführungs-Erkennung |
| `kidnap_detect_tolerance_m` | float | `1.0` | Entführungs-Schwelle (m) |

### Batterie

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `battery_low_v` | float | `25.5` | Niedrig → Dock auslösen (V) |
| `battery_critical_v` | float | `18.9` | Kritisch → Sofortstopp (V) |
| `battery_full_v` | float | `30.0` | Voll-Spannungsschwelle (V) |
| `battery_full_current_a` | float | `-0.1` | Voll-Strom-Schwelle (A) |
| `battery_full_slope` | float | `0.002` | Voll wenn Spannungsanstieg < Schwelle |

### Temperatur

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `overheat_temp_c` | int | `85` | Überhitzung → Dock (°C) |
| `too_cold_temp_c` | int | `5` | Zu kalt → Dock (°C) |
| `use_temp_sensor` | bool | `true` | `false` wenn kein HTU21D verbaut |

### Sensoren

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `buzzer_enabled` | bool | `true` | Buzzer aktivieren |
| `port_expander_addr` | string | `"0x20"` | PCA9555-I2C-Adresse |
| `bumper_deadtime_ms` | int | `1000` | Totzeit nach Bumper-Auslösung (ms) |
| `bumper_invert` | bool | `false` | Sensor-Polarität umkehren |
| `enable_lift_detection` | bool | `true` | Hubsensor aktiv |
| `lift_obstacle_avoidance` | bool | `true` | Lift → Hindernisausweichen statt nur Motorstopp |
| `lift_invert` | bool | `false` | Lift-Sensor-Polarität umkehren |
| `enable_tilt_detection` | bool | `true` | Neigungssensor aktiv |

### Hindernisumgehung

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `obstacle_diameter_m` | float | `1.2` | Durchmesser des platzierten virtuellen Hindernisses (m) |
| `obstacle_loop_max_count` | int | `5` | Max. Hindernisse in Zeitfenster vor ErrorOp |
| `obstacle_loop_window_ms` | int | `30000` | Zeitfenster für Loop-Erkennung (ms) |

### Undocking

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `undock_speed_ms` | float | `0.08` | Rückwärtsgeschwindigkeit beim Undocking (m/s) |
| `undock_distance_m` | float | `0.32` | Rückwärts-Fahrstrecke (m) |
| `undock_charger_timeout_ms` | int | `5000` | Timeout für Lader-Disconnect (ms) |
| `undock_position_timeout_ms` | int | `10000` | Timeout für Startposition erreichen (ms) |
| `undock_heading_tolerance_rad` | float | `0.175` | Kursabweichungs-Toleranz (~10°) |
| `undock_encoder_check_ms` | int | `1000` | Encoder-Prüfintervall beim Undocking (ms) |
| `undock_encoder_min_ticks` | int | `5` | Mindest-Ticks zum Nachweis der Bewegung |
| `undock_ignore_gps_distance_m` | float | `2.0` | GPS ignorieren innerhalb Dock-Radius (m) |

### Docking

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `dock_trackslow_distance_m` | float | `5.0` | Ab diesem Abstand Langsamfahrt zum Dock (m) |
| `dock_auto_start` | bool | `true` | Nach Laden automatisch wieder mähen |
| `dock_front_side` | bool | `true` | `true` = Vorderseite zuerst einfahren |
| `dock_retry_max_attempts` | int | `3` | Max. Docking-Retry-Versuche |
| `dock_retry_approach_ms` | int | `2000` | Anlauf-Dauer beim Retry (ms) |
| `dock_retry_contact_timeout_ms` | int | `3000` | Kontakt-Timeout beim Retry (ms) |

### Preflight

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `preflight_min_voltage` | float | `26.5` | Mindestspannung vor Undock (V) |
| `preflight_gps_timeout_ms` | int | `60000` | GPS-Wartezeit vor Undock (ms) |

### Stuck-Detection

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `stuck_detect_timeout_ms` | int | `5000` | Timeout für Steckenbleiben (ms) |
| `stuck_detect_min_speed_ms` | float | `0.01` | Mindestgeschwindigkeit — darunter = stecken (m/s) |
| `stuck_recovery_max_attempts` | int | `3` | Max. Wiederherstellungsversuche |
| `stuck_recovery_reverse_ms` | int | `2000` | Rückwärtsdauer bei Recovery (ms) |
| `stuck_recovery_pause_ms` | int | `1000` | Pause-Dauer bei Recovery (ms) |
| `stuck_recovery_forward_ms` | int | `1500` | Vorwärtsdauer nach Recovery (ms) |

### Sonstiges

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `rain_delay_min` | int | `60` | Regen-Verzögerung — Wartezeit nach Regen (min) |

---

## 5. Eingeschränkte Bereiche (Phase-2-TODOs)

### GPS-Fusion (StateEstimator)

**Status:** Stub implementiert, Logik fehlt.

`StateEstimator::updateGps()` setzt in Phase 1 nur `gpsHasFix_`/`gpsHasFloat_`-Flags. Eine echte Positions-Fusion (Komplementärfilter oder Kalman) ist für Phase 2 geplant.

**Konsequenz:** Bei GPS-Loss driftet der Robot im reinen Odometrie-Modus, ohne Fehlerkorrekturen. Es gibt kein "Odometry-Only"-Flag in OpContext — MowOp kann nicht unterscheiden.

### A*-Pfadplanung (Map)

**Status:** Placeholder.

`Map` enthält vereinfachte Pfadplanung (direkter Waypoint-Ansatz, keine Hindernis-Umgehung via A*). Echte A*-Implementierung für Phase 2 geplant.

### IMU-Integration (StateEstimator)

**Status:** Vollständig entfernt für Phase 1.

`onImuTilt()` und `onImuError()` in MowOp existieren, werden aber nie aufgerufen (kein IMU-Sensor angebunden). Phase-2-Anforderung.

### `gpsFixAge_ms` in OpContext

**Status:** Hardcoded Wert (BUG-006).

`OpContext.gpsFixAge_ms` ist auf `9 999 999 ms` fest kodiert. GpsWaitFixOp verwendet diesen Wert für den 2-Minuten-Timeout → Verhalten unzuverlässig. Echte Zeitstempel-Differenz für Phase 2.

### Phase-2-Treiber: PicoRobotDriver

**Status:** Nicht begonnen.

`hal/PicoRobotDriver/` für RP2040 Pico mit direktem PWM/Hall-Zugriff. Beginnt erst nach erfolgreichem A.9 (Alfred-Hardware-Test).

### No-Go-Zonen zur Laufzeit

**Status:** Bewusst nicht implementiert (Entscheidung 2026-03-22).

No-Go-Zonen werden ausschließlich im Python-Pfadplaner aus dem Mähpfad herausgeschnitten (Shapely boolean difference). Sunray-Core prüft sie nicht zur Laufzeit.

**Risiko:** Bei Pfadabweichung (Hindernis-Umfahrung) kann der Robot in eine No-Go-Zone fahren. Akzeptiert für Phase 1.

### AT+W Waypoint-Batches

**Status:** Nicht implementiert.

Der berechnete Mähpfad kann bisher nicht über das UART-Protokoll an den Robot übertragen werden. Phase-2-Blocker für autonomes Mähen.

---

## 6. Bekannte Bugs

Vollständige Bug-Liste: siehe **[docs/BUG_REPORT.md](BUG_REPORT.md)**

### P0-Items (Blocker vor A.9-Hardware-Test)

| ID | Datei | Impact | Problem |
|----|-------|--------|---------|
| **BUG-004** | `SerialRobotDriver.cpp:246–250` | **kritisch** | CRC-Algorithmus: Dokumentation sagt XOR, Code verwendet Byte-Summe. Wenn Alfred-Firmware XOR nutzt → alle AT-Frames verworfen → Robot antwortet nicht. |
| (ANALYSIS) | `core/op/GpsWaitFixOp.cpp` | **kritisch** | `ctx.hw.setMowMotor()` aufgerufen — Methode existiert in `HardwareInterface` nicht. Build-Fehler auf Pi. |

### P1-Items (Hoch, vor erstem Feldeinsatz)

| ID | Datei | Impact | Problem |
|----|-------|--------|---------|
| **BUG-001** | `core/Robot.cpp:345,348` | hoch | Null-Pointer in `checkBattery()` — `activeOp()` kann `nullptr` sein |
| **BUG-002** | `core/Robot.cpp:342` | hoch | `setBuzzer(false)` bei kritischer Batterie — Alarm wird AUSGESCHALTET statt eingeschaltet |
| **BUG-005** | `core/WebSocketServer.cpp:603` | hoch | `broadcastNmea()` aus Robot-Thread + Push-Loop aus serverThread_ = Data Race auf WebSocket-Connection |
| **BUG-008** | `useMowPath.ts:519–531` | hoch | Bypass-Berechnung nur oben/unten → Robot fährt bei horizontalen Exclusions durch No-Go-Zone |
| **BUG-010** | `MapEditor.vue:654–671` | hoch | Doppelklick fügt spuriösen Vertex ein → jedes Polygon ist defekt |

### P2-Items (Mittel)

| ID | Datei | Impact | Problem |
|----|-------|--------|---------|
| **BUG-003** | `SerialRobotDriver.cpp:329–331` | mittel | `fieldInt()` → `int` Overflow bei langen Mähsessions |
| **BUG-006** | `core/Robot.cpp:135,228` | mittel | `gpsFixAge_ms` hardcoded 9 999 999 ms → GPS-Recovery unzuverlässig |
| **BUG-009** | `useMowPath.ts:682–684` | mittel | K-Turn: `stripLen × 2` statt `segLen × 0.4` → Overshoot bei kurzen Randstreifen |
| **BUG-012** | `MapEditor.vue:545–550` | mittel | Zone.settings.pattern (`spiral`) vollständig ignoriert — UI-Dropdown funktionslos |

### Niedrig

| ID | Datei | Problem |
|----|-------|---------|
| **BUG-007** | `WebSocketServer.cpp:481–483` | Exception-Message unsanitized im JSON-Error-Body |
| **BUG-011** | `MapEditor.vue:421–422` | `fitView()` crasht bei >10 000 Waypoints (Spread-Operator Stack-Overflow) |
