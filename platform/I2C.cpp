// I2C.cpp — Linux i2cdev bus wrapper (see I2C.h for design notes).

#include "I2C.h"

#include <cerrno>
#include <cstring>   // strerror
#include <stdexcept>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

// Linux kernel I2C headers
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

namespace sunray::platform {

// ── Construction / destruction ────────────────────────────────────────────────

I2C::I2C(const std::string& bus)
    : bus_(bus)
{
    fd_ = ::open(bus.c_str(), O_RDWR);
    if (fd_ < 0) {
        throw std::runtime_error(
            "I2C: cannot open '" + bus + "': " + std::strerror(errno));
    }
}

I2C::~I2C() {
    closeBus();
}

// ── Move semantics ────────────────────────────────────────────────────────────

I2C::I2C(I2C&& o) noexcept
    : bus_(std::move(o.bus_))
    , fd_(o.fd_)
    , currentAddr_(o.currentAddr_)
{
    o.fd_ = -1;
}

I2C& I2C::operator=(I2C&& o) noexcept {
    if (this != &o) {
        closeBus();
        bus_         = std::move(o.bus_);
        fd_          = o.fd_;
        currentAddr_ = o.currentAddr_;
        o.fd_        = -1;
    }
    return *this;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void I2C::closeBus() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_          = -1;
        currentAddr_ = 0xFF;
    }
}

bool I2C::selectDevice(uint8_t addr) {
    if (addr == currentAddr_) return true;  // already selected — skip ioctl
    if (::ioctl(fd_, I2C_SLAVE, static_cast<long>(addr)) < 0) {
        return false;
    }
    currentAddr_ = addr;
    return true;
}

// ── Single-phase operations ───────────────────────────────────────────────────

bool I2C::write(uint8_t addr, const uint8_t* buf, size_t len) {
    if (fd_ < 0 || !buf || len == 0) return false;
    if (!selectDevice(addr)) return false;

    ssize_t n = ::write(fd_, buf, len);
    return n == static_cast<ssize_t>(len);
}

bool I2C::read(uint8_t addr, uint8_t* buf, size_t len) {
    if (fd_ < 0 || !buf || len == 0) return false;
    if (!selectDevice(addr)) return false;

    ssize_t n = ::read(fd_, buf, len);
    return n == static_cast<ssize_t>(len);
}

// ── Combined write+read (I2C_RDWR — Repeated START, no STOP) ─────────────────

bool I2C::writeRead(uint8_t        addr,
                    const uint8_t* txBuf, size_t txLen,
                    uint8_t*       rxBuf, size_t rxLen)
{
    if (fd_ < 0 || !txBuf || txLen == 0 || !rxBuf || rxLen == 0) return false;

    // Two i2c_msg structs — kernel executes both atomically.
    // The write message sets the register address; the read message follows
    // with a Repeated START (I2C_M_RD flag, no I2C_M_NOSTART — default).
    struct i2c_msg msgs[2] = {
        {
            .addr  = addr,
            .flags = 0,                              // write
            .len   = static_cast<__u16>(txLen),
            .buf   = const_cast<uint8_t*>(txBuf),   // kernel API requires non-const
        },
        {
            .addr  = addr,
            .flags = I2C_M_RD,                       // read, Repeated START
            .len   = static_cast<__u16>(rxLen),
            .buf   = rxBuf,
        },
    };

    struct i2c_rdwr_ioctl_data data = {
        .msgs  = msgs,
        .nmsgs = 2,
    };

    if (::ioctl(fd_, I2C_RDWR, &data) < 0) {
        // I2C_RDWR not supported by this kernel/driver — fall back to
        // separate write + read (works for most devices, no Repeated START).
        // selectDevice() sets I2C_SLAVE and updates currentAddr_.
        if (!selectDevice(addr)) return false;
        if (::write(fd_, txBuf, txLen) != static_cast<ssize_t>(txLen)) return false;
        if (::read(fd_,  rxBuf, rxLen) != static_cast<ssize_t>(rxLen)) return false;
        return true;
    }

    // I2C_RDWR uses address from the message struct directly — it does NOT call
    // I2C_SLAVE ioctl, so the kernel fd's slave address remains unchanged.
    // Invalidate the cache so the next write() / read() MUST issue I2C_SLAVE.
    // Without this, the next write() would skip the ioctl and go to the
    // previously selected address (e.g. the I2C mux at 0x70) instead of addr.
    currentAddr_ = 0xFF;
    return true;
}

} // namespace sunray::platform
