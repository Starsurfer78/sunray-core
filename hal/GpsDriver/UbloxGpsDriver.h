#pragma once
// UbloxGpsDriver.h — ZED-F9P GPS receiver driver for Sunray Core.
//
// Runs a background thread that reads UBX binary frames from a serial/USB port
// and parses:
//   UBX-NAV-RELPOSNED  → RTK solution (float/fixed), relPosN/E
//   UBX-NAV-HPPOSLLH   → WGS-84 lat/lon, hAccuracy
//   UBX-NAV-VELNED      → ground speed (stored, not yet exposed)
//   UBX-RXM-RTCM        → RTCM correction status (dgpsAge)
//   NMEA GNGGA          → raw GGA sentence
//
// Ported from sunray/src/ublox/ublox.cpp — all Arduino shims removed.
//
// Config keys:
//   gps_port       (string)  default: /dev/serial/by-id/usb-u-blox_...
//   gps_baud       (int)     default: 115200
//   gps_configure  (bool)    default: false — send UBX-CFG-VALSET at init

#include "hal/GpsDriver/GpsDriver.h"
#include "core/Config.h"
#include "core/Logger.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace sunray {

class UbloxGpsDriver : public GpsDriver {
public:
    explicit UbloxGpsDriver(std::shared_ptr<Config> config,
                            std::shared_ptr<Logger> logger);
    ~UbloxGpsDriver() override;

    bool    init()          override;
    GpsData getData() const override;

private:
    // ── UBX frame state machine ────────────────────────────────────────────────
    enum class State : uint8_t { SYNC1, SYNC2, CLASS, ID, LEN1, LEN2, PAYLOAD, CHKA, CHKB };

    void parseByte(uint8_t b);
    void dispatchMessage();
    void addChk(uint8_t b);

    // Little-endian unpack helpers (UBX is always little-endian)
    int32_t  unpackI32(int ofs) const;
    int8_t   unpackI8 (int ofs) const;
    uint32_t unpackU32(int ofs) const;

    // ── UBX frame builder / sender ────────────────────────────────────────────
    void sendUbx(uint8_t cls, uint8_t id, const std::vector<uint8_t>& payload);

    /// Send one UBX-CFG-VALSET packet (RAM layer).
    /// items: { key, value_bytes_LE } — value size inferred from key[31:28]
    bool sendCfgValset(
        const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& items);

    bool configure();

    // ── Background reader ─────────────────────────────────────────────────────
    void readerLoop();

    // ── Members ───────────────────────────────────────────────────────────────
    std::shared_ptr<Config> config_;
    std::shared_ptr<Logger> logger_;

    int fd_ = -1;  ///< POSIX file descriptor for GPS serial port

    // Parser state — owned by readerLoop thread only (no lock needed)
    State    state_    = State::SYNC1;
    uint8_t  msgClass_ = 0;
    uint8_t  msgId_    = 0;
    uint16_t msgLen_   = 0;
    uint16_t count_    = 0;
    uint8_t  chkA_     = 0;
    uint8_t  chkB_     = 0;
    uint8_t  payload_[1024]{};

    std::string nmeaAccum_;       ///< NMEA line accumulator (readerLoop only)
    uint64_t lastRelposMs_ = 0;   ///< steady_clock ms of last RELPOSNED (readerLoop only)
    uint64_t rtcmLastMs_   = 0;   ///< steady_clock ms of last RTCM packet (readerLoop only)

    // Shared GPS data — protected by dataMutex_
    mutable std::mutex dataMutex_;
    GpsData            data_;

    // Background thread
    std::thread       thread_;
    std::atomic<bool> running_{false};

    static constexpr const char* TAG             = "UbloxGps";
    static constexpr uint64_t    SOL_TIMEOUT_MS  = 3000;
    static constexpr const char* DEFAULT_PORT    =
        "/dev/serial/by-id/usb-u-blox_AG_-_www.u-blox.com_u-blox_GNSS_receiver-if00";
};

} // namespace sunray
