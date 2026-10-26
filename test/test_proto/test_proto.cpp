#include <unity.h>
#include <deque>
#include <vector>
#include "kwp_proto.h"

// Mock channel that hands back scripted complements/bytes and records writes.
struct MockCh : IByteChannel {
    std::deque<uint8_t> to_read;
    std::vector<uint8_t> wrote;
    bool duplex = false;
    bool read(uint8_t& out, uint32_t) override {
        if (to_read.empty()) return false;
        out = to_read.front(); to_read.pop_front(); return true;
    }
    void write(uint8_t b) override {
        wrote.push_back(b);
        if (duplex) to_read.push_front(b);  // half-duplex echo
    }
    bool full_duplex() override { return duplex; }
    void delay_ms(uint32_t) override {}
};

// Queue an ECU->tester block's own bytes (tester generates complements itself).
static void queue_block(MockCh& m, uint8_t counter, uint8_t t, std::vector<uint8_t> d) {
    std::vector<uint8_t> b = { (uint8_t)(3 + d.size()), counter, t };
    b.insert(b.end(), d.begin(), d.end());
    b.push_back(0x03);
    for (uint8_t x : b) m.to_read.push_back(x);
}

void test_send_block_frames_length_counter_end() {
    MockCh m; m.duplex = false;
    // group-read request: title 0x29, data [0x01] -> bytes 04,01,29,01,03
    // complements for all but final: !04,!01,!29,!01
    m.to_read = { 0xFB, 0xFE, 0xD6, 0xFE };
    Protocol p(&m, Timing{});
    uint8_t d = 0x01;
    TEST_ASSERT_EQUAL((int)KwpErr::Ok, (int)p.send_block(title::REQ_GROUP, &d, 1));
    std::vector<uint8_t> want = { 0x04, 0x01, 0x29, 0x01, 0x03 };
    TEST_ASSERT_EQUAL(want.size(), m.wrote.size());
    for (size_t i = 0; i < want.size(); i++) TEST_ASSERT_EQUAL(want[i], m.wrote[i]);
    TEST_ASSERT_EQUAL(1, p.counter());
}

void test_recv_block_reads_rom_response() {
    MockCh m; m.duplex = true;   // echo our complements
    queue_block(m, 0x05, title::RECV_ROM, { 0xDE, 0xAD });
    Protocol p(&m, Timing{});
    Block blk;
    TEST_ASSERT_EQUAL((int)KwpErr::Ok, (int)p.recv_block(true, blk));
    TEST_ASSERT_EQUAL(title::RECV_ROM, blk.title);
    TEST_ASSERT_EQUAL(2, blk.data.size());
    TEST_ASSERT_EQUAL(0xDE, blk.data[0]);
    TEST_ASSERT_EQUAL(0xAD, blk.data[1]);
    TEST_ASSERT_EQUAL(0x05, blk.counter);
}

void test_bad_complement_reported() {
    struct Wrong : IByteChannel {
        bool read(uint8_t& out, uint32_t) override { out = 0x00; return true; }
        void write(uint8_t) override {}
        bool full_duplex() override { return false; }
        void delay_ms(uint32_t) override {}
    } w;
    Protocol p(&w, Timing{});
    TEST_ASSERT_EQUAL((int)KwpErr::BadComplement, (int)p.send_block(title::ACK, nullptr, 0));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_send_block_frames_length_counter_end);
    RUN_TEST(test_recv_block_reads_rom_response);
    RUN_TEST(test_bad_complement_reported);
    return UNITY_END();
}
