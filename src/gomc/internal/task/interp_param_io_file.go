// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

// File-based parameter-I/O backend for integration tests: the interpreter reads
// (and, on save, writes) a .var file. Tests point this at a throwaway copy in a
// temp dir so the committed fixture stays read-only. Pure cgo (no Go callbacks),
// It is only referenced from tests.

// #cgo CFLAGS: -I${SRCDIR}/../../.. -I${SRCDIR}/../../../../include
// #include <stdlib.h>
// #include "emc/rs274ngc/interp_parameter_io.hh"
// #include "interp_shim.h"
// extern interp_param_io_t interp_param_io_file_create(const char *filename);
// extern void interp_param_io_file_destroy(interp_param_io_t *io);
import "C"
import "unsafe"

type interpParamIOFile struct {
	io C.interp_param_io_t
}

func newInterpParamIOFile(filename string) *interpParamIOFile {
	cs := C.CString(filename)
	defer C.free(unsafe.Pointer(cs))
	return &interpParamIOFile{io: C.interp_param_io_file_create(cs)}
}

func (p *interpParamIOFile) install(interp *CInterp) { C.interp_set_param_io(interp.handle, &p.io) }
func (p *interpParamIOFile) destroy()                { C.interp_param_io_file_destroy(&p.io) }
