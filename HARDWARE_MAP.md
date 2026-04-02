# Hardware Map

## Scope

This map captures only what is supported by repo evidence and clearly labels weaker claims.

## Confidence Legend

- High: directly supported by active code and naming
- Medium: supported by code structure or comments but not fully proven
- Low: plausible but not yet verified in hardware

## Confirmed

- Raspberry Pi side runtime owns Linux serial and I2C access
- STM32-backed serial runtime exists and is the Alfred default driver path
- I2C mux and port expander support are active concepts in the runtime
- GPS receiver is expected on the Pi side and integrated into navigation and telemetry
- Three status LEDs and a buzzer are modeled in the hardware interface

## Likely

- STM32 owns low-level motor command execution and board telemetry acquisition
- Port expander lines drive LEDs and possibly other Alfred board functions
- Dock detection and charger telemetry are sourced through MCU-to-Pi data exchange

## Unknown

- Full physical pinout for every Alfred revision
- Whether CAN is physically present and active in deployed units
- Exact emergency-stop electrical topology

## Signal Table

| Signal | Side | Interface | Source | Meaning | Safety Critical | Confidence |
| --- | --- | --- | --- | --- | --- | --- |
| `driver_port` default `/dev/ttyS0` | Pi | UART/serial | `core/Config.cpp`, serial driver | Primary Pi to STM32 runtime channel | Yes | High |
| GPS receiver port | Pi | Serial/USB serial | `core/Config.cpp`, GPS driver naming | RTK GNSS input | Yes | High |
| I2C main bus `/dev/i2c-1` | Pi | I2C | `core/Config.cpp`, `platform/I2C.*` | Local Linux hardware bus | Medium | High |
| I2C mux address `0x70` | Pi | I2C | `core/Config.cpp`, `platform/I2cMux.*` | Bus fan-out for downstream devices | Medium | High |
| IMU I2C address `0x69` | Pi-side logical device | I2C | `core/Config.cpp`, IMU driver presence | Orientation sensing | Yes | Medium |
| Port expander address `0x20` | Pi-side logical device | I2C | config defaults, tests | GPIO expansion | Medium | Medium |
| LED_1 | Pi logical output via HAL | Likely port expander | `HardwareInterface.h`, serial driver comments | WiFi/status light | No | Medium |
| LED_2 | Pi logical output via HAL | Likely port expander | `HardwareInterface.h`, `Robot.cpp` | Error/idle status | Yes | Medium |
| LED_3 | Pi logical output via HAL | Likely port expander | `HardwareInterface.h`, `Robot.cpp` | GPS quality status | Yes | Medium |
| Buzzer | Pi logical output via HAL | Unknown electrical backend | `HardwareInterface.h`, `BuzzerSequencer.*` | Audible operator feedback | Medium | Medium |
| Charger connected | STM32 to Pi data | Serial payload | battery and charge logic in `Robot.cpp` | Dock contact and charging state | Yes | High |
| Battery voltage/current | STM32 to Pi data | Serial payload | runtime logic and tests | Shutdown and docking decisions | Yes | High |
| Bumpers | STM32 to Pi data | Serial payload | safety stop logic | Obstacle detection | Yes | High |
| Lift sensor | STM32 to Pi data | Serial payload | safety stop logic | Lifted robot detection | Yes | High |
| Motor fault | STM32 to Pi data | Serial payload | safety stop logic | Drive fault indication | Yes | High |
| Motor enable signal, candidate GPIO 17 | Unknown | GPIO | Requested explicit tracking in bootstrap note only | Possible enable/arm line | Yes | Low |

## Notes On Motor Enable

### FACT

- The active repo does not prove a concrete Linux GPIO number for a motor enable line
- Motor safety is modeled through `setMotorPwm(0,0,0)` and fault transitions

### INFERENCE

- There may be a dedicated enable path outside direct PWM commands

### UNKNOWN

- Whether GPIO 17 is the real enable line on production Alfred hardware

## Pi To STM32

- Primary link is serial
- Command direction: motor PWM, LEDs, buzzer, calibration/fault reset actions
- Telemetry direction: odometry, sensors, battery, likely MCU identity and firmware metadata

## UART, I2C, SPI, CAN, Port Expander

| Interface | Status |
| --- | --- |
| UART | Active and central |
| I2C | Active and central |
| SPI | No active evidence in the current repo scan |
| CAN | No active evidence in current runtime scan |
| Port Expander | Supported and likely important, but not fully documented |

## Follow-Up Questions

- Capture actual Alfred wiring from hardware inspection or commissioning notes
- Confirm if any GPIO on the Pi directly gates drive power or relays
- Validate whether port expander address `0x20` and mux channel usage are stable across units
