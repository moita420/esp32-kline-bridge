#include <unity.h>
#include <deque>
#include "kwp_client.h"

// Scripted block-level ECU: recv_block hands back queued blocks in order; send_block
// just records the title. This tests the client's request/keep-alive logic without
// faking the byte handshake (Protocol's own tests cover that).
struct MockBlockIo : IBlockIo {
    std::deque<Block> incoming;
    std::vector<uint8_t> sent_titles;
    KwpErr send_block(uint8_t title_byte, const uint8_t*, size_t) override {
        sent_titles.push_back(title_byte);
        return KwpErr::Ok;
    }
    KwpErr recv_block(bool, Block& out) override {
        if (incoming.empty()) return KwpErr::Timeout;
        out = incoming.front(); incoming.pop_front();
        return KwpErr::Ok;
    }
};

void test_coalesce_merges_small_gaps() {
    // 0xC000,0xC001,0xC004 with max_gap=4 -> single run C000..C004 (5 bytes)
    auto runs = KwpClient::coalesce({ 0xC000, 0xC001, 0xC004 }, 4);
    TEST_ASSERT_EQUAL(1, runs.size());
    TEST_ASSERT_EQUAL(0xC000, runs[0].start);
    TEST_ASSERT_EQUAL(5, runs[0].bytes.size());
}

void test_coalesce_splits_big_gaps() {
    // gap of 0x20 -> two runs
    auto runs = KwpClient::coalesce({ 0xC000, 0xC020 }, 4);
    TEST_ASSERT_EQUAL(2, runs.size());
}

void test_read_ram_one_request() {
    MockBlockIo io;
    // ECU replies with a RECV_RAM block (0x12 0x34), then an ACK to end the exchange.
    io.incoming.push_back(Block{ 0x02, title::RECV_RAM, { 0x12, 0x34 } });
    io.incoming.push_back(Block{ 0x03, title::ACK, {} });
    KwpClient cl(&io);
    std::vector<uint8_t> out;
    TEST_ASSERT_EQUAL((int)KwpErr::Ok, (int)cl.read_ram(0xC2A8, 2, out));
    TEST_ASSERT_EQUAL(2, out.size());
    TEST_ASSERT_EQUAL(0x12, out[0]);
    TEST_ASSERT_EQUAL(0x34, out[1]);
    // First thing sent must be a ReadRAM request (title 0x01).
    TEST_ASSERT_TRUE(io.sent_titles.size() >= 1);
    TEST_ASSERT_EQUAL(title::REQ_READ_RAM, io.sent_titles[0]);
}

void test_refuse_is_reported() {
    MockBlockIo io;
    io.incoming.push_back(Block{ 0x02, title::REFUSE, {} });
    KwpClient cl(&io);
    std::vector<uint8_t> out;
    TEST_ASSERT_EQUAL((int)KwpErr::Refused, (int)cl.read_ram(0xE000, 1, out));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_coalesce_merges_small_gaps);
    RUN_TEST(test_coalesce_splits_big_gaps);
    RUN_TEST(test_read_ram_one_request);
    RUN_TEST(test_refuse_is_reported);
    return UNITY_END();
}
