#pragma once
// Serial.h — POSIX termios serial port wrapper (Linux only).
//
// Clean replacement for LinuxSerial.cpp + Arduino HardwareSerial shims.
// No Arduino dependencies, no global objects, no background threads.
//
// Design:
//   - Constructor opens and configures the port immediately.
//   - All I/O is non-blocking: read() returns 0 if no data is available.
//   - write() blocks only until the kernel write buffer accepts the data
//     (does NOT wait for transmission to complete — use flush() if needed).
//   - Destructor restores the original termios and closes the fd.
//   - Not copyable; move-constructible so it can be stored in unique_ptr or
//     returned from a factory function.
//
// Fixes vs. LinuxSerial.cpp:
//   - c_iflag / c_lflag / c_oflag explicitly zeroed (raw mode guaranteed)
//   - VMIN=0 / VTIME=0 for true non-blocking behaviour (no hidden 0.1 s wait)
//   - tcflush() both before and after tcsetattr to drain stale bytes
//   - Constructor throws std::runtime_error on failure — no silent half-open state
//
// Usage:
//   sunray::platform::Serial port("/dev/ttyS0", 115200);
//   port.writeStr("AT+V\r\n");
//   uint8_t buf[256];
//   int n = port.read(buf, sizeof(buf));

#include <cstddef>
#include <cstdint>
#include <string>
#include <termios.h>  // struct termios

namespace sunray::platform {

class Serial {
public:
    /// Open and configure the serial port.
    /// port: device path, e.g. "/dev/ttyS0" or "/dev/ttyAMA0"
    /// baud: standard baud rate (9600 … 115200); unmapped values default to 115200
    /// Throws std::runtime_error if the port cannot be opened or configured.
    Serial(const std::string& port, unsigned int baud);

    /// Restore original termios and close the file descriptor.
    ~Serial();

    // Non-copyable
    Serial(const Serial&)            = delete;
    Serial& operator=(const Serial&) = delete;

    // Move-constructible / move-assignable
    Serial(Serial&&) noexcept;
    Serial& operator=(Serial&&) noexcept;

    // ── I/O ───────────────────────────────────────────────────────────────────

    /// Read up to maxLen bytes into buf. Non-blocking.
    /// Returns: number of bytes read (0 = no data available, -1 = error).
    int  read(uint8_t* buf, size_t maxLen);

    /// Write exactly len bytes from buf. Retries on EAGAIN.
    /// Returns true if all bytes were accepted by the kernel.
    bool write(const uint8_t* buf, size_t len);

    /// Write a null-terminated C string. Convenience wrapper around write().
    bool writeStr(const char* str);

    // ── Status ────────────────────────────────────────────────────────────────

    /// Number of bytes available in the kernel receive buffer (ioctl FIONREAD).
    /// Returns 0 if the port is closed or the ioctl fails.
    int  available();

    /// Wait up to timeoutMs for readable input on the serial fd.
    /// Returns true if select() reports readable data.
    bool waitReadable(int timeoutMs);

    /// Discard all unread input and unsent output (tcflush TCIOFLUSH).
    void flush();

    /// True if the port is currently open.
    bool isOpen() const { return fd_ >= 0; }

    /// The device path passed to the constructor.
    const std::string& port() const { return port_; }

private:
    std::string    port_;
    int            fd_ = -1;
    struct termios savedTermios_{};  ///< original settings, restored in destructor

    void closePort();

    /// Map a numeric baud rate to a POSIX speed_t constant.
    /// Returns B115200 for any unrecognised value.
    static speed_t baudConstant(unsigned int baud);
};

} // namespace sunray::platform
