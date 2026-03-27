# platform/PortExpander

**Status:** ✅ Complete
**Files:** `platform/PortExpander.h`, `platform/PortExpander.cpp`
**Purpose:** PCA9555 16-bit I/O port expander driver (LEDs, Buzzer, Fan, IMU power)

**Public API:** `setPin(port, pin, level)`, `getPin(port, pin)`, `setOutputPort(port, val)`, `setConfigPort(port, mask)`, `getInputPort(port, val&)`, `address()`

**Notes:**
- No internal state cache (read-modify-write per call).
- Alfred addresses: EX1=0x21, EX2=0x20 (Buzzer), EX3=0x22 (LEDs).

**Tests:** `tests/test_port_expander.cpp` — I2C mock tests
