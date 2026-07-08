// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

// In-memory parameter-I/O backend for the interpreter, used by integration
// tests so the real rs274ngc interpreter can run inside `go test` without a
// persist backend or a .var file. restore() returns a zeroed parameter table
// (with #5220 = 1 so G54 is the active coordinate system); save() discards.
//
// This lives in a non-test file because cgo //export must, but it is only
// referenced from tests (newInMemoryParamIO is otherwise dead code).

// #cgo CFLAGS: -I${SRCDIR}/../../.. -I${SRCDIR}/../../../../include
// #include "emc/rs274ngc/interp_parameter_io.hh"
// #include "interp_shim.h"
//
// extern int gomcParamRestore(void *ctx, double *params);
// extern int gomcParamSave(void *ctx, double *params, int *required);
//
// static void gomc_fill_mem_param_io(interp_param_io_t *io) {
//     io->restore = (int (*)(void *, double *))gomcParamRestore;
//     io->save    = (int (*)(void *, const double *, const int *))gomcParamSave;
//     io->ctx     = 0;
// }
import "C"
import "unsafe"

const interpParamMax = 5602

//export gomcParamRestore
func gomcParamRestore(ctx unsafe.Pointer, params *C.double) C.int {
	arr := unsafe.Slice(params, interpParamMax)
	for i := range arr {
		arr[i] = 0
	}
	arr[5220] = 1 // active coordinate system = G54
	return 0
}

//export gomcParamSave
func gomcParamSave(ctx unsafe.Pointer, params *C.double, required *C.int) C.int {
	return 0 // in-memory: nothing to persist
}

// inMemoryParamIO holds the C param-IO struct wired to the Go callbacks.
type inMemoryParamIO struct {
	io C.interp_param_io_t
}

func newInMemoryParamIO() *inMemoryParamIO {
	p := &inMemoryParamIO{}
	C.gomc_fill_mem_param_io(&p.io)
	return p
}

// install sets the param IO on the interpreter (must be called before Init).
func (p *inMemoryParamIO) install(interp *CInterp) {
	C.interp_set_param_io(interp.handle, &p.io)
}
