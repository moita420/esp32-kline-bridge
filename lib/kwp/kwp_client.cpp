#include "kwp_client.h"

KwpErr KwpClient::connect(Ident& out) {
    // After 5-baud init the ECU streams ID blocks; ACK each until it ACKs us.
    for (int guard = 0; guard < 32; guard++) {
        Block blk;
        KwpErr e = p_->recv_block(true, blk);
        if (e != KwpErr::Ok) return e;
        if (blk.title == title::ACK) break;
        if (blk.title == title::RECV_ID) {
            std::string line(blk.data.begin(), blk.data.end());
            if (out.part_number.empty() &&
                (line.find("038906") != std::string::npos ||
                 line.find("0281") != std::string::npos)) {
                out.part_number = line;
            }
        }
        if ((e = p_->send_ack()) != KwpErr::Ok) return e;
    }
    return KwpErr::Ok;
}

KwpErr KwpClient::read_mem(uint8_t req_title, uint16_t addr, uint8_t len,
                        std::vector<uint8_t>& out) {
    out.clear();
    uint16_t a = addr;
    int remaining = len;
    while (remaining > 0) {
        uint8_t n = (uint8_t)(remaining > 8 ? 8 : remaining);
        uint8_t req[3] = { n, (uint8_t)(a >> 8), (uint8_t)(a & 0xFF) };
        KwpErr e = p_->send_block(req_title, req, 3);
        if (e != KwpErr::Ok) return e;
        // Read response block(s) until the ECU ACKs; take data from the first
        // non-empty block (title varies: RECV_RAM/RECV_ROM/other).
        std::vector<uint8_t> got;
        for (int guard = 0; guard < 8; guard++) {
            Block blk;
            if ((e = p_->recv_block(true, blk)) != KwpErr::Ok) return e;
            if (blk.title == title::REFUSE) return KwpErr::Refused;
            if (blk.title == title::ACK) break;
            if (got.empty() && !blk.data.empty()) got = blk.data;
            if ((e = p_->send_ack()) != KwpErr::Ok) return e;
        }
        if (got.empty()) return KwpErr::NoData;
        size_t take = got.size() < n ? got.size() : n;
        out.insert(out.end(), got.begin(), got.begin() + take);
        a += n; remaining -= n;
    }
    return KwpErr::Ok;
}

KwpErr KwpClient::snapshot(uint16_t start, uint16_t end,
                        std::vector<std::pair<uint16_t, uint8_t>>& out) {
    out.clear();
    for (uint16_t a = start; a < end; ) {
        uint16_t chunk = (uint16_t)((end - a) > 8 ? 8 : (end - a));
        std::vector<uint8_t> buf;
        KwpErr e = read_ram(a, (uint8_t)chunk, buf);
        if (e != KwpErr::Ok) return e;
        for (size_t i = 0; i < buf.size(); i++)
            out.push_back({ (uint16_t)(a + i), buf[i] });
        a += chunk;
    }
    return KwpErr::Ok;
}

std::vector<Run> KwpClient::coalesce(const std::vector<uint16_t>& addrs, uint16_t max_gap) {
    std::vector<Run> runs;
    if (addrs.empty()) return runs;
    std::vector<uint16_t> a = addrs;   // assume sorted ascending
    Run cur; cur.start = a[0]; cur.bytes.assign(1, 0);
    uint16_t last = a[0];
    for (size_t i = 1; i < a.size(); i++) {
        if (a[i] <= last + max_gap && a[i] > last) {
            for (uint16_t f = last + 1; f <= a[i]; f++) cur.bytes.push_back(0);
            last = a[i];
        } else if (a[i] == last) {
            continue;
        } else {
            runs.push_back(cur);
            cur = Run{}; cur.start = a[i]; cur.bytes.assign(1, 0); last = a[i];
        }
    }
    runs.push_back(cur);
    return runs;
}
