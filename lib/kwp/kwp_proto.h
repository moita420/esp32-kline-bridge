#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

enum class KwpErr { Ok, Timeout, BadEcho, BadComplement, BadEnd, BadLen, Refused, NoData };

struct Block {
    uint8_t counter = 0;
    uint8_t title = 0;
    std::vector<uint8_t> data;
};

// Lowest-level transport: exchange single bytes on the K-line. Hardware implements
// it over UART2+L9637D; tests implement it in memory. delay_ms is a no-op in tests.
struct IByteChannel {
    virtual bool read(uint8_t& out, uint32_t timeout_ms) = 0;  // false = timeout
    virtual void write(uint8_t b) = 0;
    virtual bool full_duplex() = 0;   // K-line echoes our TX
    virtual void delay_ms(uint32_t ms) = 0;
    virtual ~IByteChannel() {}
};

namespace title {
    constexpr uint8_t ACK = 0x09, REFUSE = 0x0A, DISCONNECT = 0x06;
    constexpr uint8_t REQ_FAULTS = 0x07, REQ_GROUP = 0x29;
    constexpr uint8_t REQ_READ_RAM = 0x01, REQ_READ_ROM = 0x03;
    constexpr uint8_t RECV_ID = 0xF6, RECV_FAULTS = 0xFC, RECV_GROUP = 0xE7;
    constexpr uint8_t RECV_RAM = 0xFE, RECV_ROM = 0xFD;
}

struct Timing {
    uint32_t inter_byte = 1, complement_delay = 1, complement_timeout = 300;
    uint32_t block_timeout = 1200, byte_timeout = 300;
};

// Block-level transport: the seam KwpClient sits on. Protocol implements it over the
// wire; tests implement it with scripted blocks, so the client's request/keep-alive
// logic is tested without faking the byte-level handshake.
struct IBlockIo {
    virtual KwpErr send_block(uint8_t title_byte, const uint8_t* data, size_t n) = 0;
    virtual KwpErr send_ack() { return send_block(title::ACK, nullptr, 0); }
    virtual KwpErr recv_block(bool first, Block& out) = 0;
    virtual ~IBlockIo() {}
};

// Drives the block protocol over an IByteChannel, tracking the shared counter.
class Protocol : public IBlockIo {
public:
    Protocol(IByteChannel* ch, Timing t) : ch_(ch), t_(t), counter_(0) {}
    uint8_t counter() const { return counter_; }
    void set_counter(uint8_t c) { counter_ = c; }

    KwpErr send_block(uint8_t title_byte, const uint8_t* data, size_t n) override;
    KwpErr recv_block(bool first, Block& out) override;

private:
    KwpErr send_byte(uint8_t b, bool final_byte);
    KwpErr recv_byte(uint32_t timeout, bool final_byte, uint8_t& out);
    IByteChannel* ch_;
    Timing t_;
    uint8_t counter_;
};
