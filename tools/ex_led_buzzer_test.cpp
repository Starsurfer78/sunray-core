#include "platform/I2C.h"
#include "platform/I2cMux.h"
#include "platform/PortExpander.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <thread>

namespace {

bool setAllLeds(sunray::platform::PortExpander& ex3, bool green, bool red) {
    bool ok = true;
    ok = ex3.setPin(0, 0, green) && ok;
    ok = ex3.setPin(0, 1, red)   && ok;
    ok = ex3.setPin(0, 2, green) && ok;
    ok = ex3.setPin(0, 3, red)   && ok;
    ok = ex3.setPin(0, 4, green) && ok;
    ok = ex3.setPin(0, 5, red)   && ok;
    return ok;
}

void sleepMs(int durationMs) {
    std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));
}

} // namespace

int main(int argc, char** argv) {
    const char* busPath = (argc > 1) ? argv[1] : "/dev/i2c-1";
    const int ex3ChannelArg = (argc > 2) ? std::atoi(argv[2]) : 0;
    const int ledHoldMs = (argc > 3) ? std::atoi(argv[3]) : 10000;
    const int buzzerMs = (argc > 4) ? std::atoi(argv[4]) : 1000;

    if (ex3ChannelArg < 0 || ex3ChannelArg > 7) {
        std::cerr << "ERROR: ex3_channel must be in range 0..7\n";
        return 1;
    }
    if (ledHoldMs < 100 || ledHoldMs > 60000) {
        std::cerr << "ERROR: led_hold_ms must be in range 100..60000\n";
        return 1;
    }
    if (buzzerMs < 1 || buzzerMs > 10000) {
        std::cerr << "ERROR: buzzer_ms must be in range 1..10000\n";
        return 1;
    }

    const uint8_t ex3Channel = static_cast<uint8_t>(ex3ChannelArg);

    try {
        sunray::platform::I2C bus(busPath);
        sunray::platform::I2cMux mux(bus, 0x70);
        sunray::platform::PortExpander ex3(bus, 0x22);
        sunray::platform::PortExpander ex2(bus, 0x20);

        std::cout << "== EX LED + Buzzer Test ==\n";
        std::cout << "Bus:         " << busPath << "\n";
        std::cout << "EX3 channel: " << unsigned(ex3Channel) << "\n";
        std::cout << "LED hold:    " << ledHoldMs << " ms\n";
        std::cout << "Buzzer:      " << buzzerMs << " ms\n";

        if (!mux.selectChannel(ex3Channel)) {
            std::cerr << "ERROR: failed to select EX3 mux channel\n";
            return 2;
        }

        std::cout << "Phase 1/4: LEDs OFF\n";
        const bool offOk = setAllLeds(ex3, false, false);
        std::cout << "  result: " << (offOk ? "PASS" : "FAIL") << "\n";
        sleepMs(ledHoldMs);

        std::cout << "Phase 2/4: LEDs RED\n";
        const bool redOk = setAllLeds(ex3, false, true);
        std::cout << "  result: " << (redOk ? "PASS" : "FAIL") << "\n";
        sleepMs(ledHoldMs);

        std::cout << "Phase 3/4: LEDs GREEN\n";
        const bool greenOk = setAllLeds(ex3, true, false);
        std::cout << "  result: " << (greenOk ? "PASS" : "FAIL") << "\n";
        sleepMs(ledHoldMs);

        std::cout << "Phase 4/4: Buzzer ON\n";
        const bool buzzerOnOk = ex2.setPin(1, 1, true);
        std::cout << "  result: " << (buzzerOnOk ? "PASS" : "FAIL") << "\n";
        if (buzzerOnOk) {
            sleepMs(buzzerMs);
            const bool buzzerOffOk = ex2.setPin(1, 1, false);
            std::cout << "Buzzer OFF:  " << (buzzerOffOk ? "PASS" : "FAIL") << "\n";
        }

        setAllLeds(ex3, false, false);
        return (offOk && redOk && greenOk && buzzerOnOk) ? 0 : 3;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
