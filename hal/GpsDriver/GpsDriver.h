#pragma once
// GpsDriver.h — Abstract GPS driver interface for Sunray Core.
//
// Implementors:
//   hal/GpsDriver/UbloxGpsDriver — ZED-F9P via USB serial
//   (tests use MockGpsDriver — see tests/test_gps_driver.cpp)

#include <cstdint>
#include <string>

namespace sunray
{

    /// RTK solution quality decoded from UBX-NAV-RELPOSNED flags bits 3-4.
    enum class GpsSolution : int
    {
        Invalid = 0, ///< no RTK data or solution timeout (3 s)
        Float = 1,   ///< RTK float — cm-level accuracy
        Fixed = 2,   ///< RTK fixed — mm-level accuracy
    };

    /// Complete GPS snapshot for one navigation cycle.
    struct GpsData
    {
        double lat = 0.0;     ///< WGS-84 latitude  (degrees, positive = N)
        double lon = 0.0;     ///< WGS-84 longitude (degrees, positive = E)
        float relPosN = 0.0f; ///< metres North from RTK base station
        float relPosE = 0.0f; ///< metres East from RTK base station
        GpsSolution solution = GpsSolution::Invalid;
        int numSV = 0;           ///< healthy satellites used in fix
        int numCorrSignals = 0;  ///< healthy rover signals with carrier corrections applied
        float hAccuracy = 0.0f;  ///< horizontal position accuracy (m)
        uint32_t dgpsAge_ms = 0; ///< ms since last RTCM correction received
        std::string nmeaGGA;     ///< last raw GNGGA sentence (trimmed)
        bool valid = false;      ///< true once first RELPOSNED received
    };

    /// Abstract GPS driver. Implementations open a serial/USB port and parse UBX frames.
    /// getData() must be thread-safe — called from Robot::run() while the driver
    /// may be parsing in a background thread.
    class GpsDriver
    {
    public:
        virtual ~GpsDriver() = default;

        /// Open serial port; optionally send UBX-CFG-VALSET to configure F9P.
        /// Returns false if the port cannot be opened.
        virtual bool init() = 0;

        /// Thread-safe: returns the most recent GPS data snapshot.
        virtual GpsData getData() const = 0;
    };

} // namespace sunray
