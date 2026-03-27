# hal/HardwareInterface

**Status:** ✅ Complete (154 lines)
**File:** `hal/HardwareInterface.h`
**Purpose:** Abstract base class — single Core↔Hardware boundary

**Data structs:**
- `OdometryData` — leftTicks, rightTicks, mowTicks, mcuConnected
- `SensorData` — bumperLeft/Right, lift, rain, stopButton, motorFault, nearObstacle
- `BatteryData` — voltage, chargeVoltage, chargeCurrent, batteryTemp, chargerConnected
- `LedId` — LED_1/2/3 | `LedState` — OFF/GREEN/RED

**Methods:** `init()`, `run()`, `setMotorPwm()`, `resetMotorFault()`, `readOdometry()`, `readSensors()`, `readBattery()`, `setBuzzer()`, `setLed()`, `keepPowerOn()`, `getCpuTemperature()`, `getRobotId()`, `getMcuFirmwareName()`, `getMcuFirmwareVersion()`

**Implementations:** `SerialRobotDriver` | `SimulationDriver`
