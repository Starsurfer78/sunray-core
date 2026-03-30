#include "platform/I2C.h"
#include "platform/I2cMux.h"
#include "platform/PortExpander.h"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>

namespace {

bool readReg(sunray::platform::I2C& bus, uint8_t addr, uint8_t reg, uint8_t& value) {
    return bus.writeRead(addr, &reg, 1, &value, 1);
}

void printValueOrErr(bool ok, uint8_t value) {
    if (!ok) {
        std::cout << "ERR";
        return;
    }
    std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0')
              << unsigned(value) << std::dec;
}

void dumpRegs(sunray::platform::I2C& bus, uint8_t addr) {
    for (uint8_t reg : {uint8_t(0x00), uint8_t(0x01), uint8_t(0x02), uint8_t(0x03), uint8_t(0x06), uint8_t(0x07)}) {
        uint8_t value = 0;
        const bool ok = readReg(bus, addr, reg, value);
        std::cout << "  reg 0x" << std::hex << int(reg) << std::dec << ": ";
        printValueOrErr(ok, value);
        std::cout << "\n";
    }
}

bool probeEx3Presence(sunray::platform::I2C& bus, uint8_t addr, uint8_t& value) {
    return readReg(bus, addr, 0x06, value);
}

void scanMuxChannels(sunray::platform::I2cMux& mux, sunray::platform::I2C& bus, uint8_t addr) {
    std::cout << "Mux channel scan for 0x22 via reg 0x06:\n";
    for (uint8_t channel = 0; channel < 8; ++channel) {
        const bool selectOk = mux.selectChannel(channel);
        uint8_t value = 0;
        const bool probeOk = selectOk && probeEx3Presence(bus, addr, value);
        std::cout << "  ch " << unsigned(channel) << ": ";
        if (!selectOk) {
            std::cout << "select FAIL";
        } else {
            printValueOrErr(probeOk, value);
        }
        std::cout << "\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    const char* busPath = (argc > 1) ? argv[1] : "/dev/i2c-1";
    const uint8_t addr = 0x22;
    const uint8_t muxAddr = 0x70;
    const int channelArg = (argc > 2) ? std::atoi(argv[2]) : 1;
    if (channelArg < 0 || channelArg > 7) {
        std::cerr << "ERROR: mux channel must be in range 0..7\n";
        return 1;
    }
    const uint8_t ex3Channel = static_cast<uint8_t>(channelArg);

    try {
        sunray::platform::I2C bus(busPath);
        sunray::platform::I2cMux mux(bus, muxAddr);

        std::cout << "== EX3 LED Probe ==\n";
        std::cout << "Bus:     " << busPath << "\n";
        std::cout << "Mux:     0x70\n";
        std::cout << "Channel: " << unsigned(ex3Channel) << "\n";
        std::cout << "Addr:    0x22\n";

        uint8_t mask = 0;
        const bool maskBeforeOk = mux.getEnabledMask(mask);
        std::cout << "Mux mask before select: ";
        printValueOrErr(maskBeforeOk, mask);
        std::cout << "\n";

        if (!mux.selectChannel(ex3Channel)) {
            std::cerr << "WARN: TCA9548A mux channel select failed\n";
        }

        const bool maskAfterOk = mux.getEnabledMask(mask);
        std::cout << "Mux mask after select:  ";
        printValueOrErr(maskAfterOk, mask);
        std::cout << "\n";

        scanMuxChannels(mux, bus, addr);
        mux.selectChannel(ex3Channel);

        sunray::platform::PortExpander ex3(bus, addr);
        std::cout << "Before:\n";
        dumpRegs(bus, addr);

        bool ok = true;
        ok = ex3.setPin(0, 0, true) && ok;   // LED1 green on
        ok = ex3.setPin(0, 1, false) && ok;  // LED1 red off
        ok = ex3.setPin(0, 2, true) && ok;   // LED2 green on
        ok = ex3.setPin(0, 3, false) && ok;  // LED2 red off
        ok = ex3.setPin(0, 4, true) && ok;   // LED3 green on
        ok = ex3.setPin(0, 5, false) && ok;  // LED3 red off

        std::cout << "\nWrite result: " << (ok ? "PASS" : "FAIL") << "\n";
        std::cout << "After:\n";
        dumpRegs(bus, addr);

        return ok ? 0 : 2;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
