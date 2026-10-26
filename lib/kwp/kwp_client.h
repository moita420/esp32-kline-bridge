#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include "kwp_proto.h"

struct Ident { uint8_t kw1 = 0, kw2 = 0; std::string part_number; };
struct Run { uint16_t start; std::vector<uint8_t> bytes; };

class KwpClient {
public:
    explicit KwpClient(IBlockIo* p) : p_(p) {}

    // Consume the post-init ID blocks (title 0xF6 ASCII lines), ACKing each, until
    // the ECU sends ACK (0x09). Fills keywords + best-effort part number.
    KwpErr connect(Ident& out);

    KwpErr read_mem(uint8_t req_title, uint16_t addr, uint8_t len, std::vector<uint8_t>& out);
    KwpErr read_ram(uint16_t addr, uint8_t len, std::vector<uint8_t>& out) {
        return read_mem(title::REQ_READ_RAM, addr, len, out);
    }
    KwpErr read_rom(uint16_t addr, uint8_t len, std::vector<uint8_t>& out) {
        return read_mem(title::REQ_READ_ROM, addr, len, out);
    }

    // Read [start,end) into (addr,value) pairs, in runs of 8.
    KwpErr snapshot(uint16_t start, uint16_t end,
                    std::vector<std::pair<uint16_t, uint8_t>>& out);

    // Coalesce sorted addresses into runs, merging gaps <= max_gap.
    static std::vector<Run> coalesce(const std::vector<uint16_t>& addrs, uint16_t max_gap);

private:
    IBlockIo* p_;
};
