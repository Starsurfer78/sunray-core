# Hardware Map

## Scope

This map covers hardware and hardware-adjacent software bindings that are supported by repository evidence from:

- `hal/SerialRobotDriver/*`
- `platform/*`
- `core/Config.cpp`
- `core/Robot.cpp`
- `alfred/firmware/rm18.ino`

Every entry is classified as `FACT`, `INFERENCE`, or `UNKNOWN`.

## Confidence Rules

- `FACT`: directly supported by active code or active firmware in this repo
- `INFERENCE`: strongly suggested by code structure or naming, but not electrically proven
- `UNKNOWN`: not provable from current repo evidence

## Confirmed Runtime Boundary

- `FACT`: Raspberry Pi runs `sunray-core`, owns Linux device access, opens `/dev/ttyS0` for the MCU link, opens `/dev/i2c-1` for local peripherals, and derives charger state from STM32 telemetry.
- `FACT`: STM32 firmware in [rm18.ino](/mnt/LappiDaten/Projekte/sunray-core/alfred/firmware/rm18.ino) owns direct motor PWM, brake, odometry ISR, stop-button ISR, ADC sampling, and local watchdog reload.
- `FACT`: Pi to STM32 communication uses UART AT-style frames:
  - `AT+M` at 50 Hz for motor command plus fast response data
  - `AT+S` at 2 Hz for summary telemetry
  - `AT+V` for firmware identity
- `FACT`: There is active Pi-side I2C support for a TCA9548A mux and PCA9555-style port expanders.

## Pi-Side Bindings

| Binding | Evidence | Classification |
| --- | --- | --- |
| Primary MCU link on `/dev/ttyS0` | `core/Config.cpp`, `SerialRobotDriver::init()` | `FACT` |
| UART baud default `19200` | `core/Config.cpp` | `FACT` |
| Linux I2C bus `/dev/i2c-1` | `core/Config.cpp`, `platform/I2C.cpp`, `SerialRobotDriver::init()` | `FACT` |
| I2C mux address `0x70` | `core/Config.cpp`, `SerialRobotDriver::init()`, `platform/I2cMux.cpp` | `FACT` |
| I2C mux channels `0`, `4`, `5`, `6` used by runtime | `core/Config.cpp`, `SerialRobotDriver::init()` | `FACT` |
| EX1 port expander at `0x21` | `SerialRobotDriver::init()` | `FACT` |
| EX2 port expander at `0x20` | `SerialRobotDriver::init()` | `FACT` |
| EX3 port expander at `0x22` | `SerialRobotDriver::init()` | `FACT` |
| IMU primary I2C address `0x69`, fallback `0x68` | `core/Config.cpp`, `SerialRobotDriver::init()` | `FACT` |
| GPS receiver on separate Pi serial path | `core/Config.cpp`, startup path in `main.cpp` | `FACT` |
| Port expander family is PCA9555-compatible | `platform/PortExpander.cpp` implementation and `SerialRobotDriver` comments | `INFERENCE` |

## Port Expander Evidence

| Device | Address | Proven function | Evidence | Classification |
| --- | --- | --- | --- | --- |
| EX1 | `0x21` | IMU power | `SerialRobotDriver::init()` sets EX1 `IO1.6` high | `FACT` |
| EX1 | `0x21` | Fan power | `SerialRobotDriver::setFanPower()` drives EX1 `IO1.7` | `FACT` |
| EX1 | `0x21` | ADC mux or related board-side routing | inline comment `IMU/fan/ADC mux` only | `INFERENCE` |
| EX2 | `0x20` | Buzzer | `SerialRobotDriver::setBuzzer()` drives EX2 `IO1.1` | `FACT` |
| EX3 | `0x22` | Panel LEDs | `SerialRobotDriver::writeLed()` maps LED pins on EX3 port 0 | `FACT` |

## LED and Buzzer Bindings

| Signal | Binding | Evidence | Classification |
| --- | --- | --- | --- |
| `LED_1` green | EX3 port 0 pin 0 | `SerialRobotDriver::writeLed()` | `FACT` |
| `LED_1` red | EX3 port 0 pin 1 | `SerialRobotDriver::writeLed()` | `FACT` |
| `LED_2` green | EX3 port 0 pin 2 | `SerialRobotDriver::writeLed()` | `FACT` |
| `LED_2` red | EX3 port 0 pin 3 | `SerialRobotDriver::writeLed()` | `FACT` |
| `LED_3` green | EX3 port 0 pin 4 | `SerialRobotDriver::writeLed()` | `FACT` |
| `LED_3` red | EX3 port 0 pin 5 | `SerialRobotDriver::writeLed()` | `FACT` |
| Buzzer | EX2 port 1 pin 1 | `SerialRobotDriver::setBuzzer()` | `FACT` |
| LED_1 semantic = WiFi | `HardwareInterface.h`, `SerialRobotDriver.h`, `updateWifiLed()` | `FACT` |
| LED_2 semantic = error or idle | `HardwareInterface.h` | `FACT` |
| LED_3 semantic = GPS | `HardwareInterface.h` | `FACT` |

## UART Bindings

### Pi <-> STM32 runtime UART

| Signal | Binding | Evidence | Classification |
| --- | --- | --- | --- |
| Pi runtime UART | `/dev/ttyS0` | `core/Config.cpp`, `SerialRobotDriver::init()` | `FACT` |
| Frame `AT+M` | motor command and fast sensor return | `SerialRobotDriver::run()`, `parseMotorFrame()`, `rm18.ino::cmdMotor()` | `FACT` |
| Frame `AT+S` | summary telemetry | `SerialRobotDriver::run()`, `parseSummaryFrame()`, `rm18.ino::cmdSummary()` | `FACT` |
| Frame `AT+V` | firmware name and version | `SerialRobotDriver::run()`, `parseVersionFrame()`, `rm18.ino::cmdVersion()` | `FACT` |
| CRC suffix `,0xNN` | both sides compute and verify additive CRC | `SerialRobotDriver::calcCrc()/verifyCrc()`, `rm18.ino::cmdAnswer()/processCmd()` | `FACT` |

### STM32-defined UART candidates

| Signal | STM32 pin | Evidence | Classification |
| --- | --- | --- | --- |
| Soft UART RX at display panel | `PC9` as `pinKeyArea3` | `rm18.ino` defines `SoftwareSerial mSerial(pinKeyArea3, pinKeyArea1)` | `FACT` |
| Soft UART TX at display panel | `PA9` as `pinKeyArea1` | same | `FACT` |
| Alternate hardware UART 1 RX | `PA10` | defined but not active for main console in current firmware | `FACT` |
| Alternate hardware UART 1 TX | `PA9` | defined but not active for main console in current firmware | `FACT` |
| Alternate UART 3 RX for perimeter MCU comm | `PC11` | commented-out candidate in firmware | `FACT` for definition, `UNKNOWN` for current deployment use |
| Alternate UART 3 TX for perimeter MCU comm | `PC10` | commented-out candidate in firmware | `FACT` for definition, `UNKNOWN` for current deployment use |
| Secondary Bluetooth UART | `PD6/PD5` | `HardwareSerial mSerial2(pinBluetoothRX, pinBluetoothTX)` in firmware | `FACT` for firmware definition, `UNKNOWN` for deployed use with Pi runtime |

## I2C Bindings

| Signal | Binding | Evidence | Classification |
| --- | --- | --- | --- |
| Pi main bus | `/dev/i2c-1` | `core/Config.cpp`, `platform/I2C.cpp` | `FACT` |
| TCA9548A mux | address `0x70` | config + `platform/I2cMux.cpp` | `FACT` |
| IMU channel | mux channel `4` | config + `SerialRobotDriver::init()` | `FACT` |
| EEPROM channel | mux channel `5` | config + `SerialRobotDriver::init()` | `FACT` |
| ADC channel | mux channel `6` | config + `SerialRobotDriver::init()` | `FACT` |
| EX3 LED channel | mux channel `0` | config + `SerialRobotDriver::writeLed()` | `FACT` |
| STM32 local I2C pins | `PB8` = `pinSCL`, `PB9` = `pinSDA` | `rm18.ino` defines them | `FACT` for firmware pin definition |
| STM32 IMU enable pin | `PB7` = `pinMpuEnable` | `rm18.ino` defines it, but current Pi runtime powers IMU through EX1 instead | `FACT` for firmware definition, `UNKNOWN` for active electrical role in current build |

## SPI Bindings

| Signal | Binding | Evidence | Classification |
| --- | --- | --- | --- |
| STM32 LoRa SPI SCK | `PB3` | `rm18.ino` defines `pinLoraSCK` | `FACT` for firmware pin definition |
| STM32 LoRa SPI MISO | `PB4` | `rm18.ino` defines `pinLoraMISO` | `FACT` for firmware pin definition |
| STM32 LoRa SPI MOSI | `PB5` | `rm18.ino` defines `pinLoraMOSI` | `FACT` for firmware pin definition |
| STM32 LoRa reset | `PC12` | `rm18.ino` defines `pinLoraReset` | `FACT` for firmware pin definition |
| STM32 LoRa busy | `PE1` | `rm18.ino` defines `pinLoraBusy` | `FACT` for firmware pin definition |
| LoRa path actively used by current firmware loop | only commented test toggles seen, no active protocol path found | `UNKNOWN` |
| Pi-side SPI involvement in runtime | no active runtime code found in `main.cpp`, `core/`, `hal/`, `platform/` | `UNKNOWN` |

## CAN Bindings

| Signal | Evidence | Classification |
| --- | --- | --- |
| Any Pi-side CAN runtime path | no active CAN code found in current repo scan | `UNKNOWN` |
| Any STM32 firmware CAN peripheral use in `rm18.ino` | no CAN pin or driver evidence found in firmware | `UNKNOWN` |

## Motor and Motion-Related Signals

### STM32 motor pins from firmware

| Signal | STM32 pin | Evidence | Classification |
| --- | --- | --- | --- |
| Left traction PWM | `PE13` | `rm18.ino` pin definitions | `FACT` |
| Left traction brake | `PD8` | `rm18.ino` pin definitions | `FACT` |
| Left traction direction | `PD9` | `rm18.ino` pin definitions | `FACT` |
| Left traction encoder | `PD1` | `rm18.ino` ISR attachment | `FACT` |
| Left traction current sense | `PC0` | `rm18.ino` analog read path | `FACT` |
| Right traction PWM | `PE9` | `rm18.ino` pin definitions | `FACT` |
| Right traction brake | `PB12` | `rm18.ino` pin definitions | `FACT` |
| Right traction direction | `PB13` | `rm18.ino` pin definitions | `FACT` |
| Right traction encoder | `PD0` | `rm18.ino` ISR attachment | `FACT` |
| Right traction current sense | `PC1` | `rm18.ino` analog read path | `FACT` |
| Mow PWM | `PE11` | `rm18.ino` pin definitions | `FACT` |
| Mow brake | `PB15` | `rm18.ino` pin definitions | `FACT` |
| Mow direction | `PB14` | `rm18.ino` pin definitions | `FACT` |
| Mow encoder | `PD3` | `rm18.ino` ISR attachment | `FACT` |
| Mow current sense | `PC2` | `rm18.ino` analog read path | `FACT` |
| Mow fault input | `PE10` | `rm18.ino` digital read path | `FACT` |

### Motor enable and brake

| Signal | Evidence | Classification |
| --- | --- | --- |
| Pi sends traction and mow PWM setpoints through `AT+M` | `SerialRobotDriver::run()`, `rm18.ino::cmdMotor()` | `FACT` |
| STM32 applies traction brake outputs when both traction setpoints are zero | `rm18.ino::motor()` | `FACT` |
| STM32 uses `pinRelay` for mow brake pulsing and off/on sequencing | `rm18.ino::mower()` | `FACT` |
| A dedicated Pi Linux GPIO motor-enable line exists | no active Pi GPIO driver or binding found | `UNKNOWN` |
| Candidate `GPIO17` as motor enable | appears only as prior note, not in active code | `UNKNOWN` |

## Emergency Stop

| Signal | Evidence | Classification |
| --- | --- | --- |
| Emergency stop input on STM32 `PD2` | `pinStopButton`, `INPUT_PULLUP`, interrupt attached in `setup()` | `FACT` |
| Companion line `PD7` driven low as `pinStopButtonB` | firmware config in `setup()` | `FACT` |
| Stop topology = two normally-closed switches in series, high level means stop | explicit firmware comment in `setup()` | `FACT` |
| Stop button state returned to Pi in `AT+M` field 7 | `rm18.ino::cmdMotor()`, `SerialRobotDriver::parseMotorFrame()` | `FACT` |
| Pi safety path stops motors on stop-related sensor conditions | `core/Robot.cpp` handles sensor-based stops and operator stop hold behavior | `FACT` |
| Physical stop path bypasses software and cuts motor power directly | not proven from current repo | `UNKNOWN` |

## Dock Detect and Charge Path

| Signal | Evidence | Classification |
| --- | --- | --- |
| Dock detect on Pi is derived from charge voltage threshold | `charger_connected_voltage_v`, `chargerConnectedFromVoltage()` | `FACT` |
| Threshold default is `7.0 V` | `core/Config.cpp` | `FACT` |
| STM32 samples charger voltage on `PA7` as `pinChargeV` | `rm18.ino::readSensorsHighFrequency()` and definitions | `FACT` |
| Pi receives charge voltage in `AT+M` field 4 and `AT+S` field 2 | `parseMotorFrame()`, `parseSummaryFrame()` | `FACT` |
| Pi exposes `chargerConnected` into docking and charge ops | `core/Robot.cpp`, `core/op/DockOp.cpp`, `core/op/ChargeOp.cpp`, `core/op/UndockOp.cpp` | `FACT` |
| Separate physical dock switch input exists | no explicit dock switch found in current repo | `UNKNOWN` |

## Battery Telemetry

| Signal | STM32 source | Pi mapping | Classification |
| --- | --- | --- | --- |
| Battery voltage | `PC5` as `pinBatteryV`, low-pass filtered in firmware | `AT+S` field 1 -> `battery_.voltage` | `FACT` |
| Charge voltage | `PA7` as `pinChargeV` | `AT+M` field 4 and `AT+S` field 2 -> `battery_.chargeVoltage` | `FACT` |
| Charge current | `PC4` as `pinChargeI` | `AT+S` field 3 -> `battery_.chargeCurrent` | `FACT` |
| Battery temperature | `PA6` as `pinBatteryT` | `AT+S` field 11 -> `battery_.batteryTemp` | `FACT` |
| Over-voltage check | `PE14` as `pinOVCheck` | `AT+S` field 12 raises `sensors_.motorFault` | `FACT` |
| Battery RX signal on `PB6` | pin defined only, no active use proven in current firmware excerpt | `UNKNOWN` |

## Sensor Inputs

| Signal | STM32 source | Pi mapping | Classification |
| --- | --- | --- | --- |
| Left bumper | `PA4` as `pinBumperX` | `AT+M` field 9, `AT+S` field 13 | `FACT` |
| Right bumper | `PA5` as `pinBumperY` | `AT+M` field 10, `AT+S` field 14 | `FACT` |
| Combined bumper legacy bit | firmware combined `bumper` | `AT+M` field 5, `AT+S` field 5 | `FACT` |
| Lift left analog | `PA2` as `pinLift2` | contributes to `lift` bit | `FACT` |
| Lift right analog | `PA3` as `pinLift1` | contributes to `lift` bit | `FACT` |
| Lift bit | low-pass filtered lift logic in firmware | `AT+M` field 6, `AT+S` field 4 | `FACT` |
| Rain | `PA1` as `pinRain` | `AT+S` field 6 | `FACT` |
| Motor fault | firmware combines mow fault, overload, permanent fault, and over-voltage | `AT+M` field 8, `AT+S` field 7 and 12 | `FACT` |

## Watchdog Paths

| Path | Evidence | Classification |
| --- | --- | --- |
| STM32 local watchdog active | `IWatchdog.begin(2000000)` and `IWatchdog.reload()` in `loop()` | `FACT` |
| Developer-triggered watchdog hang path exists | `AT+Y` -> `cmdTriggerWatchdog()` infinite loop | `FACT` |
| Pi runtime comms watchdog for MCU silence | `SerialRobotDriver::run()` marks MCU disconnected if TX occurs with no RX | `FACT` |
| Pi runtime op watchdog | `core/Robot.cpp` checks `watchdogTimeoutMs()` on active op | `FACT` |
| External system supervisor restarts Pi process | not proven in current repo evidence | `UNKNOWN` |

## Safe-Key, Power, and Relay Lines

| Signal | STM32 pin | Evidence | Classification |
| --- | --- | --- | --- |
| `pinKeySafe` | `PD10` | defined and pulsed in `testPins()` only | `FACT` for firmware definition, `UNKNOWN` for runtime role |
| `pinRelay` | `PD11` | actively used in mower brake sequence | `FACT` |
| `pinPower` | `PD12` | `power(bool)` helper writes it | `FACT` for firmware helper, `UNKNOWN` for deployed use in current loop |
| `pinCheckPower` | `PD13` | configured as input only in setup | `FACT` for firmware definition, `UNKNOWN` for current role |

## Linux vs STM32 Responsibilities

### FACT

- Pi owns:
  - process runtime
  - mission and operation logic
  - GPS integration
  - MQTT and WebSocket
  - Linux UART and I2C device access
  - charger-connected interpretation
  - I2C mux and port-expander control
- STM32 owns:
  - direct motor PWM and brake outputs
  - odometry interrupts
  - stop-button interrupt
  - ADC sensor sampling
  - local watchdog feeding
  - immediate frame responses over UART

### INFERENCE

- Some board-local protection likely happens on the STM32 side before the Pi can react, especially around motion outputs.

### UNKNOWN

- Exact electrical cut path for emergency stop
- Whether any separate safety relay or FET stage exists outside the shown firmware pins

## High-Risk Hardware-Adjacent Files

- [hal/SerialRobotDriver/SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp)
- [hal/SerialRobotDriver/SerialRobotDriver.h](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.h)
- [platform/PortExpander.cpp](/mnt/LappiDaten/Projekte/sunray-core/platform/PortExpander.cpp)
- [platform/I2C.cpp](/mnt/LappiDaten/Projekte/sunray-core/platform/I2C.cpp)
- [platform/I2cMux.cpp](/mnt/LappiDaten/Projekte/sunray-core/platform/I2cMux.cpp)
- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [alfred/firmware/rm18.ino](/mnt/LappiDaten/Projekte/sunray-core/alfred/firmware/rm18.ino)

## Residual Unknowns

- Exact physical Pi header pin usage for deployed Alfred units
- Whether Bluetooth and LoRa features are populated or active on production boards
- Whether the commented perimeter-MCU UART path is still wired on target hardware
- Whether any CAN-capable hardware exists on other board revisions
