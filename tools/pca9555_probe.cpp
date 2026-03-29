#include "platform/I2C.h"

#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

bool readReg(sunray::platform::I2C& bus, uint8_t addr, uint8_t reg, uint8_t& value) {
    if (bus.writeRead(addr, &reg, 1, &value, 1)) return true;
    if (!bus.write(addr, &reg, 1)) return false;
    return bus.read(addr, &value, 1);
}

bool writeReg(sunray::platform::I2C& bus, uint8_t addr, uint8_t reg, uint8_t value) {
    const uint8_t buf[2] = {reg, value};
    return bus.write(addr, buf, sizeof(buf));
}

void printReg(sunray::platform::I2C& bus, uint8_t addr, uint8_t reg) {
    uint8_t value = 0;
    const bool ok = readReg(bus, addr, reg, value);
    std::cout << "  reg 0x" << std::hex << int(reg) << ": ";
    if (ok) {
        std::cout << "0x" << std::setw(2) << std::setfill('0') << int(value);
    } else {
        std::cout << "ERR";
    }
    std::cout << std::dec << "\n";
}

void dumpChip(sunray::platform::I2C& bus, uint8_t addr) {
    printReg(bus, addr, 0x00);
    printReg(bus, addr, 0x01);
    printReg(bus, addr, 0x02);
    printReg(bus, addr, 0x03);
    printReg(bus, addr, 0x06);
    printReg(bus, addr, 0x07);
}

void probeChip(sunray::platform::I2C& bus, uint8_t addr) {
    std::cout << "\n== PCA9555 Probe 0x" << std::hex << int(addr) << std::dec << " ==\n";
    std::cout << "Before:\n";
    dumpChip(bus, addr);

    uint8_t cfg0 = 0;
    uint8_t out0 = 0;
    const bool canReadCfg = readReg(bus, addr, 0x06, cfg0);
    const bool canReadOut = readReg(bus, addr, 0x02, out0);

    bool writeOk = false;
    if (canReadCfg && canReadOut) {
        const uint8_t testMask = 0x01;
        writeOk = writeReg(bus, addr, 0x06, static_cast<uint8_t>(cfg0 & ~testMask))
               && writeReg(bus, addr, 0x02, static_cast<uint8_t>(out0 ^ testMask))
               && writeReg(bus, addr, 0x02, out0)
               && writeReg(bus, addr, 0x06, cfg0);
    }

    std::cout << "\nWrite test: " << (writeOk ? "PASS" : "FAIL") << "\n";
    std::cout << "After:\n";
    dumpChip(bus, addr);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string busPath = (argc > 1) ? argv[1] : "/dev/i2c-1";

    try {
        sunray::platform::I2C bus(busPath);
        std::cout << "== PCA9555 Multi-Probe ==\n";
        std::cout << "Bus: " << busPath << "\n";
        probeChip(bus, 0x20);
        probeChip(bus, 0x21);
        probeChip(bus, 0x22);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
