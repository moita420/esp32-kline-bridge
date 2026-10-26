#include "kline_channel.h"

bool KLineChannel::begin() {
    pinMode(nslp_, OUTPUT);
    digitalWrite(nslp_, HIGH);            // L9637D awake
    uart_.begin(baud_, SERIAL_8N1, rx_, tx_);
    return true;
}

KwpErr KLineChannel::slow_init(uint8_t module, uint8_t kw[2]) {
    // Build 10 line states: start(0) + 7 data LSB-first + odd parity + stop(1).
    bool bits[10];
    bool parity = true;                    // odd parity seed
    bits[0] = false;
    for (int i = 0; i < 7; i++) {
        bool b = ((module >> i) & 1) != 0;
        bits[1 + i] = b; parity ^= b;
    }
    bits[8] = parity;
    bits[9] = true;

    // Detach UART, bit-bang TX at 5 baud (200 ms/bit) through the L9637D.
    uart_.end();
    pinMode(tx_, OUTPUT);
    digitalWrite(tx_, HIGH);               // idle high
    delay(300);
    for (int i = 0; i < 10; i++) {
        digitalWrite(tx_, bits[i] ? HIGH : LOW);
        delay(200);
    }
    digitalWrite(tx_, HIGH);               // idle high

    // Re-attach UART and look for 0x55 sync, skipping break-noise (0x00/0xFF).
    uart_.begin(baud_, SERIAL_8N1, rx_, tx_);
    uint32_t deadline = millis() + 1000;
    bool got_sync = false;
    while (millis() < deadline) {
        if (uart_.available()) {
            uint8_t b = uart_.read();
            if (b == 0x55) { got_sync = true; break; }
            // else: noise or wrong-baud tell; keep scanning
        }
    }
    if (!got_sync) return KwpErr::Timeout;

    uint8_t k1, k2;
    if (!read(k1, 500)) return KwpErr::Timeout;
    if (!read(k2, 500)) return KwpErr::Timeout;
    kw[0] = k1; kw[1] = k2;
    delay(30);
    write((uint8_t)(k2 ^ 0xFF));           // reply: complement of KW2
    uint8_t echo; read(echo, 100);         // consume echo
    return KwpErr::Ok;
}

bool KLineChannel::read(uint8_t& out, uint32_t timeout_ms) {
    uint32_t deadline = millis() + timeout_ms;
    while (millis() < deadline) {
        if (uart_.available()) { out = uart_.read(); return true; }
    }
    return false;
}

void KLineChannel::write(uint8_t b) {
    uart_.write(b);
    uart_.flush();
}

int KLineChannel::sniff(uint8_t* buf, int maxlen, uint32_t window_ms) {
    int n = 0;
    uint32_t deadline = millis() + window_ms;
    while (millis() < deadline && n < maxlen)
        if (uart_.available()) buf[n++] = uart_.read();
    return n;
}
