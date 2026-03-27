# core/Robot

**Status:** ✅ Complete
**Files:** `core/Robot.h`, `core/Robot.cpp`
**Purpose:** Main robot class — DI constructor, 50 Hz control loop, state machine orchestrator

**Key methods:** `init()`, `loop()`, `run()`, `stop()`, `startMowing()`, `startDocking()`, `emergencyStop()`, `loadMap()`, `setPose()`, `setWebSocketServer()`, `setGpsDriver()`, `setMqttClient()`

**Loop sequence (each tick):**
`hw->run()` → `readSensors` → `stateEst.update` → `OpContext` → `checkBattery` → `opMgr.tick` → `safety stop` → `updateLEDs` → `pushTelemetry` → `++loops`

**Notes:**
- `setWebSocketServer(ws*)` and `setGpsDriver()` are optional setters — not in constructor.
- `buildTelemetry()` private helper assembles TelemetryData from current state.

**Tests:** `tests/test_robot.cpp` — 21 tests: null-ptr guards, init, run, state transitions, battery, loop exit
