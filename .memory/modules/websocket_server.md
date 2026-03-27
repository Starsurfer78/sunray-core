# core/WebSocketServer

**Status:** ✅ Complete (A.6)
**Files:** `core/WebSocketServer.h`, `core/WebSocketServer.cpp`
**Purpose:** Crow-based WebSocket + HTTP server — 10 Hz telemetry push + command reception

**Endpoint:** `GET /ws/telemetry` — port from config `ws_port` (default 8765)
**Push:** 100 ms interval. Keepalive: `{"type":"ping"}` if no new data.
**Telemetry format (frozen, 15 keys):** `type/op/x/y/heading/battery_v/charge_v/gps_sol/gps_text/gps_lat/gps_lon/bumper_l/bumper_r/motor_err/uptime_s`
**Commands:** `{"cmd":"start|stop|dock|charge|setpos"}` → routed to Robot methods

**Notes:**
- Crow I/O in own thread pool; push loop in `serverThread_`; telemetry shared via mutex.
- Pimpl: Crow headers only in `.cpp` — not in `.h`.
- `Robot::setWebSocketServer(ws*)` setter.

**Tests:** `tests/test_websocket_server.cpp` — 8 tests: JSON format, all 15 keys, Op names, API surface
