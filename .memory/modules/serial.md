# platform/Serial

**Status:** ✅ Complete
**Files:** `platform/Serial.h`, `platform/Serial.cpp`
**Purpose:** POSIX termios serial port wrapper — replaces LinuxSerial.cpp + Arduino shims

**Public API:** `read()`, `write()`, `writeStr()`, `available()`, `flush()`, `isOpen()`, `port()`

**Notes:**
- Raw 8N1, non-blocking (VMIN=0/VTIME=0). All c_iflag/c_lflag/c_oflag zeroed.
- Constructor throws on failure. Move-only (not copyable).
- Maps baud rates 50…115200 to B-constants.

**Tests:** `tests/test_serial.cpp` — 4 tests: nonexistent port throws, error message, move semantics, non-copyable
