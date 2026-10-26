#include "kwp_proto.h"

KwpErr Protocol::send_byte(uint8_t b, bool final_byte) {
    ch_->delay_ms(t_.inter_byte);
    ch_->write(b);
    if (ch_->full_duplex()) {
        uint8_t echo;
        if (!ch_->read(echo, t_.byte_timeout)) return KwpErr::Timeout;
        if (echo != b) return KwpErr::BadEcho;
    }
    if (!final_byte) {
        uint8_t comp;
        if (!ch_->read(comp, t_.complement_timeout)) return KwpErr::Timeout;
        if (comp != (uint8_t)(b ^ 0xFF)) return KwpErr::BadComplement;
    }
    return KwpErr::Ok;
}

KwpErr Protocol::recv_byte(uint32_t timeout, bool final_byte, uint8_t& out) {
    if (!ch_->read(out, timeout)) return KwpErr::Timeout;
    if (!final_byte) {
        ch_->delay_ms(t_.complement_delay);
        uint8_t comp = out ^ 0xFF;
        ch_->write(comp);
        if (ch_->full_duplex()) {
            uint8_t echo;
            if (!ch_->read(echo, t_.byte_timeout)) return KwpErr::Timeout;
            if (echo != comp) return KwpErr::BadEcho;
        }
    }
    return KwpErr::Ok;
}

KwpErr Protocol::send_block(uint8_t title_byte, const uint8_t* data, size_t n) {
    uint8_t len = (uint8_t)(3 + n);
    counter_ = (uint8_t)(counter_ + 1);
    KwpErr e;
    if ((e = send_byte(len, false)) != KwpErr::Ok) return e;
    if ((e = send_byte(counter_, false)) != KwpErr::Ok) return e;
    if ((e = send_byte(title_byte, false)) != KwpErr::Ok) return e;
    for (size_t i = 0; i < n; i++)
        if ((e = send_byte(data[i], false)) != KwpErr::Ok) return e;
    return send_byte(0x03, true);   // block end: no complement
}

KwpErr Protocol::recv_block(bool first, Block& out) {
    uint32_t t0 = first ? t_.block_timeout : t_.byte_timeout;
    uint8_t len;
    KwpErr e;
    if ((e = recv_byte(t0, false, len)) != KwpErr::Ok) return e;
    if (len < 3) return KwpErr::BadLen;
    uint8_t counter, ttl;
    if ((e = recv_byte(t_.byte_timeout, false, counter)) != KwpErr::Ok) return e;
    if ((e = recv_byte(t_.byte_timeout, false, ttl)) != KwpErr::Ok) return e;
    out.counter = counter; out.title = ttl; out.data.clear();
    for (int i = 0; i < len - 3; i++) {
        uint8_t d;
        if ((e = recv_byte(t_.byte_timeout, false, d)) != KwpErr::Ok) return e;
        out.data.push_back(d);
    }
    uint8_t end;
    if ((e = recv_byte(t_.byte_timeout, true, end)) != KwpErr::Ok) return e;
    if (end != 0x03) return KwpErr::BadEnd;
    counter_ = counter;   // stay in step with the ECU
    return KwpErr::Ok;
}
