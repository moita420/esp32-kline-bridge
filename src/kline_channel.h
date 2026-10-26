#pragma once
#include <Arduino.h>
#include "kwp_proto.h"

// K-line transport over UART2 + L9637D. Half-duplex: our TX echoes on RX.
class KLineChannel : public IByteChannel {
public:
    KLineChannel(HardwareSerial& uart, int rx, int tx, int nslp, uint32_t baud)
        : uart_(uart), rx_(rx), tx_(tx), nslp_(nslp), baud_(baud) {}

    bool begin();                              // wake L9637D, start UART2
    KwpErr slow_init(uint8_t module, uint8_t kw[2]);   // 5-baud wake-up
    void set_baud(uint32_t baud) { baud_ = baud; }

    // IByteChannel
    bool read(uint8_t& out, uint32_t timeout_ms) override;
    void write(uint8_t b) override;
    bool full_duplex() override { return true; }
    void delay_ms(uint32_t ms) override { delay(ms); }

    // Passive listener: read raw bytes without transmitting (sniff mode).
    int sniff(uint8_t* buf, int maxlen, uint32_t window_ms);

private:
    HardwareSerial& uart_;
    int rx_, tx_, nslp_;
    uint32_t baud_;
};
