#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hostmot2.h"

extern hostmot2_t *hm2;
extern hm2_sserial_remote_t *remote;
int sslbp_read_byte(rtapi_u32 addr);
static rtapi_u32 returned_word;
static int fail_doit, timeout;
void rtapi_print_msg(msg_level_t level, const char *fmt, ...) { (void)level; (void)fmt; }
void rtapi_print(const char *fmt, ...) { (void)fmt; }
long long rtapi_get_time(void) { static long long t; return t += 10000000; }
void rtapi_delay(long ns) { (void)ns; }
static int mock_read(hm2_lowlevel_io_t *io, rtapi_u32 address, void *data, int size) {
    (void)io; assert(size == 4);
    rtapi_u32 word = address == remote->rw_addr[0] ? returned_word :
        address == remote->data_reg_addr ? fail_doit : timeout;
    memcpy(data, &word, sizeof(word));
    return 1;
}
static int mock_write(hm2_lowlevel_io_t *io, rtapi_u32 addr, const void *data, int size) {
    (void)io; (void)addr; (void)data; assert(size == 4); return 1;
}
int main(void) {
    hm2_lowlevel_io_t llio = { .read = mock_read, .write = mock_write };
    hostmot2_t board = { .llio = &llio };
    hm2_sserial_remote_t chan = { .command_reg_addr = 0x100, .data_reg_addr = 0x200,
        .reg_cs_addr = 0x300, .rw_addr = {0x400} };
    hm2 = &board; remote = &chan;
    for (unsigned byte = 0; byte < 256; byte++) {
        returned_word = 0xabcdef00u | byte;
        assert(sslbp_read_byte(0) == (int)byte);
    }
    fail_doit = 1; assert(sslbp_read_byte(0) == -1);
    fail_doit = 0; timeout = 1; assert(sslbp_read_byte(0) == -1);
    puts("PASS: all 256 byte results (upper bits discarded), doit error and timeout preserved as -1");
    return 0;
}
