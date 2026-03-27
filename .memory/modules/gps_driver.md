# hal/GpsDriver

**Status:** ✅ Code complete — Pi-Test (A.9) pending hardware access
**Files:** `hal/GpsDriver/GpsDriver.h`, `UbloxGpsDriver.h`, `UbloxGpsDriver.cpp`
**Purpose:** ZED-F9P GPS receiver driver — UBX binary parser + background reader thread

**Key types:**
- `GpsSolution` — Invalid / Float / Fixed
- `GpsData` — lat, lon, relPosN, relPosE, solution, numSV, hAccuracy, dgpsAge_ms, nmeaGGA, valid

**UBX messages parsed:** NAV-RELPOSNED (RTK), NAV-HPPOSLLH (lat/lon, accuracy), RXM-RTCM (dgpsAge)

**Config keys:** `gps_port`, `gps_baud` (default 115200), `gps_configure` (bool, default false)

**Notes:**
- `dataMutex_` guards all GpsData reads/writes.
- `Robot::setGpsDriver()` setter — optional, not in constructor.
- `ws_->broadcastNmea(line)` sends `{"type":"nmea","line":"..."}` to all clients.

**Tests:** `tests/test_gps_driver.cpp` — 9 tests: MockGpsDriver, decode sanity, quality transitions
