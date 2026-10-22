#include <assert.h>
#include <stdio.h>
#include "hostmot2.h"

static unsigned errors, writes;

void rtapi_print_msg(msg_level_t level, const char *fmt, ...) {
    (void)fmt;
    if (level == RTAPI_MSG_ERR) errors++;
}

static int write_word(hm2_lowlevel_io_t *io, rtapi_u32 addr, const void *data, int size) {
    (void)io;
    (void)addr;
    (void)data;
    assert(size == sizeof(rtapi_u32));
    writes++;
    return 1;
}

static void test_rate(int mux, int force, unsigned clock, unsigned width, unsigned requested) {
    rtapi_uint rate = requested, fast = 1, slow = 1, off = 0;
    rtapi_u32 control = 0, mpg = 0, filter = 0;
    hm2_lowlevel_io_t llio = {.write = write_word};
    hm2_inm_instance_t inm = {.scanwidth = width};
    hm2_inmux_instance_t inmux = {.scanwidth = width};
    hostmot2_t board = {.llio = &llio};
    board.inm.num_instances = board.inmux.num_instances = 1;
    board.inm.clock_frequency = board.inmux.clock_frequency = clock;
    board.inm.instance = &inm;
    board.inmux.instance = &inmux;
    board.inm.control_reg = board.inmux.control_reg = &control;
    board.inm.mpg_mode_reg = board.inmux.mpg_mode_reg = &mpg;
    board.inm.filter_reg = board.inmux.filter_reg = &filter;
    inm.hal.param.scan_rate = inmux.hal.param.scan_rate = (hal_uint_t)&rate;
    inm.hal.param.fast_scans = inmux.hal.param.fast_scans = (hal_uint_t)&fast;
    inm.hal.param.slow_scans = inmux.hal.param.slow_scans = (hal_uint_t)&slow;
    inmux.hal.param.enc0_mode = inmux.hal.param.enc1_mode =
        inmux.hal.param.enc2_mode = inmux.hal.param.enc3_mode = (hal_bool_t)&off;
    for (unsigned i = 0; i < 32; i++) inmux.hal.pin.slow[i] = (hal_bool_t)&off;

    void (*write)(hostmot2_t *) = mux ? hm2_inmux_write : hm2_inm_write;
    void (*force_write)(hostmot2_t *) = mux ? hm2_inmux_force_write : hm2_inm_force_write;
    unsigned minimum = (clock + 4096u * width - 1) / (4096u * width);
    unsigned expected = requested <= 1 ? minimum : requested;
    unsigned divisor = requested <= 1 ? 1023 : clock / (4u * width * requested) - 1;
    errors = writes = 0;
    (force ? force_write : write)(&board);
    assert(hal_get_ui32(inm.hal.param.scan_rate) == expected);
    assert(control == ((1u << 5) | (divisor << 6) | (1u << 16) | (1u << 22)));
    assert(errors == (!force && requested <= 1 ? 1u : 0u));

    // A normalized rate must stay in range without repeated diagnostics or writes.
    errors = 0;
    write(&board);
    divisor = clock / (4u * width * expected) - 1;
    assert(divisor <= 1023);
    assert(control == ((1u << 5) | (divisor << 6) | (1u << 16) | (1u << 22)));
    unsigned settled_writes = writes;
    for (unsigned cycle = 0; cycle < 5; cycle++) {
        write(&board);
        assert(hal_get_ui32(inm.hal.param.scan_rate) == expected);
        assert(errors == 0 && writes == settled_writes);
    }
}

int main(void) {
    const unsigned clocks[] = {32768000, 50000000, 100000000};
    for (int mux = 0; mux <= 1; mux++) {
        for (int force = 0; force <= 1; force++) {
            for (unsigned c = 0; c < sizeof(clocks) / sizeof(clocks[0]); c++) {
                for (unsigned width = 1; width <= 32; width++) {
                    unsigned minimum = (clocks[c] + 4096u * width - 1) / (4096u * width);
                    const unsigned rates[] = {0, 1, minimum, 50000};
                    for (unsigned r = 0; r < sizeof(rates) / sizeof(rates[0]); r++)
                        test_rate(mux, force, clocks[c], width, rates[r]);
                }
            }
        }
    }
    puts("PASS: INM/INMUX scan-rate bounds, startup/runtime clamps and repeated cycles for widths 1..32");
    return 0;
}
