cmake_minimum_required(VERSION 3.20)
project(sunray-core VERSION 0.1.0 LANGUAGES CXX)

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

target_link_libraries(sunray-core PRIVATE
    sunray_serial_driver       # Alfred driver (links transitively: hal + platform + core)
    sunray_simulation_driver   # Software-only driver (--sim flag)
    sunray_core
    sunray_hal
    sunray_platform
    nlohmann_json::nlohmann_json
    Crow::Crow
)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
