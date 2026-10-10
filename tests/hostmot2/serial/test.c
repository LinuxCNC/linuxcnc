#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hostmot2.h"
#include "hostmot2-serial.h"

static hostmot2_t board;
static hm2_lowlevel_io_t llio;
static hm2_uart_instance_t uart;
static hm2_bspi_instance_t bspi;
static unsigned writes, calls, fail_at;
static int failure;
static rtapi_u32 read_count;
static rtapi_u32 rx_bitrate, tx_bitrate;
static rtapi_u32 words[16];
static int lookup_result;
int hm2_get_uart(hostmot2_t **hm2, const char *name) {
    (void)name; *hm2 = &board; return lookup_result;
}
int hm2_get_bspi(hostmot2_t **hm2, const char *name) {
    (void)name; *hm2 = &board; return lookup_result;
}
void rtapi_print_msg(msg_level_t level, const char *fmt, ...) {
    (void)level; (void)fmt;
}
int hm2_register_tram_read_region(hostmot2_t *h, rtapi_u16 a, rtapi_u16 s, rtapi_u32 **b) {
    (void)h; (void)a; (void)s; (void)b; return 0;
}
int hm2_register_tram_write_region(hostmot2_t *h, rtapi_u16 a, rtapi_u16 s, rtapi_u32 **b) {
    (void)h; (void)a; (void)s; (void)b; return 0;
}
static int write_word(hm2_lowlevel_io_t *io, rtapi_u32 a, const void *data, int size) {
    (void)io;
    assert(size == 4 && writes < 16);
    calls++;
    if (fail_at && calls == fail_at) return failure;
    memcpy(&words[writes++], data, 4);
    if (a == uart.rx_bitrate_addr) memcpy(&rx_bitrate, data, 4);
    if (a == uart.tx_bitrate_addr) memcpy(&tx_bitrate, data, 4);
    return 1;
}

static int read_word(hm2_lowlevel_io_t *io, rtapi_u32 addr, void *data, int size) {
    (void)io; assert(size == 4);
    calls++;
    if (fail_at && calls == fail_at) return failure;
    rtapi_u32 word = addr == uart.rx_fifo_count_addr ? read_count : 0x42424242;
    memcpy(data, &word, 4);
    return 1;
}
static void test_failures(void) {
    unsigned char data[8] = {0};
    uart.clock_freq = 50000000;
    uart.rx_fifo_count_addr = 0x100;
    uart.rx1_addr = 0x110; uart.rx2_addr = 0x120;
    uart.rx3_addr = 0x130; uart.rx4_addr = 0x140;
    uart.rx_bitrate_addr = 0x150; uart.tx_bitrate_addr = 0x160;
    llio.read = read_word;
    for (unsigned convention = 0; convention < 2; convention++) {
        failure = convention ? -EFAULT : 0;
        uart.bitrate = 115200;
        for (unsigned size = 1; size <= 8; size++) {
            for (fail_at = 1; fail_at <= (size + 3) / 4; fail_at++) {
                calls = writes = 0;
                assert(hm2_uart_send("test", data, size) == -1);
                assert(calls == fail_at);
            }
            read_count = size;
            for (fail_at = 1; fail_at <= (size + 3) / 4 + 1; fail_at++) {
                calls = 0;
                assert(hm2_uart_read("test", data) == -1);
                assert(calls == fail_at);
            }
        }
        for (unsigned fail = 1; fail <= 7; fail++) {
            uart.bitrate = 0;
            fail_at = fail; calls = writes = 0;
            assert(hm2_uart_setup("test", 115200, 0, 0) == -1);
            assert(calls == fail);
            if (fail <= 5) assert(uart.bitrate == 0);
            fail_at = 0; calls = writes = 0;
            assert(hm2_uart_setup("test", 115200, 0, 0) == 0);
            assert(calls == (fail <= 5 ? 7 : 2));
        }
        for (unsigned fail = 1; fail <= 5; fail++) {
            rtapi_u32 original = uart.bitrate;
            fail_at = fail; calls = writes = 0;
            assert(hm2_uart_setup("test", 230400, 0, 0) == -1);
            assert(calls == fail && uart.bitrate == 0);
            fail_at = 0; calls = writes = 0;
            assert(hm2_uart_setup("test", 115200, 0, 0) == 0);
            assert(calls == 7 && uart.bitrate == original);
            assert(rx_bitrate == original && tx_bitrate == original);
        }
        bspi.conf_flag[0] = true;
        calls = writes = 0; fail_at = 1;
        assert(hm2_bspi_write_chan("test", 0, 0) == -1);
        calls = 0;
        assert(hm2_bspi_clear_fifo("test") == -1);
    }
    fail_at = 0; writes = 0;
    assert(hm2_bspi_write_chan("test", 0, 0) == 1);
    assert(hm2_bspi_clear_fifo("test") == 1);
    for (unsigned size = 0; size <= 8; size++) {
        read_count = size;
        assert(hm2_uart_read("test", data) == (int)size);
        for (unsigned i = 0; i < size; i++) assert(data[i] == 0x42);
    }
    puts("PASS: UART/BSPI zero and negative I/O failures, setup retries, successful API returns");
}

int main(void) {
    board.llio = &llio;
    board.uart.instance = &uart;
    board.bspi.instance = &bspi;
    llio.write = write_word;
    unsigned char data[8] = {0};
    lookup_result = -1;
    for (unsigned i = 0; i < 3; i++) assert(hm2_uart_send("test", data, 4) == -1);
    lookup_result = 0;
    for (unsigned i = 0; i < 3; i++) assert(hm2_uart_send("test", data, 4) == -1);
    uart.bitrate = 115200;
    assert(hm2_uart_send("test", data, -1) == -1);
    assert(writes == 0);
    for (unsigned byte = 0; byte <= 255; byte++) {
        memset(data, byte, sizeof(data));
        for (unsigned size = 0; size <= 8; size++) {
            writes = 0;
            assert(hm2_uart_send("test", data, size) == (int)size);
            assert(writes == (size + 3) / 4);
            for (unsigned i = 0; i < writes; i++) {
                rtapi_u32 expected = 0;
                for (unsigned j = 0; j < 4 && i * 4 + j < size; j++)
                    expected |= (rtapi_u32)byte << (j * 8);
                assert(words[i] == expected);
            }
        }
    }
    int channels[] = {-1, 16, 1000};
    writes = 0;
    for (unsigned i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
        assert(hm2_bspi_write_chan("test", channels[i], 0) == -1);
        assert(hm2_tram_add_bspi_frame("test", channels[i], NULL, NULL) == -1);
    }
    assert(hm2_bspi_setup_chan("test", 0, 0, 8, 0.0, 0, 0, 0, 0, 0, 0) == -1);
    assert(hm2_bspi_setup_chan("test", 0, 0, 8, -1.0, 0, 0, 0, 0, 0, 0) == -1);
    assert(writes == 0);
    puts("PASS: repeated UART errors, 2304 packing cases, BSPI channel and frequency error returns");
    test_failures();
    return 0;
}
