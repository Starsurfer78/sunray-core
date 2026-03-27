# hal/SerialRobotDriver

**Status:** ✅ Complete
**Files:** `hal/SerialRobotDriver/SerialRobotDriver.h`, `.cpp`
**Purpose:** Alfred (STM32) driver — implements HardwareInterface via UART AT-frames
**Depends on:** platform/Serial, platform/I2C, platform/PortExpander, core/Config

**Protocol:** AT+M @ 50 Hz, AT+S @ 2 Hz, AT+V once. CRC verify on RX (byte sum), CRC append on TX.

**Active bugs/fixes:**
- BUG-05: long cast for tick overflow ✅
- BUG-07: PWM/encoder swap in send+parse — **intentional**, wiring compensation ✅
- BUG-08: no Pi-side mow clamp ✅
- IMP-01: ovCheck in summaryFrame field 12 ✅

**Notes:**
- Fan via CPU temp (>65°C on, <60°C off).
- WiFi LED via wpa_cli (7s interval).
- `keepPowerOn(false)` → 5s grace → shutdown.
- Battery fallback 28V when MCU disconnected.

**Tests:** `tests/test_serial_driver.cpp` — Mock-based AT protocol tests
