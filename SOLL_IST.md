# SOLL / IST — Alfred Hardware & Firmware Vergleich

Vergleich: Original Sunray-Alfred-Firmware (`ALTE_DATEIEN/Sunray/`)
↕
Neue sunray-core C++17-Reimplementierung

**Status-Legende:**
- ✅ IMPLEMENTIERT — vollständig und korrekt
- ⚠️ TEILWEISE — vorhanden, aber Lücken / Abweichungen
- ❌ FEHLT — nicht implementiert
- 🔧 ANPASSEN — vorhanden, aber Wert/Logik muss korrigiert werden

---

## 1. Config-Parameter

### SOLL (Original `config_alfred.h`)

| Parameter | Wert Original |
|---|---|
| TICKS_PER_REVOLUTION | 320 |
| WHEEL_DIAMETER | 0.205 m |
| WHEEL_BASE | 0.390 m |
| GO_HOME_VOLTAGE | 25.5 V |
| BAT_CRITICAL_VOLTAGE | ~19 V |
| MOTOR_LEFT_SWAP_DIRECTION | 1 |
| MOTOR_RIGHT_SWAP_DIRECTION | 1 |
| SONAR_ENABLE | false |
| MOTOR_PID_KP/KI/KD | vorhanden |
| Stanley P/K | 1.1 / 0.1 |
| BUTTON_CONTROL | true |

### IST (sunray-core `config.example.json` + `Config.cpp`)

| Parameter | Wert sunray-core | Status |
|---|---|---|
| ticks_per_revolution | 320 | ✅ |
| wheel_diameter_m | 0.205 | ✅ |
| wheel_base_m | 0.390 | ✅ |
| battery_low_v | 25.5 | ✅ |
| battery_critical_v | 18.9 | ✅ |
| Motor-Swap | BUG-07 in SerialRobotDriver.cpp | ✅ |
| nearObstacle | immer `false` (SONAR_ENABLE=false) | ✅ |
| motor_pid_kp/ki/kd | 0.5 / 0.01 / 0.01 | ✅ |
| stanley_k/p | 0.1 / 1.1 | ✅ |
| motor_fault_current_a | 3.0 (Config.cpp Default) | ⚠️ |
| motor_overload_current_a | 1.2 (Config.cpp Default) | ⚠️ |

### Lücken

| Fehlendes Feature | Anmerkung |
|---|---|
| Motrostrom (f[8..10] im S-Frame) | Werte werden **geparst aber verworfen** — keine Pi-seitige Stromerkennung. `motor_fault_current_a` / `motor_overload_current_a` sind in Config aber nie ausgelesen. |
| `checkBattery()` Fallback-Defaults | Robot.cpp Z. 606/607 nutzt `21.0f` / `20.0f` als Fallback. Config.cpp setzt richtige Defaults, aber Robot.cpp-Fallback sollte trotzdem auf `25.5` / `18.9` angepasst werden. |
| NTRIP / RTCM | Original hatte experimentellen NTRIP-Client — fehlt in sunray-core komplett. |
| BLE / CAN / R/C | Nicht vorgesehen — bewusste Scope-Eingrenzung (Alfred Phase 1). |

---

## 2. Alfred Custom-PCB — Hardware-Einbindung

### SOLL

Raspberry Pi 4B + 3× PCA9555 I/O-Expander (I²C) + STM32F103 via UART.

| Komponente | I²C / Pin |
|---|---|
| EX1 PCA9555 (0x21) | IMU-Power IO1.6, Fan IO1.7, ADC-Mux |
| EX2 PCA9555 (0x20) | Buzzer IO1.1 |
| EX3 PCA9555 (0x22) | Panel LEDs IO0.0–IO0.5 |
| STM32 UART | AT-Frame-Protokoll |

### IST (`SerialRobotDriver.cpp`, `PortExpander.cpp`)

| Komponente | Status | Details |
|---|---|---|
| EX1 (0x21) IMU-Power | ✅ | `ex1_->setPin(1, 6, true)` beim Init |
| EX1 (0x21) Fan | ✅ | `setFanPower()` via IO1.7, auto an/aus bei 60/65°C |
| EX2 (0x20) Buzzer | ✅ | `ex2_->setPin(1, 1, on)` |
| EX3 (0x22) Panel LEDs | ✅ | `writeLed()` mit korrekter Pin-Zuordnung |
| STM32 UART AT-Frames | ✅ | `AT+M` 50 Hz, `AT+S` 2 Hz, `AT+V` einmalig |
| PCA9555 PortExpander | ✅ | Vollständiger RMW-Zugriff (readReg/writeReg) |
| ADC-Mux (EX1) | ⚠️ | Im Kommentar erwähnt, aber **nicht genutzt** — Batteriespannung kommt aus MCU-Frame |
| Robot-ID (eth0 MAC) | ✅ | `ip link show eth0` via `shellRead()` |

---

## 3. Panel LED — Vollständig funktionsfähig?

### SOLL

3 bi-color LEDs (rot/grün), sinnvolle Statusanzeige.

### IST (`SerialRobotDriver.cpp` + `Robot.cpp`)

| LED | Position | Funktion | Ansteuerung | Status |
|---|---|---|---|---|
| LED_1 | unten | WiFi-Status | grün = COMPLETED, rot = INACTIVE, aus = getrennt | ✅ |
| LED_2 | oben | Op-Status | grün = normal, rot = Error-Op | ✅ |
| LED_3 | mitte | GPS-Qualität | grün = RTK-Fix, rot = Float, aus = kein GPS | ✅ |

**Besonderheiten:**
- Startup-Test: alle 3 LEDs grün beim `init()` ✅
- Shutdown-Sequenz: alle 3 LEDs aus bei `keepPowerOn(false)` ✅
- WiFi-Update: alle 3 Sekunden via `wpa_cli` ✅
- Update-Takt LED_2/3: jede Schleife (~50 Hz) via `updateStatusLeds()` ✅

**Fazit: Panel LEDs sind vollständig funktionsfähig.**

---

## 4. IMU MPU6050 — Eingebunden?

### SOLL

MPU6050 (I²C 0x68), 6DoF, Gyroskop-Kalibrierung, Fusion im Zustandsschätzer.

### IST (`Mpu6050Driver.cpp`, `SerialRobotDriver.cpp`, `StateEstimator.cpp`)

| Funktion | Status | Details |
|---|---|---|
| WHO_AM_I Check (0x68) | ✅ | Init schlägt fehl wenn Sensor fehlt — Robot läuft trotzdem (GPS+Odo Fallback) |
| Wake-up + Clock X-Gyro | ✅ | REG_PWR_MGMT_1 = 0x01 |
| DLPF ~44 Hz | ✅ | REG_CONFIG = 0x03 |
| Gyro ±250 °/s | ✅ | REG_GYRO_CONFIG = 0x00 |
| Accel ±2 g | ✅ | REG_ACCEL_CONFIG = 0x00 |
| Update 50 Hz | ✅ | via `SerialRobotDriver::run()` |
| Complementary Filter | ✅ | Roll/Pitch: 98% Gyro + 2% Accel |
| Yaw Integration | ⚠️ | Nur Gyro-Integration — **kein Magnetometer**, Drift über Zeit |
| Gyro-Kalibrierung | ✅ | 250 Samples (~5 s), Bias-Subtraktion |
| Thread-safe `getData()` | ✅ | `std::mutex` in `Mpu6050Driver` |
| EKF-Integration | ✅ | `stateEst_.updateImu(yaw, roll, pitch)` in `Robot::tickHardware()` |
| Tilt-Erkennung | ⚠️ | `ImuData.pitch/roll` vorhanden, aber `onImuTilt()` in Ops nicht verdrahtet |

**Fazit: MPU6050 ist korrekt eingebunden. Yaw-Drift bei langen Mähsessions möglich.**

---

## 5. GPS-Empfänger / UBX-Nachrichten

### SOLL (ZED-F9P über USB)

| UBX-Nachricht | Funktion |
|---|---|
| NAV-HPPOSLLH (01/14) | Hochpräzise lat/lon |
| NAV-RELPOSNED (01/3C) | RTK-Relativposition N/E + Lösung |
| NAV-VELNED (01/12) | Geschwindigkeit + Kurs |
| RXM-RTCM (02/32) | DGPS-Alter |
| NMEA GGA | Anzeige / NTRIP-Feed |

### IST (`UbloxGpsDriver.cpp`)

| Nachricht | Status | Details |
|---|---|---|
| NAV-HPPOSLLH | ✅ | lat/lon mit HP-Korrektur (1e-9 Grad), hAccuracy |
| NAV-RELPOSNED | ✅ | relPosN/E in Meter, RTK Fixed/Float/Invalid, Timeout 5 s |
| RXM-RTCM | ✅ | `dgpsAge_ms` seit letztem RTCM-Paket |
| NMEA GGA | ✅ | gepuffert + via WebSocket broadcast (`broadcastNmea`) |
| NAV-VELNED | ⚠️ | Frame wird empfangen, **Speed/Heading auskommentiert** — `gps_speed_detection` hat keine Wirkung |
| F9P Auto-Config | ✅ | `gps_configure=true` → CFG-VALSET: 5 Hz, UBX-Nachrichten aktiviert |
| UBX-Checksumme | ✅ | Fletcher-Algorithmus, chkA/chkB |
| NTRIP-Client | ❌ | Nicht implementiert — Original hatte experimentellen NTRIP |

**EKF-Integration:**

| Aspekt | Status |
|---|---|
| GPS-Fusion in StateEstimator | ✅ `updateGps(relPosE, relPosN, isFix, isFloat)` |
| GPS-Failover (>5 s kein Fix) | ✅ `ekf_gps_failover_ms` |
| GPS-Resilience (GpsWait-Op) | ✅ `monitorGpsResilience()` in Robot.cpp |

---

## 6. Batterie-Parameter

### SOLL (Original)

| Parameter | Wert |
|---|---|
| GO_HOME_VOLTAGE | 25.5 V |
| BAT_CRITICAL | ~19 V |
| ChargeOp: Voll-Erkennung | Spannung + Ladestrom |

### IST

| Parameter | Config-Wert | Status |
|---|---|---|
| battery_low_v (→ dock) | 25.5 V | ✅ |
| battery_critical_v (→ shutdown) | 18.9 V | ✅ |
| battery_full_v | 30.0 V | ✅ |
| battery_full_current_a | -0.1 A | ✅ |
| ChargeOp Voll-Erkennung | `voltage >= CHARGE_COMPLETE_V && chargeCurrent < 0.1A` | ✅ |
| `checkBattery()` Fallback-Defaults | 21.0 / 20.0 V (Robot.cpp Z.606/607) | 🔧 |

**Aktion:** Robot.cpp Z. 606–607 anpassen:
```cpp
// IST:  config_->get<float>("battery_low_v",      21.0f)
// SOLL: config_->get<float>("battery_low_v",      25.5f)
// IST:  config_->get<float>("battery_critical_v", 20.0f)
// SOLL: config_->get<float>("battery_critical_v", 18.9f)
```
*(Gilt nur wenn config.json fehlt — aber besser korrekte Defaults)*

---

## 7. Button-Funktion

### SOLL (Original `robot.cpp` + `config_alfred.h`)

Physischer Button am Roboter, Aktionen per **Hold-Duration**:

| Sekunden gehalten | Aktion | Ton |
|---|---|---|
| 1 s | Stop → Idle | 1 Beep |
| 3 s | R/C-Modus toggle (RCMODEL_ENABLE, default aus) | 3 Beeps |
| 5 s | Dock | 5 Beeps |
| 6 s | Mähen starten | 6 Beeps |
| 9 s | System herunterfahren | 9 Beeps |
| 12 s | WPS-Netzwerk-Setup | 12 Beeps |

Feedback: **1 Beep pro Sekunde** (SND_READY) solange gehalten.

### IST (sunray-core)

| Aspekt | Status | Details |
|---|---|---|
| `stopButton` empfangen | ⚠️ | Wird aus AT+M Frame (f[7]) gelesen und in `SensorData.stopButton` gespeichert |
| Hold-Duration-Timer | ❌ | **Nicht implementiert** |
| Sekunden-Beep-Feedback | ❌ | **Nicht implementiert** |
| Button → Idle/Dock/Mow/Shutdown | ❌ | **Nicht implementiert** |
| stopButton in Safety-Logic | ❌ | `tickSafetyStop()` prüft nur bumper/lift/motorFault — **stopButton wird ignoriert** |

**Fazit: Der physische Button ist wirkungslos. Steuerung erfolgt ausschließlich über WebSocket-API.**

**Empfehlung:** Button-Hold-Logic in `Robot::tickHardware()` oder neuer Hilfsmethode implementieren:
```cpp
// Pseudocode:
if (sensors_.stopButton) {
    buttonHoldMs_ += dt;
    if (buttonHoldMs_ % 1000 < dt) hw_->setBuzzer_pulse();  // 1 Beep/s
} else if (buttonHoldMs_ > 0) {
    const int secs = buttonHoldMs_ / 1000;
    if      (secs >= 1 && secs < 5)  emergencyStop();
    else if (secs >= 5 && secs < 6)  startDocking();
    else if (secs >= 6 && secs < 9)  startMowing();
    else if (secs >= 9)              hw_->keepPowerOn(false);
    buttonHoldMs_ = 0;
}
```

---

## 8. Motor-Swap

### SOLL (Original `config_alfred.h`)

```cpp
#define MOTOR_LEFT_SWAP_DIRECTION  1   // Linksmotor dreht verkehrt herum
#define MOTOR_RIGHT_SWAP_DIRECTION 1   // Rechtsmotor dreht verkehrt herum
```

Zusätzlich: Encoder und PWM sind im AT-Frame links↔rechts vertauscht (Verdrahtungsfehler auf Alfred-PCB).

### IST (`SerialRobotDriver.cpp`)

| Aspekt | Status | Details |
|---|---|---|
| PWM-Swap im AT+M-Frame | ✅ | BUG-07: `AT+M,<rightPwm>,<leftPwm>,<mow>` (Felder vertauscht) |
| Encoder-Swap beim Lesen | ✅ | BUG-07: `f[1]→rawTicksRight_`, `f[2]→rawTicksLeft_` (Felder vertauscht) |
| Encoder Overflow-Fix | ✅ | BUG-05: `long`-Cast vor Subtraktion |
| Richtungs-Negation | ✅ | Durch Kreuz-Verdrahtung: Swap = effektive Richtungsumkehr beider Motoren |

**Fazit: Motor-Swap vollständig und korrekt implementiert.**

---

## 9. Buzzer-Funktionen

### SOLL (Original)

Strukturierte Sounds via `buzzer.sound()`:

| Sound | Pattern | Verwendung |
|---|---|---|
| SND_READY | kurze Folge | Button-Feedback (1×/Sekunde), Bereit |
| SND_ERROR | Fehler-Muster | Fehler aufgetreten |
| SND_WARNING | Warn-Muster | Warnung |
| SND_CHARGE | Lade-Muster | Ladevorgang |
| Beep/s beim Button-Halten | 1 Beep pro Sekunde | Hold-Feedback |

### IST (`HardwareInterface.h`, `SerialRobotDriver.cpp`, `Robot.cpp`)

| Aspekt | Status | Details |
|---|---|---|
| Hardware-Zugriff Buzzer | ✅ | `ex2_->setPin(1, 1, on)` — funktioniert |
| `setBuzzer(bool)` Interface | ✅ | Nur Ein/Aus, keine Frequenz/Pattern-Unterstützung |
| Kritische Batterie → Buzzer an | ✅ | `Robot::checkBattery()` → `hw_->setBuzzer(true)` |
| ErrorOp → Buzzer-Alarm | ✅ | 500 ms an / 500 ms aus Dauerschleife |
| Button-Feedback (1 Beep/s) | ❌ | Nicht implementiert |
| Startup-Beep | ❌ | Nicht implementiert |
| Sound-Patterns (SND_READY etc.) | ❌ | HardwareInterface hat nur `setBuzzer(bool)` |
| Dock-Beep / Mow-Beep | ❌ | Nicht implementiert |

**Empfehlung:** `HardwareInterface` um `buzzerBeep(int count, int onMs, int offMs)` erweitern,
damit Muster ohne Arduino-ähnliche Blocking-Delays umsetzbar sind.

---

## Gesamtübersicht

| # | Bereich | Status | Kritisch? |
|---|---|---|---|
| 1 | Config-Parameter | ⚠️ Motrostrom nicht Pi-seitig ausgewertet | Nein |
| 2 | Alfred Custom-PCB | ✅ Vollständig | — |
| 3 | Panel LEDs | ✅ Vollständig funktionsfähig | — |
| 4 | IMU MPU6050 | ✅ Eingebunden, ⚠️ Yaw-Drift möglich | Nein |
| 5 | GPS / UBX | ✅ Kern OK, ⚠️ NAV-VELNED ungenutzt | Nein |
| 6 | Batterie-Parameter | ⚠️ Fallback-Defaults in Robot.cpp zu niedrig | Nein |
| 7 | Button-Funktion | ❌ Nicht implementiert | **Ja** |
| 8 | Motor-Swap | ✅ Korrekt (BUG-07) | — |
| 9 | Buzzer-Funktionen | ⚠️ Nur Ein/Aus, keine Muster | Nein |

---

## Priorisierte Handlungsempfehlungen

1. **[KRITISCH] Button-Hold-Logic implementieren** — physischer Button ist aktuell wirkungslos
2. **[MITTEL] Buzzer-Muster** — mindestens Button-Feedback (1 Beep/s) + Startup-Beep
3. **[NIEDRIG] Robot.cpp Fallback-Defaults** anpassen (25.5 / 18.9 statt 21.0 / 20.0)
4. **[NIEDRIG] NAV-VELNED** auswerten wenn `gps_speed_detection=true`
5. **[OPTIONAL] Motrostrom-Monitoring** auf Pi-Seite (aktuell nur MCU-seitig via `motorFault`)
