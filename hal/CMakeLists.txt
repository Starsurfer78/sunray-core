# hal — Hardware Abstraction Layer
# HardwareInterface.h is header-only (pure abstract).
# Driver implementations are in subdirectories.

# Interface-only target: other modules include hal/ to get HardwareInterface.h
add_library(sunray_hal INTERFACE)
target_include_directories(sunray_hal INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# SerialRobotDriver — Alfred/STM32 implementation (Step 6.2.4)
add_subdirectory(SerialRobotDriver)

# SimulationDriver — software-only driver for testing without hardware
add_subdirectory(SimulationDriver)

# GpsDriver — ZED-F9P via USB serial (A.8)
add_subdirectory(GpsDriver)
