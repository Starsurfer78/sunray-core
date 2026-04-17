// UbloxGpsDriver.cpp — ZED-F9P UBX binary protocol parser.
//
// Ported from sunray/src/ublox/ublox.cpp (Alexander Grau / Grau GmbH).
// All Arduino shims replaced with POSIX I/O and std::chrono.

#include "hal/GpsDriver/UbloxGpsDriver.h"

#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace sunray
{

    namespace
    {

        uint64_t nowMs()
        {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
        }

        speed_t baudConstant(unsigned int baud)
        {
            switch (baud)
            {
            case 9600:
                return B9600;
            case 19200:
                return B19200;
            case 38400:
                return B38400;
            case 57600:
                return B57600;
            case 115200:
                return B115200;
            default:
                return B115200;
            }
        }

        std::vector<uint8_t> u1(uint8_t value)
        {
            return {value};
        }

        std::vector<uint8_t> u2(uint16_t value)
        {
            return {
                static_cast<uint8_t>(value & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
            };
        }

        std::vector<uint8_t> u4(uint32_t value)
        {
            return {
                static_cast<uint8_t>(value & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 24) & 0xFF),
            };
        }

        int parseGgaSatelliteCount(const std::string &gga)
        {
            std::vector<std::string> fields;
            std::string token;
            for (char c : gga)
            {
                if (c == ',')
                {
                    fields.push_back(token);
                    token.clear();
                }
                else
                {
                    token += c;
                }
            }
            fields.push_back(token);

            if (fields.size() <= 7 || fields[7].empty())
                return 0;

            try
            {
                return std::stoi(fields[7]);
            }
            catch (...)
            {
                return 0;
            }
        }

        bool parseGgaLatLon(const std::string &gga, double &latDeg, double &lonDeg)
        {
            std::vector<std::string> fields;
            std::string token;
            for (char c : gga)
            {
                if (c == ',')
                {
                    fields.push_back(token);
                    token.clear();
                }
                else
                {
                    token += c;
                }
            }
            fields.push_back(token);

            if (fields.size() <= 6)
                return false;

            const std::string &latRaw = fields[2];
            const std::string &latHem = fields[3];
            const std::string &lonRaw = fields[4];
            const std::string &lonHem = fields[5];
            const std::string &qualityRaw = fields[6];

            if (latRaw.empty() || lonRaw.empty() || latHem.empty() || lonHem.empty())
                return false;

            try
            {
                const int quality = qualityRaw.empty() ? 0 : std::stoi(qualityRaw);
                if (quality <= 0)
                    return false;

                const double latVal = std::stod(latRaw);
                const double lonVal = std::stod(lonRaw);

                const double latD = std::floor(latVal / 100.0);
                const double latM = latVal - latD * 100.0;
                const double lonD = std::floor(lonVal / 100.0);
                const double lonM = lonVal - lonD * 100.0;

                latDeg = latD + latM / 60.0;
                lonDeg = lonD + lonM / 60.0;

                if (latHem == "S")
                    latDeg = -latDeg;
                if (lonHem == "W")
                    lonDeg = -lonDeg;

                if (latDeg == 0.0 || lonDeg == 0.0)
                    return false;

                return true;
            }
            catch (...)
            {
                return false;
            }
        }

    } // namespace

    // ── Construction / destruction ───────────────────────────────────────────────

    UbloxGpsDriver::UbloxGpsDriver(std::shared_ptr<Config> config,
                                   std::shared_ptr<Logger> logger)
        : config_(std::move(config)), logger_(std::move(logger))
    {
    }

    UbloxGpsDriver::~UbloxGpsDriver()
    {
        running_.store(false);
        if (thread_.joinable())
            thread_.join();
        if (fd_ >= 0)
            ::close(fd_);
    }

    // ── init ─────────────────────────────────────────────────────────────────────

    bool UbloxGpsDriver::init()
    {
        const std::string port = config_
                                     ? config_->get<std::string>("gps_port", DEFAULT_PORT)
                                     : std::string(DEFAULT_PORT);
        const unsigned int baud = config_
                                      ? static_cast<unsigned int>(config_->get<int>("gps_baud", 115200))
                                      : 115200u;
        const bool doConfig = config_
                                  ? config_->get<bool>("gps_configure", false)
                                  : false;

        fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd_ < 0)
        {
            logger_->error(TAG, "Cannot open GPS port: " + port);
            return false;
        }

        struct termios tty{};
        ::tcgetattr(fd_, &tty);
        ::cfmakeraw(&tty);
        tty.c_cflag |= CLOCAL | CREAD;
        tty.c_cflag &= ~CRTSCTS;
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 0;
        ::cfsetospeed(&tty, baudConstant(baud));
        ::cfsetispeed(&tty, baudConstant(baud));
        ::tcflush(fd_, TCIOFLUSH);
        ::tcsetattr(fd_, TCSANOW, &tty);
        ::tcflush(fd_, TCIOFLUSH);

        logger_->info(TAG, "GPS port opened: " + port + " @ " + std::to_string(baud));

        if (doConfig)
        {
            if (!configure())
            {
                logger_->warn(TAG, "F9P configuration failed — continuing with receiver defaults");
            }
        }

        running_.store(true);
        thread_ = std::thread(&UbloxGpsDriver::readerLoop, this);
        return true;
    }

    // ── getData ──────────────────────────────────────────────────────────────────

    GpsData UbloxGpsDriver::getData() const
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        return data_;
    }

    // ── readerLoop ───────────────────────────────────────────────────────────────

    void UbloxGpsDriver::readerLoop()
    {
        uint8_t buf[512];

        while (running_.load())
        {
            const uint64_t now = nowMs();

            // RELPOSNED timeout → demote solution to Invalid
            if (lastRelposMs_ > 0 && (now - lastRelposMs_) > SOL_TIMEOUT_MS)
            {
                std::lock_guard<std::mutex> lock(dataMutex_);
                data_.solution = GpsSolution::Invalid;
            }

            // Update dgpsAge from last RTCM packet time
            if (rtcmLastMs_ > 0)
            {
                std::lock_guard<std::mutex> lock(dataMutex_);
                data_.dgpsAge_ms = static_cast<uint32_t>(now - rtcmLastMs_);
            }

            const int n = ::read(fd_, buf, sizeof(buf));
            if (n <= 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            for (int i = 0; i < n; ++i)
            {
                parseByte(buf[i]);
            }
        }
    }

    // ── UBX parser ───────────────────────────────────────────────────────────────

    void UbloxGpsDriver::addChk(uint8_t b)
    {
        chkA_ = static_cast<uint8_t>((chkA_ + b) & 0xFF);
        chkB_ = static_cast<uint8_t>((chkB_ + chkA_) & 0xFF);
    }

    void UbloxGpsDriver::parseByte(uint8_t b)
    {
        // ── NMEA line accumulator (active when no UBX sync) ──────────────────────
        if (state_ == State::SYNC1 || state_ == State::SYNC2)
        {
            const char ch = static_cast<char>(b);
            if (ch == '$')
                nmeaAccum_.clear();
            if (nmeaAccum_.size() < 1000)
                nmeaAccum_ += ch;
            if (ch == '\n')
            {
                if (nmeaAccum_.size() > 6 &&
                    nmeaAccum_[0] == '$' &&
                    nmeaAccum_.substr(3, 3) == "GGA")
                {
                    // Trim trailing CR/LF
                    std::string gga = nmeaAccum_;
                    while (!gga.empty() &&
                           (gga.back() == '\r' || gga.back() == '\n'))
                    {
                        gga.pop_back();
                    }
                    const int numSV = parseGgaSatelliteCount(gga);
                    double lat = 0.0;
                    double lon = 0.0;
                    const bool hasLatLon = parseGgaLatLon(gga, lat, lon);
                    std::lock_guard<std::mutex> lock(dataMutex_);
                    data_.nmeaGGA = std::move(gga);
                    if (numSV > 0)
                        data_.numSV = numSV;
                    if (hasLatLon)
                    {
                        data_.lat = lat;
                        data_.lon = lon;
                    }
                }
                nmeaAccum_.clear();
            }
        }

        // ── UBX state machine ─────────────────────────────────────────────────────
        switch (state_)
        {
        case State::SYNC1:
            if (b == 0xB5)
                state_ = State::SYNC2;
            break;

        case State::SYNC2:
            if (b == 0x62)
            {
                state_ = State::CLASS;
                chkA_ = chkB_ = 0;
            }
            else
            {
                state_ = State::SYNC1;
            }
            break;

        case State::CLASS:
            msgClass_ = b;
            addChk(b);
            state_ = State::ID;
            break;

        case State::ID:
            msgId_ = b;
            addChk(b);
            state_ = State::LEN1;
            break;

        case State::LEN1:
            msgLen_ = b;
            addChk(b);
            state_ = State::LEN2;
            break;

        case State::LEN2:
            msgLen_ |= static_cast<uint16_t>(b) << 8;
            addChk(b);
            count_ = 0;
            state_ = State::PAYLOAD;
            break;

        case State::PAYLOAD:
            addChk(b);
            if (count_ < sizeof(payload_))
                payload_[count_] = b;
            ++count_;
            if (count_ >= msgLen_)
                state_ = State::CHKA;
            break;

        case State::CHKA:
            if (b == chkA_)
            {
                state_ = State::CHKB;
            }
            else
            {
                logger_->debug(TAG, "UBX chkA error cls=" + std::to_string(msgClass_) + " id=" + std::to_string(msgId_));
                state_ = State::SYNC1;
            }
            break;

        case State::CHKB:
            if (b == chkB_)
            {
                dispatchMessage();
            }
            else
            {
                logger_->debug(TAG, "UBX chkB error cls=" + std::to_string(msgClass_) + " id=" + std::to_string(msgId_));
            }
            state_ = State::SYNC1;
            break;
        }
    }

    // ── UBX message dispatcher ────────────────────────────────────────────────────

    void UbloxGpsDriver::dispatchMessage()
    {
        const uint64_t now = nowMs();

        switch (msgClass_)
        {
        case 0x01: // NAV class
            switch (msgId_)
            {
            case 0x12:
            { // UBX-NAV-VELNED
                // Ground speed and course-over-ground — stored but not yet exposed.
                // double groundSpeed = static_cast<double>(unpackU32(20)) / 100.0;
                // double heading = static_cast<double>(unpackI32(24)) * 1e-5 * (M_PI / 180.0);
                break;
            }
            case 0x14:
            { // UBX-NAV-HPPOSLLH
                // lat/lon in 1e-7 degrees (I4) + high-precision correction (I1, 1e-2 units of 1e-7 deg)
                const double lon = 1e-7 * (static_cast<double>(static_cast<int32_t>(unpackU32(8))) + static_cast<double>(unpackI8(24)) * 1e-2);
                const double lat = 1e-7 * (static_cast<double>(static_cast<int32_t>(unpackU32(12))) + static_cast<double>(unpackI8(25)) * 1e-2);
                // hAccuracy: U4 in 0.1 mm units → metres
                const float hAcc = static_cast<float>(unpackU32(28)) * 0.1f / 1000.f;
                {
                    std::lock_guard<std::mutex> lock(dataMutex_);
                    data_.lat = lat;
                    data_.lon = lon;
                    data_.hAccuracy = hAcc;
                }
                break;
            }
            case 0x07:
            { // UBX-NAV-PVT
                if (msgLen_ < 44)
                    break;
                // lon/lat in 1e-7 degrees at offsets 24/28
                const double lon = 1e-7 * static_cast<double>(static_cast<int32_t>(unpackU32(24)));
                const double lat = 1e-7 * static_cast<double>(static_cast<int32_t>(unpackU32(28)));
                // hAcc in mm at offset 40 → metres
                const float hAcc = static_cast<float>(unpackU32(40)) / 1000.f;
                if (lat != 0.0 || lon != 0.0)
                {
                    std::lock_guard<std::mutex> lock(dataMutex_);
                    data_.lat = lat;
                    data_.lon = lon;
                    data_.hAccuracy = hAcc;
                }
                break;
            }
            case 0x43:
            { // UBX-NAV-SIG
                if (msgLen_ < 8)
                    break;

                const int numSigs = unpackI8(5);
                int healthySignals = 0;
                int correctedSignals = 0;

                for (int i = 0; i < numSigs; ++i)
                {
                    const int blockOfs = 8 + 16 * i;
                    if ((blockOfs + 11) >= msgLen_)
                        break;

                    const uint16_t sigFlags = unpackU16(blockOfs + 10);
                    const bool health = (sigFlags & 0x3) == 0x1;
                    const bool prUsed = (sigFlags & 0x8) != 0;
                    const bool crCorrUsed = (sigFlags & 0x80) != 0;

                    if (!health || !prUsed)
                        continue;

                    ++healthySignals;
                    if (crCorrUsed)
                        ++correctedSignals;
                }

                std::lock_guard<std::mutex> lock(dataMutex_);
                if (healthySignals > 0)
                    data_.numSV = healthySignals;
                data_.numCorrSignals = correctedSignals;
                break;
            }
            case 0x3C:
            { // UBX-NAV-RELPOSNED
                // relPosN/E in cm (I4) → metres
                const float relPosN = static_cast<float>(
                                          static_cast<int32_t>(unpackU32(8))) /
                                      100.0f;
                const float relPosE = static_cast<float>(
                                          static_cast<int32_t>(unpackU32(12))) /
                                      100.0f;
                // flags[31:0] at offset 60; carrSoln = bits 4-3
                const uint32_t flags = unpackU32(60);
                const int carrSoln = static_cast<int>((flags >> 3) & 0x3);
                GpsSolution sol;
                switch (carrSoln)
                {
                case 1:
                    sol = GpsSolution::Float;
                    break;
                case 2:
                    sol = GpsSolution::Fixed;
                    break;
                default:
                    sol = GpsSolution::Invalid;
                    break;
                }
                lastRelposMs_ = now;
                {
                    std::lock_guard<std::mutex> lock(dataMutex_);
                    data_.relPosN = relPosN;
                    data_.relPosE = relPosE;
                    data_.solution = sol;
                    data_.valid = true;
                }
                break;
            }
            }
            break;

        case 0x02: // RXM class
            if (msgId_ == 0x32)
            { // UBX-RXM-RTCM
                rtcmLastMs_ = now;
                {
                    std::lock_guard<std::mutex> lock(dataMutex_);
                    data_.dgpsAge_ms = 0;
                }
            }
            break;
        }
    }

    // ── Unpack helpers ────────────────────────────────────────────────────────────

    int32_t UbloxGpsDriver::unpackI32(int ofs) const
    {
        return static_cast<int32_t>(
            (static_cast<uint32_t>(payload_[ofs + 3]) << 24) |
            (static_cast<uint32_t>(payload_[ofs + 2]) << 16) |
            (static_cast<uint32_t>(payload_[ofs + 1]) << 8) |
            (static_cast<uint32_t>(payload_[ofs + 0])));
    }

    uint32_t UbloxGpsDriver::unpackU32(int ofs) const
    {
        return (static_cast<uint32_t>(payload_[ofs + 3]) << 24) |
               (static_cast<uint32_t>(payload_[ofs + 2]) << 16) |
               (static_cast<uint32_t>(payload_[ofs + 1]) << 8) |
               (static_cast<uint32_t>(payload_[ofs + 0]));
    }

    uint16_t UbloxGpsDriver::unpackU16(int ofs) const
    {
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(payload_[ofs + 1]) << 8) |
            (static_cast<uint16_t>(payload_[ofs + 0])));
    }

    int8_t UbloxGpsDriver::unpackI8(int ofs) const
    {
        return static_cast<int8_t>(payload_[ofs]);
    }

    // ── UBX sender ───────────────────────────────────────────────────────────────

    void UbloxGpsDriver::sendUbx(uint8_t cls, uint8_t id,
                                 const std::vector<uint8_t> &payload)
    {
        const auto len = static_cast<uint16_t>(payload.size());
        std::vector<uint8_t> frame;
        frame.reserve(6 + len + 2);
        frame.push_back(0xB5);
        frame.push_back(0x62);
        frame.push_back(cls);
        frame.push_back(id);
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        for (uint8_t b : payload)
            frame.push_back(b);

        uint8_t ckA = 0, ckB = 0;
        for (size_t i = 2; i < frame.size(); ++i)
        {
            ckA = static_cast<uint8_t>((ckA + frame[i]) & 0xFF);
            ckB = static_cast<uint8_t>((ckB + ckA) & 0xFF);
        }
        frame.push_back(ckA);
        frame.push_back(ckB);

        ::write(fd_, frame.data(), frame.size());
    }

    bool UbloxGpsDriver::sendCfgValset(
        const std::vector<std::pair<uint32_t, std::vector<uint8_t>>> &items)
    {
        // UBX-CFG-VALSET payload: version(1) + layers(1) + reserved(2) + key-value pairs
        std::vector<uint8_t> payload;
        payload.push_back(0x00); // version = 0
        payload.push_back(0x01); // layers = RAM (bit 0)
        payload.push_back(0x00); // reserved
        payload.push_back(0x00);

        for (const auto &[key, val] : items)
        {
            payload.push_back(static_cast<uint8_t>(key & 0xFF));
            payload.push_back(static_cast<uint8_t>((key >> 8) & 0xFF));
            payload.push_back(static_cast<uint8_t>((key >> 16) & 0xFF));
            payload.push_back(static_cast<uint8_t>((key >> 24) & 0xFF));
            for (uint8_t b : val)
                payload.push_back(b);
        }

        sendUbx(0x06, 0x8A, payload);
        // Brief wait — no ACK parsing in Phase 1
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        return true;
    }

    // ── F9P configuration ─────────────────────────────────────────────────────────

    bool UbloxGpsDriver::configure()
    {
        logger_->info(TAG, "Configuring F9P (gps_configure=true)...");

        const bool filterEnabled = config_
                                       ? config_->get<bool>("gps_config_filter", true)
                                       : true;
        const uint8_t minElevDeg = static_cast<uint8_t>(std::clamp(
            config_ ? config_->get<int>("gps_receiver_min_elev_deg", 10) : 10,
            0, 90));
        const uint8_t cnoThresholdNumSvs = static_cast<uint8_t>(std::clamp(
            filterEnabled
                ? (config_ ? config_->get<int>("gps_receiver_cno_threshold_num_svs", 10) : 10)
                : 0,
            0, 32));
        const uint8_t cnoThresholdDbHz = static_cast<uint8_t>(std::clamp(
            filterEnabled
                ? (config_ ? config_->get<int>("gps_receiver_cno_threshold_dbhz", 30) : 30)
                : 0,
            0, 55));
        const uint8_t dgnssTimeoutS = static_cast<uint8_t>(std::clamp(
            config_ ? config_->get<int>("gps_receiver_dgnss_timeout_s", 60) : 60,
            0, 255));
        const uint32_t constrainAltCm = static_cast<uint32_t>(std::max(
            config_ ? config_->get<int>("gps_receiver_constrain_alt_cm", 10000) : 10000,
            0));

        // Enable required UBX messages on USB (all U1 = 1-byte value)
        const bool ok1 = sendCfgValset({
            {0x20910090, u1(1)},  // CFG-MSGOUT-UBX_NAV_RELPOSNED_USB  every solution
            {0x20910036, u1(1)},  // CFG-MSGOUT-UBX_NAV_HPPOSLLH_USB   every solution
            {0x20910045, u1(1)},  // CFG-MSGOUT-UBX_NAV_VELNED_USB     every solution
            {0x2091026b, u1(5)},  // CFG-MSGOUT-UBX_RXM_RTCM_USB       every 5 solutions
            {0x20910348, u1(20)}, // CFG-MSGOUT-UBX_NAV_SIG_USB        every 20 solutions
            {0x209100bd, u1(60)}, // CFG-MSGOUT-NMEA_ID_GGA_USB        every 60 solutions
        });

        // Ported from the original Sunray F9P tuning: fix mode, startup altitude
        // constraint, receiver-side DGNSS timeout and RTK stability filters.
        const bool ok2 = sendCfgValset({
            {0x20110011, u1(3)},                  // CFG-NAVSPG-FIXMODE        auto
            {0x10110013, u1(0)},                  // CFG-NAVSPG-INIFIX3D       no initial 3D requirement
            {0x401100c1, u4(constrainAltCm)},     // CFG-NAVSPG-CONSTR_ALT     startup altitude constraint
            {0x201100a4, u1(minElevDeg)},         // CFG-NAVSPG-INFIL_MINELEV  min satellite elevation
            {0x201100aa, u1(cnoThresholdNumSvs)}, // CFG-NAVSPG-INFIL_NCNOTHRS
            {0x201100ab, u1(cnoThresholdDbHz)},   // CFG-NAVSPG-INFIL_CNOTHRS
            {0x201100c4, u1(dgnssTimeoutS)},      // CFG-NAVSPG-CONSTR_DGNSSTO
        });

        // Set measurement rate: 200 ms (5 Hz), nav rate: 1 cycle (U2 = 2-byte LE)
        const bool ok3 = sendCfgValset({
            {0x30210001, u2(200)}, // CFG-RATE-MEAS = 200 ms
            {0x30210002, u2(1)},   // CFG-RATE-NAV  = 1 cycle
        });

        if (ok1 && ok2 && ok3)
        {
            logger_->info(TAG,
                          "F9P config applied: minElev=" + std::to_string(minElevDeg) +
                              "deg, C/N0 SVs=" + std::to_string(cnoThresholdNumSvs) +
                              ", C/N0=" + std::to_string(cnoThresholdDbHz) +
                              "dBHz, DGNSS timeout=" + std::to_string(dgnssTimeoutS) + "s");
            logger_->info(TAG, "F9P configuration sent successfully");
            return true;
        }
        return false;
    }

} // namespace sunray
