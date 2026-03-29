// Serial.cpp — POSIX termios serial port (see Serial.h for design notes).

#include "Serial.h"

#include <cerrno>
#include <cstring>   // strerror, strlen
#include <stdexcept>

#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace sunray::platform {

// ── Construction / destruction ────────────────────────────────────────────────

Serial::Serial(const std::string& port, unsigned int baud)
    : port_(port)
{
    fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        throw std::runtime_error(
            "Serial: cannot open '" + port + "': " + std::strerror(errno));
    }

    // Save original termios so we can restore it on close.
    if (::tcgetattr(fd_, &savedTermios_) < 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error(
            "Serial: tcgetattr failed on '" + port + "': " + std::strerror(errno));
    }

    // Build clean raw 8N1 configuration.
    struct termios t{};  // zero-initialise all fields
    speed_t speed = baudConstant(baud);
    ::cfsetispeed(&t, speed);
    ::cfsetospeed(&t, speed);

    t.c_cflag  = CS8 | CLOCAL | CREAD;  // 8 data bits, no modem control, enable RX
    t.c_iflag  = 0;  // raw input — no IXON/IXOFF, no CR/LF translation
    t.c_oflag  = 0;  // raw output
    t.c_lflag  = 0;  // raw mode — no ICANON, no ECHO, no signals
    t.c_cc[VMIN]  = 0;  // non-blocking: return immediately with available data
    t.c_cc[VTIME] = 0;

    // Drain stale bytes before changing settings.
    ::tcflush(fd_, TCIOFLUSH);

    if (::tcsetattr(fd_, TCSANOW, &t) < 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error(
            "Serial: tcsetattr failed on '" + port + "': " + std::strerror(errno));
    }

    // Drain again after configuration (some drivers buffer during tcsetattr).
    ::tcflush(fd_, TCIOFLUSH);
}

Serial::~Serial() {
    closePort();
}

// ── Move semantics ────────────────────────────────────────────────────────────

Serial::Serial(Serial&& o) noexcept
    : port_(std::move(o.port_))
    , fd_(o.fd_)
    , savedTermios_(o.savedTermios_)
{
    o.fd_ = -1;  // prevent double-close
}

Serial& Serial::operator=(Serial&& o) noexcept {
    if (this != &o) {
        closePort();
        port_         = std::move(o.port_);
        fd_           = o.fd_;
        savedTermios_ = o.savedTermios_;
        o.fd_         = -1;
    }
    return *this;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void Serial::closePort() {
    if (fd_ >= 0) {
        ::tcsetattr(fd_, TCSANOW, &savedTermios_);
        ::close(fd_);
        fd_ = -1;
    }
}

speed_t Serial::baudConstant(unsigned int baud) {
    switch (baud) {
        case 50:     return B50;
        case 110:    return B110;
        case 300:    return B300;
        case 600:    return B600;
        case 1200:   return B1200;
        case 2400:   return B2400;
        case 4800:   return B4800;
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:     return B115200;  // safe fallback — matches driver_baud default
    }
}

// ── I/O ───────────────────────────────────────────────────────────────────────

int Serial::read(uint8_t* buf, size_t maxLen) {
    if (fd_ < 0 || maxLen == 0) return 0;

    ssize_t n = ::read(fd_, buf, maxLen);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;  // no data yet
        return -1;  // real I/O error
    }
    return static_cast<int>(n);
}

bool Serial::write(const uint8_t* buf, size_t len) {
    if (fd_ < 0 || len == 0) return false;

    size_t written = 0;
    while (written < len) {
        ssize_t n = ::write(fd_, buf + written, len - written);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Kernel TX buffer full — yield and retry (rare on ttyS0 at 115200)
                continue;
            }
            return false;
        }
        written += static_cast<size_t>(n);
    }
    return true;
}

bool Serial::writeStr(const char* str) {
    return write(reinterpret_cast<const uint8_t*>(str), std::strlen(str));
}

// ── Status ────────────────────────────────────────────────────────────────────

int Serial::available() {
    if (fd_ < 0) return 0;
    int bytes = 0;
    if (::ioctl(fd_, FIONREAD, &bytes) < 0) return 0;
    return bytes;
}

bool Serial::waitReadable(int timeoutMs) {
    if (fd_ < 0) return false;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd_, &readfds);

    struct timeval tv{};
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    const int rc = ::select(fd_ + 1, &readfds, nullptr, nullptr, &tv);
    return rc > 0 && FD_ISSET(fd_, &readfds);
}

void Serial::flush() {
    if (fd_ >= 0) {
        ::tcflush(fd_, TCIOFLUSH);
    }
}

} // namespace sunray::platform
