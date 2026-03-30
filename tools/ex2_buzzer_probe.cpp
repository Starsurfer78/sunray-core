#include "platform/I2C.h"
#include "platform/PortExpander.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    const char* busPath = (argc > 1) ? argv[1] : "/dev/i2c-1";
    const int durationMs = (argc > 2) ? std::atoi(argv[2]) : 150;
    if (durationMs < 1 || durationMs > 5000) {
        std::cerr << "ERROR: duration_ms must be in range 1..5000\n";
        return 1;
    }

    try {
        sunray::platform::I2C bus(busPath);
        sunray::platform::PortExpander ex2(bus, 0x20);

        std::cout << "== EX2 Buzzer Probe ==\n";
        std::cout << "Bus:      " << busPath << "\n";
        std::cout << "Addr:     0x20\n";
        std::cout << "Duration: " << durationMs << " ms\n";

        const bool onOk = ex2.setPin(1, 1, true);
        std::cout << "Buzzer ON:  " << (onOk ? "PASS" : "FAIL") << "\n";
        if (!onOk) return 2;

        std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));

        const bool offOk = ex2.setPin(1, 1, false);
        std::cout << "Buzzer OFF: " << (offOk ? "PASS" : "FAIL") << "\n";
        return offOk ? 0 : 3;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
