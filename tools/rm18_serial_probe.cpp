#include "platform/Serial.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

uint8_t calcCrc(const std::string& s) {
    uint8_t crc = 0;
    for (unsigned char c : s) crc += c;
    return crc;
}

std::string makeFrame(const std::string& body) {
    char crcBuf[12];
    std::snprintf(crcBuf, sizeof(crcBuf), ",0x%02X\r\n", calcCrc(body));
    return body + crcBuf;
}

std::string toHex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i) oss << ' ';
        oss << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return oss.str();
}

std::string toAscii(const std::vector<uint8_t>& data) {
    std::string out;
    out.reserve(data.size() * 2);
    for (uint8_t b : data) {
        char c = static_cast<char>(b);
        if (c == '\r') out += "\\r";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

std::vector<uint8_t> readFor(sunray::platform::Serial& port, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::vector<uint8_t> all;

    while (std::chrono::steady_clock::now() < deadline) {
        if (!port.waitReadable(50)) continue;

        while (true) {
            uint8_t buf[512];
            const int n = port.read(buf, sizeof(buf));
            if (n <= 0) break;
            all.insert(all.end(), buf, buf + n);
        }

        if (!all.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            while (port.waitReadable(10)) {
                uint8_t buf[512];
                const int n = port.read(buf, sizeof(buf));
                if (n <= 0) break;
                all.insert(all.end(), buf, buf + n);
            }
            break;
        }
    }

    return all;
}

void runProbe(sunray::platform::Serial& port, const std::string& body) {
    const std::string frame = makeFrame(body);
    std::cout << "\n[" << body << "] request\n";
    std::cout << "  frame:  " << frame;
    port.write(reinterpret_cast<const uint8_t*>(frame.data()), frame.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto rx = readFor(port, 1200);
    if (rx.empty()) {
        std::cout << "  bytes:  <no response>\n";
        return;
    }
    std::cout << "  bytes:  " << toHex(rx) << "\n";
    std::cout << "  ascii:  " << toAscii(rx) << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    const std::string port = (argc > 1) ? argv[1] : "/dev/ttyS0";
    const unsigned baud = (argc > 2) ? static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10)) : 19200U;

    try {
        std::cout << "== RM18 C++ Serial Probe ==\n";
        std::cout << "UART:    " << port << "\n";
        std::cout << "Baud:    " << baud << "\n";

        sunray::platform::Serial serial(port, baud);
        runProbe(serial, "AT+V");
        runProbe(serial, "AT+S");
        runProbe(serial, "AT+M,0,0,0");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
