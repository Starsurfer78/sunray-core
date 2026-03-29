#include "platform/I2C.h"
#include "platform/PortExpander.h"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace {

bool readReg(sunray::platform::I2C& bus, uint8_t addr, uint8_t reg, uint8_t& value) {
    return bus.writeRead(addr, &reg, 1, &value, 1);
}

void dumpRegs(sunray::platform::I2C& bus, uint8_t addr) {
    for (uint8_t reg : {uint8_t(0x00), uint8_t(0x01), uint8_t(0x02), uint8_t(0x03), uint8_t(0x06), uint8_t(0x07)}) {
        uint8_t value = 0;
        const bool ok = readReg(bus, addr, reg, value);
        std::cout << "  reg 0x" << std::hex << int(reg)
                  << ": " << (ok ? "0x" : "ERR");
        if (ok) std::cout << int(value);
        std::cout << std::dec << "\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    const char* busPath = (argc > 1) ? argv[1] : "/dev/i2c-1";
    const uint8_t addr = 0x22;

    try {
        sunray::platform::I2C bus(busPath);
        sunray::platform::PortExpander ex3(bus, addr);

        std::cout << "== EX3 LED Probe ==\n";
        std::cout << "Bus:     " << busPath << "\n";
        std::cout << "Addr:    0x22\n";
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
