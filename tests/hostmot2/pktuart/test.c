#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "hostmot2.h"
#include "hostmot2-serial.h"

static hostmot2_t board;
static hm2_lowlevel_io_t llio;
static hm2_pktuart_instance_t instance;
static rtapi_u32 status;
static unsigned reads, writes;
static rtapi_u32 words[64], addresses[64];
static int lookup_result;
static unsigned fail_at, calls, errors;
static int failure;
static rtapi_u32 read_size;

int hm2_get_pktuart(hostmot2_t **hm2, const char *name) {
    (void)name;
    *hm2 = &board;
    return lookup_result;
}

void rtapi_print_msg(msg_level_t level, const char *fmt, ...) {
    if (level == RTAPI_MSG_ERR) errors++;
    (void)fmt;
}

static int queue_read(hm2_lowlevel_io_t *io, rtapi_u32 addr, void *data, int size) {
    (void)io;
    (void)addr;
    (void)data;
    assert(size == 4);
    reads++;
    calls++;
    if (fail_at && calls == fail_at) return failure;
    rtapi_u32 value = addr == instance.rx_mode_addr ? ((1u << 21) | (1u << 16)) :
        addr == instance.rx_fifo_count_addr ? read_size : 0x12345678;
    memcpy(data, &value, 4);
    return 1;
}

static int queue_write(hm2_lowlevel_io_t *io, rtapi_u32 addr, const void *data, int size) {
    (void)io;
    assert(size == 4 && writes < 64);
    calls++;
    if (fail_at && calls == fail_at) return failure;
    addresses[writes] = addr;
    memcpy(&words[writes++], data, 4);
    return 1;
}


static void test_failures(void) {
    llio.read = llio.queue_read = queue_read;
    llio.write = llio.queue_write = queue_write;
    instance.clock_freq = 50000000;
    instance.rx_mode_addr = 0x2000;
    instance.rx_fifo_count_addr = 0x2100;
    instance.rx_addr = 0x2200;
    instance.rx_bitrate_addr = 0x2300;
    instance.tx_mode_addr = 0x2400;
    instance.tx_bitrate_addr = 0x2500;
    unsigned char data[8] = {0};
    rtapi_u32 buffer[4];
    hm2_pktuart_config_t cfg = { .baudrate = 115200,
        .flags = HM2_PKTUART_CONFIG_FLUSH | HM2_PKTUART_CONFIG_RXEN | HM2_PKTUART_CONFIG_DRIVEEN };
    for (unsigned convention = 0; convention < 2; convention++) {
        failure = convention ? -EFAULT : 0;
        int expected = convention ? -EFAULT : -EIO;
        instance.tx_bitrate = instance.rx_bitrate = 115200;
        status = 2u << 16;
        for (fail_at = 1; fail_at <= 2; fail_at++) {
            calls = 0;
            assert(hm2_pktuart_queue_get_frame_sizes("test", buffer) == expected);
            assert(calls == fail_at);
            calls = 0;
            assert(hm2_pktuart_queue_read_data("test", buffer, 8) == expected);
            assert(calls == fail_at);
        }
        for (rtapi_u16 size = 1; size <= 8; size++) {
            unsigned operations = (size + 3) / 4 + 1;
            for (fail_at = 1; fail_at <= operations; fail_at++) {
                rtapi_u8 frames = 1;
                calls = writes = 0;
                assert(hm2_pktuart_send("test", data, &frames, &size) == expected);
                assert(calls == fail_at);
            }
            read_size = size;
            for (fail_at = 1; fail_at <= operations + 1; fail_at++) {
                rtapi_u8 frames = 1;
                rtapi_u16 limit = sizeof(data), frame_sizes[1];
                calls = 0;
                assert(hm2_pktuart_read("test", data, &frames, &limit, frame_sizes) == expected);
                assert(calls == fail_at);
            }
        }
        for (int queued = 0; queued <= 1; queued++) {
            for (unsigned fail = 1; fail <= 6; fail++) {
                instance.tx_bitrate = instance.rx_bitrate = 0;
                instance.tx_mode = instance.rx_mode = 0;
                fail_at = fail; calls = writes = 0;
                assert(hm2_pktuart_config("test", &cfg, &cfg, queued) == expected);
                assert(calls == fail);
                if (fail <= 1) assert(instance.rx_bitrate == 0);
                if (fail <= 2) assert(instance.rx_mode == 0);
                if (fail <= 4) assert(instance.tx_bitrate == 0);
                if (fail <= 5) assert(instance.tx_mode == 0);
                fail_at = 0; calls = writes = 0;
                assert(hm2_pktuart_config("test", &cfg, &cfg, queued) == 0);
                assert(instance.tx_bitrate != 0 && instance.rx_bitrate != 0);
                assert(instance.tx_mode != 0 && instance.rx_mode != 0);
            }
            for (fail_at = 1; fail_at <= 2; fail_at++) {
                calls = writes = errors = 0;
                if (queued) hm2_pktuart_queue_reset("test");
                else hm2_pktuart_reset("test");
                assert(calls == 2 && errors == 1);
            }
        }
        for (unsigned fail = 1; fail <= 6; fail++) {
            instance.tx_bitrate = instance.rx_bitrate = 0;
            fail_at = fail; calls = writes = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            assert(hm2_pktuart_setup("test", 115200, 0, 0, 1, 1) == expected);
#pragma GCC diagnostic pop
            assert(calls == fail);
        }
    }
    fail_at = 0;
    puts("PASS: zero/negative LLIO failures at every read/write stage, config retries and reset diagnostics");
}

int main(void) {
    board.llio = &llio;
    board.pktuart.instance = &instance;
    board.pktuart.rx_status_reg = &status;
    instance.tx_bitrate = instance.rx_bitrate = 115200;
    instance.tx_addr = 0x1000;
    instance.tx_fifo_count_addr = 0x1100;
    llio.queue_read = queue_read;
    llio.queue_write = queue_write;
    rtapi_u32 buffer[1024] = {0};

    for (unsigned count = 0; count <= 16; count++) {
        status = count << 16;
        reads = 0;
        assert(hm2_pktuart_queue_get_frame_sizes("test", buffer) == (int)count);
        assert(reads == count);
    }
    for (int bytes = 0; bytes <= 4096; bytes++) {
        reads = 0;
        assert(hm2_pktuart_queue_read_data("test", buffer, bytes) == (bytes + 3) / 4);
        assert(reads == (unsigned)(bytes + 3) / 4);
    }
    const int invalid_bytes[] = {INT_MIN, -1024, -4, -1};
    for (unsigned i = 0; i < sizeof(invalid_bytes) / sizeof(invalid_bytes[0]); i++) {
        reads = calls = 0;
        assert(hm2_pktuart_queue_read_data("test", buffer, invalid_bytes[i]) == -EINVAL);
        assert(reads == 0 && calls == 0);
    }
    instance.rx_bitrate = 0;
    assert(hm2_pktuart_queue_get_frame_sizes("test", buffer) == -EINVAL);
    assert(hm2_pktuart_queue_read_data("test", buffer, 4) == -EINVAL);
    instance.rx_bitrate = 115200;
    lookup_result = -1;
    assert(hm2_pktuart_queue_get_frame_sizes("test", buffer) == -ENODEV);
    assert(hm2_pktuart_queue_read_data("test", buffer, 4) == -ENODEV);
    lookup_result = 0;

    for (unsigned byte = 0; byte <= 255; byte++) {
        unsigned char data[8];
        memset(data, byte, sizeof(data));
        for (rtapi_u16 size = 1; size <= 8; size++) {
            rtapi_u8 frames = 1;
            writes = 0;
            assert(hm2_pktuart_send("test", data, &frames, &size) == size);
            assert(frames == 1);
            unsigned nwords = (size + 3) / 4;
            assert(writes == nwords + 1);
            for (unsigned i = 0; i < nwords; i++) {
                rtapi_u32 expected = 0;
                for (unsigned j = 0; j < 4 && i * 4 + j < size; j++)
                    expected |= (rtapi_u32)byte << (j * 8);
                assert(addresses[i] == instance.tx_addr && words[i] == expected);
            }
            assert(addresses[nwords] == instance.tx_fifo_count_addr);
            assert(words[nwords] == size);
        }
    }
    puts("PASS: 17 frame counts, 4097 byte counts, 8 error cases, 2048 packing cases");
    test_failures();
    return 0;
}
