# platform/I2C

**Status:** ✅ Complete
**Files:** `platform/I2C.h`, `platform/I2C.cpp`
**Purpose:** Linux i2cdev bus wrapper — replaces Arduino Wire.h

**Public API:** `write(addr, buf, len)`, `read(addr, buf, len)`, `writeRead(addr, tx, txLen, rx, rxLen)`, `isOpen()`, `busPath()`

**Notes:**
- `writeRead()` uses I2C_RDWR ioctl for atomic Repeated-START.
- Address cached to avoid redundant ioctls.
- Alfred device map: 0x20–0x22 PCA9555, 0x50 EEPROM, 0x68 MCP3421, 0x70 TCA9548A.

**Tests:** `tests/test_i2c.cpp` — 4 sw-tests + 3 hardware tests (tagged `[.][hardware]`, skipped by default)
