# hal/GpsDriver — ZED-F9P UBX driver
add_library(sunray_gps_driver STATIC
    UbloxGpsDriver.cpp
)

target_include_directories(sunray_gps_driver PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(sunray_gps_driver PUBLIC
    sunray_core
    sunray_hal
    pthread
)
