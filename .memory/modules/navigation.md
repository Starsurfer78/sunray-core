# core/navigation — StateEstimator, LineTracker, Map

**Status:** ✅ Complete (A.5)

---

## StateEstimator
**Files:** `core/navigation/StateEstimator.h`, `.cpp`
**Purpose:** Odometry dead-reckoning + GPS fusion
**Config:** `ticks_per_meter` (def 120), `wheel_base_m` (def 0.285)
**Notes:** Sanity guard: >0.5 m/frame → skip. Safety clamp: >±10 km → reset. LP filter α=0.1 for groundSpeed.
**Phase 1:** odometry-only. `updateGps()` stub. EKF planned (E.1).

## LineTracker
**Files:** `core/navigation/LineTracker.h`, `.cpp`
**Purpose:** Stanley path-tracking controller
**Formula:** `angular = p*headingError + atan2(k*lateralError, 0.001+|speed|)`
**Constants:** TARGET_REACHED_TOLERANCE=0.2m, KIDNAP_TOLERANCE=3.0m, ROTATE_SPEED=29°/s
**Events:** onTargetReached, onNoFurtherWaypoints, onKidnapped(true/false)
**Config:** `stanley_k/p_normal`, `stanley_k/p_slow`, `motor_set_speed_ms`, `dock_linear_speed_ms`

## Map
**Files:** `core/navigation/Map.h`, `.cpp`
**Purpose:** Waypoint/polygon management, perimeter enforcement, obstacle tracking
**Key types:** `Point`, `PolygonPoints`, `WayType`, `Zone`, `ZoneSettings`, `ZonePattern`
**Key methods:** `load()`, `save()`, `startMowing()`, `startDocking()`, `nextPoint()`, `isInsideAllowedArea()`, `addObstacle()`, `getDockingPos()`
**Zones (C.4b):** `zones_` vector in map.json. ZoneSettings: name, stripWidth, speed, pattern (stripe/spiral). Sorted by `zone.order`.

**Tests:** `tests/test_navigation.cpp` — 14 tests covering all three modules
