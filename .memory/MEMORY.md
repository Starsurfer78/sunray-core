# Memory Index — sunray-core

Last updated: 2026-03-25

---

## Load-on-demand: Module Files

Load only the file(s) for the module(s) you will actually touch.

| Module | File | Layer |
|--------|------|-------|
| Serial port wrapper | `.memory/modules/serial.md` | platform |
| I2C bus wrapper | `.memory/modules/i2c.md` | platform |
| PCA9555 port expander | `.memory/modules/port_expander.md` | platform |
| HardwareInterface (abstract) | `.memory/modules/hardware_interface.md` | hal |
| SerialRobotDriver (Alfred/STM32) | `.memory/modules/serial_robot_driver.md` | hal |
| SimulationDriver (no hardware) | `.memory/modules/simulation_driver.md` | hal |
| GpsDriver (ZED-F9P) | `.memory/modules/gps_driver.md` | hal |
| Config (JSON runtime config) | `.memory/modules/config.md` | core |
| Robot (main class, 50 Hz loop) | `.memory/modules/robot.md` | core |
| Op State Machine | `.memory/modules/op_statemachine.md` | core |
| Navigation (StateEstimator, LineTracker, Map) | `.memory/modules/navigation.md` | core |
| WebSocketServer (Crow, telemetry) | `.memory/modules/websocket_server.md` | core |
| MqttClient (optional telemetry) | `.memory/modules/mqtt_client.md` | core |
| WebUI (Vue 3, MapEditor, useMowPath) | `.memory/modules/webui.md` | frontend |

---

## Always available

- `decisions.md` — architectural choices, load when design decisions are needed
- `CLAUDE.md` — loaded automatically by Claude Code on session start

---

## Quick Facts

- **Language:** C++17 | No Arduino | No globals | DI everywhere
- **Phase:** Phase 1 (A.9 on hold — needs Pi access) | Phase 2 starts after A.9
- **Frozen:** Telemetry JSON (15 fields), Op-names, `/ws/telemetry`, WebUI Dark Blue design
- **Full API reference:** `docs/ARCHITECTURE.md` | `docs/WEBUI_ARCHITECTURE.md`
