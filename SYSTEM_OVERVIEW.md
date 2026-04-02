# System Overview

## Runtime Topology

`sunray-core` is the Raspberry Pi side runtime for Alfred. The Pi owns orchestration, mission state, navigation logic, telemetry serving, and higher-level safety policy. The STM32 appears to remain responsible for low-level motor and board-side sensor exchange, exposed to the Pi through the serial driver and hardware abstraction layer.

## Pi Responsibilities

- Process startup via `main.cpp`
- Own the `Robot` control loop in `core/Robot.cpp`
- Maintain mission state and operation transitions through `core/op/*`
- Run navigation, state estimation, line tracking, and local path replanning under `core/navigation/*`
- Serve telemetry and command endpoints through `core/WebSocketServer.cpp`
- Optionally publish telemetry and receive commands through `core/MqttClient.cpp`
- Persist history and event data through the history database components
- Perform Linux-side hardware access for serial, I2C, muxes, and port expanders via `platform/*`

## STM32 Responsibilities

### FACT

- The codebase treats the STM32 as the board-side motion and telemetry peer for Alfred serial runtime
- The serial driver parses framed responses and drives command exchange
- Battery, odometry, bumpers, lift, and motor fault information are expected to flow through this interface

### INFERENCE

- The STM32 likely remains the real-time motor and immediate board I/O controller, while the Pi issues high-level commands
- Some safety reactions may be duplicated across STM32 and Pi for layered protection

### UNKNOWN

- Exact firmware-side watchdog design
- Exact pin ownership split for all safety-critical outputs

## Pi To STM32 Communication

- Primary runtime link: serial
- Driver implementation: `hal/SerialRobotDriver/SerialRobotDriver.cpp`
- Data classes flowing upward: odometry, sensors, battery, IMU-related status
- Command classes flowing downward: motor PWM, LED, buzzer, likely reset/fault or calibration-related actions

## Runtime Modes

| Mode | Meaning | Primary Owner | Notes |
| --- | --- | --- | --- |
| `Idle` | Safe ready state | Pi `OpManager` | Non-mowing standby |
| `NavToStart` | Route to first mowing point | Pi | Blade off |
| `Mow` | Active mowing mission | Pi plus STM32 | Safety-heavy |
| `Dock` | Route to dock | Pi | Charger detection ends route |
| `Charge` | Docked charging state | Pi | Contact quality matters |
| `Undock` | Leave dock | Pi | Used when starting from charger |
| `GpsWait` | GPS degraded hold/recovery | Pi | Resume target logic matters |
| `WaitRain` | Weather-based hold | Pi | Controlled pause state |
| `EscapeReverse` / `EscapeForward` | Obstacle recovery | Pi | Local safety transitions |
| `Error` | Fault stop state | Pi | Must remain motor-safe |

## FACT / INFERENCE / UNKNOWN

### FACT

- Linux build system is CMake with C++17
- A Svelte frontend exists at `webui-svelte`
- Navigation, MQTT, WebSocket, history, and safety-oriented tests are present
- The repo contains platform probes for buzzer, LEDs, serial, and port expander behavior

### INFERENCE

- Alfred is already running field-like operations with this stack, but hardware mapping is not fully formalized in docs
- The system is intentionally moving toward stronger Pi-side verification and documentation discipline

### UNKNOWN

- Full production service layout on Alfred
- Whether there is a separate watchdog daemon outside this repo
- Exact commissioning flow for deployed units
