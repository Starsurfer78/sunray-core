# tests — Catch2 unit tests
# Each test file links against sunray_core + a mock HardwareInterface.
# No real hardware required — drivers are never linked into tests.

add_executable(sunray_tests
    test_config.cpp
    test_serial.cpp
    test_i2c.cpp
    test_port_expander.cpp
    test_serial_driver.cpp
    test_robot.cpp
    test_simulation_driver.cpp
    test_websocket_server.cpp
    # test_state_machine.cpp
)

target_link_libraries(sunray_tests PRIVATE
    sunray_core
    sunray_hal
    sunray_platform
    sunray_simulation_driver
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(sunray_tests)
