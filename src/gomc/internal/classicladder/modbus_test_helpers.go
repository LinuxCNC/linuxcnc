// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package classicladder

/*
#include "classicladder_rt.h"
*/
import "C"
import (
	"log/slog"
	"os"
	"testing"
)

// newTestModule creates a classicladder instance with a minimal RT for testing.
// The RT is freed when the test completes.
func newTestModule(t *testing.T) *classicladder {
	t.Helper()
	sizes := C.cl_sizes_t{
		nbr_rungs:        10,
		nbr_bits:         100,
		nbr_words:        100,
		nbr_timers:       10,
		nbr_monostables:  10,
		nbr_counters:     10,
		nbr_timers_iec:   10,
		nbr_phys_inputs:  15,
		nbr_phys_outputs: 15,
		nbr_arithm_expr:  10,
		nbr_sections:     10,
		nbr_symbols:      10,
		nbr_s32_in:       10,
		nbr_s32_out:      10,
		nbr_float_in:     10,
		nbr_float_out:    10,
		nbr_error_bits:   10,
	}
	rt := C.classicladder_rt_alloc(&sizes)
	if rt == nil {
		t.Fatal("failed to allocate RT instance")
	}
	C.classicladder_rt_init_data(rt)
	t.Cleanup(func() { C.classicladder_rt_free(rt) })

	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelWarn}))
	return &classicladder{
		logger:      logger,
		rt:          rt,
		modbus:      newModbusMaster(rt, logger),
		modbusSlave: newModbusSlave(rt, logger),
	}
}
