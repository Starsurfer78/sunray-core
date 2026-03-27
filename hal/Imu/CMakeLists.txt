# Imu — IMU drivers (MPU-6050, etc.)
add_library(sunray_imu STATIC
    Mpu6050Driver.cpp
)

target_include_directories(sunray_imu PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(sunray_imu PUBLIC
    sunray_platform
    sunray_hal
)
