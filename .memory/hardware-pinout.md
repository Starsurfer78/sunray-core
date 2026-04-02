# Hardware Pinout

## Purpose

Compact evidence-backed pin and signal snapshot for Alfred Pi plus STM32 bindings.

## Proven Pi-Side Bindings

| Signal | Binding | Confidence | Evidence |
| --- | --- | --- | --- |
| MCU runtime link | `/dev/ttyS0` at `19200` | `FACT` | `core/Config.cpp`, `SerialRobotDriver::init()` |
| Main I2C bus | `/dev/i2c-1` | `FACT` | `core/Config.cpp`, `platform/I2C.cpp` |
| I2C mux | `0x70` | `FACT` | config + `platform/I2cMux.cpp` |
| EX1 | `0x21` | `FACT` | `SerialRobotDriver::init()` |
| EX2 | `0x20` | `FACT` | `SerialRobotDriver::init()` |
| EX3 | `0x22` | `FACT` | `SerialRobotDriver::init()` |
| IMU address | `0x69`, fallback `0x68` | `FACT` | config + `SerialRobotDriver::init()` |
| Buzzer | EX2 port 1 pin 1 | `FACT` | `SerialRobotDriver::setBuzzer()` |
| LED_1 | EX3 port 0 pins 0 and 1 | `FACT` | `SerialRobotDriver::writeLed()` |
| LED_2 | EX3 port 0 pins 2 and 3 | `FACT` | `SerialRobotDriver::writeLed()` |
| LED_3 | EX3 port 0 pins 4 and 5 | `FACT` | `SerialRobotDriver::writeLed()` |
| IMU power | EX1 IO1.6 | `FACT` | `SerialRobotDriver::init()` |
| Fan power | EX1 IO1.7 | `FACT` | `SerialRobotDriver::setFanPower()` |

## Proven STM32 Pins From `rm18.ino`

| Signal | Pin | Confidence | Evidence |
| --- | --- | --- | --- |
| Stop button | `PD2` | `FACT` | `pinStopButton`, ISR attached |
| Stop button companion line | `PD7` | `FACT` | `pinStopButtonB` set low |
| Safe key | `PD10` | `FACT` for existence | firmware define |
| Relay | `PD11` | `FACT` | used in mower brake sequence |
| Power | `PD12` | `FACT` for existence | `power(bool)` helper |
| Power check | `PD13` | `FACT` for existence | input configured in setup |
| Left bumper analog | `PA4` | `FACT` | `pinBumperX` |
| Right bumper analog | `PA5` | `FACT` | `pinBumperY` |
| Lift left analog | `PA2` | `FACT` | `pinLift2` |
| Lift right analog | `PA3` | `FACT` | `pinLift1` |
| Rain analog | `PA1` | `FACT` | `pinRain` |
| Battery temp | `PA6` | `FACT` | `pinBatteryT` |
| Charge voltage | `PA7` | `FACT` | `pinChargeV` |
| Charge current | `PC4` | `FACT` | `pinChargeI` |
| Battery voltage | `PC5` | `FACT` | `pinBatteryV` |
| Over-voltage check | `PE14` | `FACT` | `pinOVCheck` |
| Mow fault | `PE10` | `FACT` | `pinMotorMowFault` |

## Interfaces

- `FACT`: UART is the active Pi-to-STM32 runtime link.
- `FACT`: I2C is active on the Pi for mux, port expanders, and IMU.
- `FACT`: SPI-like LoRa pins exist in STM32 firmware definitions.
- `UNKNOWN`: active runtime use of LoRa/SPI in current deployment.
- `UNKNOWN`: any CAN binding in current repo state.
- `UNKNOWN`: any direct Pi GPIO motion-enable line.

## Known Unknowns

- physical Raspberry Pi header pinout on deployed units
- exact e-stop electrical cut path
- whether Bluetooth, LoRa, or perimeter-MCU links are populated in production
