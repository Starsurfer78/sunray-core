# core/op — Op State Machine

**Status:** ✅ Complete (A.4)
**Files:** `core/op/Op.h`, `Op.cpp`, `IdleOp.cpp`, `MowOp.cpp`, `DockOp.cpp`, `ChargeOp.cpp`, `EscapeReverseOp.cpp`, `GpsWaitFixOp.cpp`, `ErrorOp.cpp`

**Op names (frozen):** `"Idle"`, `"Mow"`, `"Dock"`, `"Charge"`, `"EscapeReverse"`, `"GpsWait"`, `"Error"`
**Priority levels:** PRIO_LOW=10, PRIO_NORMAL=50, PRIO_HIGH=80, PRIO_CRITICAL=100

**Key transitions:**
- IdleOp: charger connected >2s → ChargeOp
- MowOp: obstacle → EscapeReverse, GPS lost → GpsWait, GPS timeout → ErrorOp, rain/battery/timetable → DockOp
- DockOp: charger connected → ChargeOp, routing fails ×5 → ErrorOp
- ChargeOp: disconnected after 3s → IdleOp, timetable + battery OK → MowOp
- GpsWaitFixOp: GPS acquired → nextOp, >2 min → ErrorOp
- ErrorOp: no autonomous exit — operator "Idle" only
