// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package classicladder

/*
#include "classicladder_rt.h"
#include <stdlib.h>
#include <string.h>
*/
import "C"

// evalCompiledForTest creates a minimal RT instance and evaluates a compiled expression.
// Used only in tests for pure-constant expressions.
func evalCompiledForTest(ce C.cl_compiled_expr_t) bool {
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
		return false
	}
	defer C.classicladder_rt_free(rt)

	// Place the compiled expression at index 0
	rt.compiled_exprs[0] = ce

	result := C.cl_eval_compare(rt, 0)
	return result != 0
}

// evalOperateForTest creates a minimal RT instance, runs an operate expression,
// and returns the value of MEM_WORD[offset].
func evalOperateForTest(ce C.cl_compiled_expr_t, offset int) int32 {
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
		return 0
	}
	defer C.classicladder_rt_free(rt)

	rt.compiled_exprs[0] = ce
	C.cl_eval_operate(rt, 0)

	return int32(rt.var_words[offset])
}

// newTestRT allocates a minimal RT instance for tests.
func newTestRT() *C.classicladder_rt_t {
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
	C.classicladder_rt_init_data(rt)
	return rt
}

// freeTestRT frees a test RT instance.
func freeTestRT(rt *C.classicladder_rt_t) {
	C.classicladder_rt_free(rt)
}

// testPrepareSequential wraps the C function.
func testPrepareSequential(rt *C.classicladder_rt_t) {
	C.cl_prepare_sequential(rt)
}

// testRefreshSequentialPage wraps the C function.
func testRefreshSequentialPage(rt *C.classicladder_rt_t, page int) {
	C.cl_refresh_sequential_page(rt, C.int(page))
}
