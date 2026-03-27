# hal/SimulationDriver

**Status:** ✅ Complete
**Files:** `hal/SimulationDriver/SimulationDriver.h`, `.cpp`
**Purpose:** Software-only HardwareInterface — no serial/I2C/hardware required (activated via `--sim`)

**Fault injection API:** `setBumperLeft/Right()`, `setLift()`, `setGpsQuality()`, `addObstacle(Polygon)`, `clearObstacles()`, `setPose()`

**Kinematics:** Differential drive unicycle model. PWM → wheel speed → v/ω → dead-reckoning pose. Ticks from arc length.

**Notes:**
- `mutex_` guards all shared state (thread-safe).
- Polygon obstacle hit → bumperLeft + bumperRight + nearObstacle = true.
- Point-in-polygon via ray casting.

**Tests:** `tests/test_simulation_driver.cpp` — 22 tests: init, kinematics, odometry, sensors, battery, obstacles, gps, thread
