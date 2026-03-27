# platform — POSIX Linux APIs (Serial, I2C)
# Replaces Arduino HardwareSerial shims and Wire.h

add_library(sunray_platform STATIC
    Serial.cpp
    I2C.cpp
    PortExpander.cpp
)

target_include_directories(sunray_platform PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
