cmake_minimum_required(VERSION 3.20)
project(sunray-core VERSION 1.2.0 LANGUAGES CXX)

# ── Language standard ─────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC)
    add_compile_options(/W4)
endif()

# export compile_commands.json for clangd / IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies (FetchContent) ───────────────────────────────────────────────
include(FetchContent)

# nlohmann/json — header-only, for Config class
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# Catch2 — unit test framework
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# Asio (standalone, header-only) — required by Crow
# FetchContent_Populate used because asio has no usable CMakeLists for library use.
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(asio)
if(NOT asio_POPULATED)
    FetchContent_Populate(asio)
endif()
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" CACHE PATH
    "Standalone Asio include directory (used by Crow)" FORCE)

# Crow — C++ HTTP/WebSocket server (header-only, uses Asio above)
FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG        v1.2.0
    GIT_SHALLOW    TRUE
)
set(CROW_USE_BOOST      OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Crow)

# Clipper2 — robust polygon offset/clipping (C++ only, used in MowRoutePlanner)
FetchContent_Declare(
    Clipper2
    GIT_REPOSITORY https://github.com/AngusJohnson/Clipper2.git
    GIT_TAG        Clipper2_1.4.0
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  CPP
)
set(CLIPPER2_UTILS    OFF CACHE BOOL "" FORCE)
set(CLIPPER2_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLIPPER2_TESTS    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Clipper2)

# ── Platform detection ────────────────────────────────────────────────────────
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(WARNING "sunray-core is designed to run on Linux (Raspberry Pi). "
                    "Building on ${CMAKE_SYSTEM_NAME} — driver code may not compile.")
endif()

# ── Subdirectories ────────────────────────────────────────────────────────────
# Each subdirectory defines a static library target.
# Core links against them — no circular dependencies.
#
#   platform  — POSIX Serial, I2C (no Arduino shims)
#   hal       — HardwareInterface + SerialRobotDriver implementation
#   core      — Robot, Op state machine, navigation (platform-independent)

add_subdirectory(platform)
add_subdirectory(hal)
add_subdirectory(core)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(sunray-core
    main.cpp
)

target_compile_definitions(sunray-core PRIVATE 
    SUNRAY_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(sunray-core PRIVATE
    sunray_serial_driver       # Alfred driver (links transitively: hal + platform + core)
    sunray_simulation_driver   # Software-only driver (--sim flag)
    sunray_gps_driver
    sunray_core
    sunray_hal
    sunray_platform
    nlohmann_json::nlohmann_json
    Crow::Crow
)

add_executable(rm18_serial_probe
    tools/rm18_serial_probe.cpp
)

target_link_libraries(rm18_serial_probe PRIVATE
    sunray_platform
)

add_executable(ex3_led_probe
    tools/ex3_led_probe.cpp
)

target_link_libraries(ex3_led_probe PRIVATE
    sunray_platform
)

add_executable(ex2_buzzer_probe
    tools/ex2_buzzer_probe.cpp
)

target_link_libraries(ex2_buzzer_probe PRIVATE
    sunray_platform
)

add_executable(ex_led_buzzer_test
    tools/ex_led_buzzer_test.cpp
)

target_link_libraries(ex_led_buzzer_test PRIVATE
    sunray_platform
)

add_executable(pca9555_probe
    tools/pca9555_probe.cpp
)

target_link_libraries(pca9555_probe PRIVATE
    sunray_platform
)

add_executable(hw_test
    tools/hw_test.cpp
)

target_link_libraries(hw_test PRIVATE
    sunray_platform
    sunray_imu
    sunray_core
)

add_executable(led_imu_test
    tools/led_imu_test.cpp
)

target_link_libraries(led_imu_test PRIVATE
    sunray_platform
    sunray_imu
    sunray_core
)

# ── Tests ─────────────────────────────────────────────────────────────────────
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
