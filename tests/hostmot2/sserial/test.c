#undef realloc
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hostmot2.h"

int hm2_absenc_parse_format(hm2_sserial_remote_t *, hm2_absenc_format_t *);
int hm2_sserial_get_bytes(hostmot2_t *, hm2_sserial_remote_t *, void *, int, int);
int hm2_sserial_update_params(hostmot2_t *, hm2_sserial_instance_t *, long);

static int fail_alloc;
static unsigned stop_writes;
static unsigned rom_index;
static unsigned char rom[128];
static rtapi_u64 hal_storage[4096];
static size_t hal_used;
static int discovery_port = -1;
static unsigned last_command[3], command_reads[3];

void *hal_malloc(long size) {
    if (size <= 0) return NULL;
    size_t words = (size + sizeof(*hal_storage) - 1) / sizeof(*hal_storage);
    assert(hal_used + words <= sizeof(hal_storage) / sizeof(*hal_storage));
    void *p = &hal_storage[hal_used];
    hal_used += words;
    return p;
}
#define MOCK_HAL_NEW(name, ref_type, value_type) \
int name(int compid, hal_pdir_t dir, ref_type *ref, value_type def, const char *fmt, ...) { \
    (void)compid; (void)dir; (void)fmt; \
    *ref = (ref_type)hal_malloc(sizeof(rtapi_u64)); \
    *(value_type *)*ref = def; \
    return 0; \
}
MOCK_HAL_NEW(hal_pin_new_bool, hal_bool_t, rtapi_bool)
MOCK_HAL_NEW(hal_pin_new_si32, hal_sint_t, rtapi_s32)
MOCK_HAL_NEW(hal_pin_new_ui32, hal_uint_t, rtapi_u32)
MOCK_HAL_NEW(hal_pin_new_real, hal_real_t, rtapi_real)
MOCK_HAL_NEW(hal_param_new_bool, hal_bool_t, rtapi_bool)
MOCK_HAL_NEW(hal_param_new_si32, hal_sint_t, rtapi_s32)
MOCK_HAL_NEW(hal_param_new_ui32, hal_uint_t, rtapi_u32)
MOCK_HAL_NEW(hal_param_new_real, hal_real_t, rtapi_real)
int hal_param_new_fake(int compid, hal_refs_u *refs) {
    (void)compid; memset(refs, 0, sizeof(*refs)); return 0;
}
int hm2_md_is_consistent(hostmot2_t *hm2, int i, rtapi_u8 v, rtapi_u8 n, rtapi_u32 s, rtapi_u32 m) {
    (void)v; (void)s; (void)m;
    return n == (hm2->md[i].gtag == HM2_GTAG_SMARTSERIALB ? 10 : 6);
}
int hm2_md_is_consistent_or_complain(hostmot2_t *hm2, int i, rtapi_u8 v, rtapi_u8 n, rtapi_u32 s, rtapi_u32 m) {
    return hm2_md_is_consistent(hm2, i, v, n, s, m);
}
const char *hm2_get_general_function_name(int tag) { (void)tag; return "SmartSerial"; }
int hm2_register_tram_read_region(hostmot2_t *hm2, rtapi_u16 addr, rtapi_u16 size, rtapi_u32 **p) {
    (void)hm2; (void)addr; (void)size; *p = hal_malloc(sizeof(**p)); return 0;
}
int hm2_register_tram_write_region(hostmot2_t *hm2, rtapi_u16 addr, rtapi_u16 size, rtapi_u32 **p) {
    return hm2_register_tram_read_region(hm2, addr, size, p);
}
void hm2_stepgen_allocate_pins(hostmot2_t *hm2) { (void)hm2; }

void *hm2_test_realloc(void *p, size_t size) {
    return fail_alloc ? NULL : realloc(p, size);
}
void rtapi_print_msg(msg_level_t level, const char *fmt, ...) {
    (void)level; (void)fmt;
}
void rtapi_print(const char *fmt, ...) { (void)fmt; }
int rtapi_snprintf(char *buffer, unsigned long size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int result = vsnprintf(buffer, size, fmt, ap);
    va_end(ap);
    return result;
}
long simple_strtol(const char *s, char **end, int base) { return strtol(s, end, base); }
long long rtapi_get_time(void) { return 0; }
void rtapi_delay(long ns) { (void)ns; }
static int read_word(hm2_lowlevel_io_t *io, rtapi_u32 addr, void *p, int size) {
    (void)io;
    assert(size == 4);
    rtapi_u32 value = addr == 0x300 ? rom[rom_index++] : 0;
    if (discovery_port >= 0) {
        unsigned port = (addr & 0xff) / 0x40;
        assert(port < 3);
        if ((addr & 0xff00) == 0x1000) command_reads[port]++;
        if ((addr & 0xff00) == 0x1100
                && (last_command[port] & 0xff00) == (READ_LOCAL_CMD & 0xff00)) value = 43;
        if (addr == 0x1400u + 0x40 * discovery_port) value = 0x41416937;
        // A discovered remote with empty process/global tables.
        if (addr == 0x1500u + 0x40 * discovery_port) value = 0x00020001;
    }
    memcpy(p, &value, sizeof(value));
    return 1;
}
static int write_word(hm2_lowlevel_io_t *io, rtapi_u32 addr, const void *p, int size) {
    (void)io;
    assert(size == 4 && addr != 0);
    rtapi_u32 value;
    memcpy(&value, p, sizeof(value));
    if (value == 0x800) stop_writes++;
    if (discovery_port >= 0 && (addr & 0xff00) == 0x1000) {
        unsigned port = (addr & 0xff) / 0x40;
        assert(port < 3);
        last_command[port] = value;
    }
    return 1;
}
static void test_discovery(void) {
    const int tags[] = {HM2_GTAG_SMARTSERIAL, HM2_GTAG_SMARTSERIALB};
    for (unsigned t = 0; t < sizeof(tags) / sizeof(*tags); t++) {
        for (discovery_port = 0; discovery_port < 3; discovery_port++) {
            hm2_lowlevel_io_t llio = { .read = read_word, .write = write_word };
            hostmot2_t board = { .llio = &llio };
            hm2_pin_t pins[24] = {0};
            rtapi_u32 source = 0;
            hal_used = 0;
            memset(hal_storage, 0, sizeof(hal_storage));
            memset(last_command, 0, sizeof(last_command));
            memset(command_reads, 0, sizeof(command_reads));
            for (unsigned p = 0; p < 24; p++) {
                pins[p].primary_tag = HM2_GTAG_IOPORT;
                pins[p].bit_num = p;
                if (p < 3) {
                    pins[p].sec_tag = tags[t];
                    pins[p].sec_unit = p;
                    pins[p].sec_pin = 0x81;
                }
            }
            board.pin = pins; board.num_pins = 24;
            board.ioport.num_instances = 1; board.idrom.port_width = 24;
            board.ioport.alt_source_reg = &source;
            board.ioport.ddr_addr = 0x2000; board.ioport.alt_source_addr = 0x2100;
            board.config.num_sserials = 1; board.sserial.baudrate = -1;
            memset(board.config.sserial_modes, 'x', sizeof(board.config.sserial_modes));
            for (int p = 0; p <= discovery_port; p++) board.config.sserial_modes[p][0] = '0';
            board.md[0] = (hm2_module_descriptor_t){ .gtag = tags[t], .instances = 3,
                .base_address = 0x1000, .instance_stride = 0x40, .register_stride = 0x100 };
            assert(hm2_sserial_parse_md(&board, 0) == 1);
            assert(board.sserial.instance[0].index == discovery_port);
            hm2_configure_pins(&board);
            assert(source == (1u << discovery_port));
            for (int p = 0; p < 3; p++) {
                assert((command_reads[p] != 0) == (p <= discovery_port));
                assert(pins[p].gtag == (p == discovery_port ? tags[t] : HM2_GTAG_IOPORT));
                assert(pins[p].direction == (p == discovery_port ? HM2_PIN_DIR_IS_OUTPUT : HM2_PIN_DIR_IS_INPUT));
            }
            hm2_sserial_cleanup(&board);
        }
    }
    discovery_port = -1;
}
static void test_formats(void) {
    hm2_sserial_remote_t chan = {0};
    hm2_absenc_format_t def = {0};
    char *format = def.string;
    for (unsigned length = 1; length <= 48; length++) {
        memset(format, 'a', length);
        strcpy(format + length, "%32u");
        assert(hm2_absenc_parse_format(&chan, &def) == 0);
        assert(chan.num_confs == 1 && chan.confs[0].DataLength == 32);
        assert(strlen(chan.confs[0].NameString) == length);
        free(chan.confs); chan.confs = NULL; chan.num_confs = 0;
    }
    memset(format, 'a', 49); strcpy(format + 49, "%32u");
    assert(hm2_absenc_parse_format(&chan, &def) == -EINVAL);
    assert(chan.confs == NULL && chan.num_confs == 0);
    strcpy(format, "value%256u");
    assert(hm2_absenc_parse_format(&chan, &def) == -EINVAL);
    strcpy(format, "low%16lhigh%16H");
    assert(hm2_absenc_parse_format(&chan, &def) == 0);
    assert(chan.num_confs == 2 && chan.confs[1].DataType == LBP_ENCODER_H);
    void *old = chan.confs;
    fail_alloc = 1;
    strcpy(format, "next%32u");
    assert(hm2_absenc_parse_format(&chan, &def) == -ENOMEM);
    assert(chan.num_confs == 2 && chan.confs == old);
    fail_alloc = 0;
    free(chan.confs);
}
static void test_strings(hostmot2_t *hm2) {
    hm2_sserial_remote_t chan = {0};
    chan.reg_cs_addr = 0x100; chan.command_reg_addr = 0x200; chan.rw_addr[0] = 0x300;
    char buffer[HM2_SSERIAL_MAX_STRING_LENGTH + 1];
    for (unsigned length = 0; length <= 64; length++) {
        for (int mode = -2; mode <= -1; mode++) {
            memset(rom, 'A', sizeof(rom)); rom[length] = 0;
            memset(buffer, 0x7f, sizeof(buffer)); rom_index = 0;
            int next = hm2_sserial_get_bytes(hm2, &chan, buffer, 0, mode);
            assert(next > 0 && next <= HM2_SSERIAL_MAX_STRING_LENGTH);
            assert(memchr(buffer, 0, sizeof(buffer)) != NULL);
            if (length) assert(buffer[0] == (mode == -1 ? 'a' : 'A'));
        }
    }
}
static void test_cleanup(hostmot2_t *hm2) {
    hm2_sserial_cleanup(hm2);
    hm2_sserial_instance_t inst[3] = {0};
    hm2->sserial.instance = inst; hm2->sserial.num_instances = 3;
    for (unsigned i = 0; i < 2; i++) {
        inst[i].command_reg_addr = 0x100 + i * 4;
        inst[i].num_remotes = 2;
        inst[i].remotes = calloc(2, sizeof(*inst[i].remotes));
        assert(inst[i].remotes);
        for (unsigned r = 0; r < 2; r++) {
            hm2_sserial_remote_t *remote = &inst[i].remotes[r];
            remote->confs = calloc(1, sizeof(*remote->confs));
            remote->modes = calloc(1, sizeof(*remote->modes));
            remote->globals = calloc(1, sizeof(*remote->globals));
            /* Also cover allocated pointers whose counts are still zero. */
            remote->num_confs = remote->num_modes = remote->num_globals = r;
        }
    }
    stop_writes = 0;
    hm2_sserial_cleanup(hm2);
    assert(stop_writes == 2 && hm2->sserial.num_instances == 0);
    for (unsigned i = 0; i < 3; i++) assert(!inst[i].remotes && !inst[i].num_remotes);
    hm2_sserial_cleanup(hm2);
    assert(stop_writes == 2);
    hm2->sserial.instance = NULL;
}
static void compare_param(hostmot2_t *hm2, unsigned bits, double current, double written, int changed) {
    rtapi_uint state2 = 1, state3 = 0;
    rtapi_real value = current;
    rtapi_u32 command = 0;
    hm2_sserial_data_t global = { .DataLength = bits };
    hm2_sserial_params_t param = { .type = LBP_FLOAT, .float_written = written };
    param.param.r = (hal_real_t)&value;
    hm2_sserial_remote_t remote = { .num_globals = 1, .globals = &global, .params = &param };
    hm2_sserial_instance_t inst = { .num_remotes = 1, .remotes = &remote,
        .state2 = (hal_uint_t)&state2, .state3 = (hal_uint_t)&state3, .command_reg_write = &command };
    hm2_sserial_update_params(hm2, &inst, 1000000);
    assert(state2 == (changed ? 1 : 2));
    assert(state3 == (changed ? 1 : 0));
    assert(command == (changed ? 0x800 : 0));
    if (bits != 32 && bits != 64) assert(param.type == LBP_PAD);
}
static void check_nan_cycles(hostmot2_t *hm2, unsigned bits, double written, unsigned expected_writes) {
    rtapi_uint state2 = 0, state3 = 0;
    rtapi_sint debug = 0;
    rtapi_real value = NAN;
    rtapi_u32 command = 0, command_read = 0, data_read = 0, cs = 0, payload[2] = {0};
    hm2_sserial_data_t global = { .DataLength = bits };
    hm2_sserial_params_t param = { .type = LBP_FLOAT, .float_written = written };
    param.param.r = (hal_real_t)&value;
    hm2_sserial_remote_t remote = { .num_globals = 1, .globals = &global,
        .params = &param, .reg_cs_write = &cs, .write = {&payload[0], &payload[1]} };
    hm2_sserial_instance_t inst = { .num_remotes = 1, .remotes = &remote,
        .state2 = (hal_uint_t)&state2, .state3 = (hal_uint_t)&state3,
        .debug = (hal_sint_t)&debug, .command_reg_write = &command,
        .command_reg_read = &command_read, .data_reg_read = &data_read };
    unsigned writes = 0, stops = 0;
    for (unsigned n = 0; n < 100; n++) {
        command = 0;
        hm2_sserial_update_params(hm2, &inst, 1000000);
        if (command == 0x1001) writes++;
        if (command == 0x800) stops++;
    }
    assert(writes == expected_writes && stops == 2 * expected_writes);
}
int main(void) {
    hm2_lowlevel_io_t llio = { .read = read_word, .write = write_word };
    hostmot2_t hm2 = { .llio = &llio };
    test_formats(); test_strings(&hm2); test_cleanup(&hm2);
    compare_param(&hm2, 32, 0.1, (double)(float)0.1, 0);
    compare_param(&hm2, 32, 1.25, 1.0, 1);
    compare_param(&hm2, 64, 0.1, (double)(float)0.1, 1);
    compare_param(&hm2, 64, -1.25, -1.25, 0);
    compare_param(&hm2, 64, 0.0, -0.0, 0);
    compare_param(&hm2, 32, INFINITY, INFINITY, 0);
    compare_param(&hm2, 32, NAN, NAN, 0);
    compare_param(&hm2, 64, NAN, NAN, 0);
    compare_param(&hm2, 32, NAN, 1.0, 1);
    compare_param(&hm2, 64, 1.0, NAN, 1);
    compare_param(&hm2, 8, 1.0, 2.0, 0);
    compare_param(&hm2, 16, 1.0, 2.0, 0);
    check_nan_cycles(&hm2, 32, NAN, 0);
    check_nan_cycles(&hm2, 64, NAN, 0);
    check_nan_cycles(&hm2, 32, 1.0, 1);
    check_nan_cycles(&hm2, 64, 1.0, 1);
    puts("PASS: format/name bounds, allocation failure, H encoding, 130 string cases, repeat cleanup, 12 float cases");
    test_discovery();
    puts("PASS: Smart Serial and Smart Serial B discovery preserves active pins and releases unused ports to GPIO");
    return 0;
}
