# SerialRobotDriver — Alfred PCB implementation of HardwareInterface
# UART → STM32 (AT-frames) + Port-Expander I2C (LEDs, Buzzer, Fan, IMU power)

add_library(sunray_serial_driver STATIC
    SerialRobotDriver.cpp
)

target_include_directories(sunray_serial_driver PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sunray_serial_driver PUBLIC
    sunray_hal        # HardwareInterface.h
    sunray_platform   # Serial, I2C, PortExpander
    sunray_core       # Config.h (no circular dep: core never includes SerialRobotDriver)
)
