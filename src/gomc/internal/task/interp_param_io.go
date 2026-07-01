// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

// #cgo CFLAGS: -I${SRCDIR}/../../.. -I${SRCDIR}/../../../../include -I${SRCDIR}/../../generated/gmi/persist
// #include "emc/rs274ngc/interp_parameter_io.hh"
// #include "interp_shim.h"
//
// extern interp_param_io_t interp_param_io_persist_create(const void *persist);
// extern void interp_param_io_persist_destroy(interp_param_io_t *io);
import "C"
import "unsafe"

// interpParamIOPersist holds the C-allocated param IO struct so we can
// destroy it on shutdown.
type interpParamIOPersist struct {
	io C.interp_param_io_t
}

// newInterpParamIOPersist creates a persist-backed parameter IO and
// installs it on the interpreter. The persist pointer comes from
// reg.GetAPIFor("milltask", "persist", instance, 2).
func newInterpParamIOPersist(persistCbs unsafe.Pointer) *interpParamIOPersist {
	p := &interpParamIOPersist{
		io: C.interp_param_io_persist_create(persistCbs),
	}
	return p
}

// install sets the param IO on the interpreter (must be called before Init).
func (p *interpParamIOPersist) install(interp *CInterp) {
	C.interp_set_param_io(interp.handle, &p.io)
}

// destroy frees the C-allocated context.
func (p *interpParamIOPersist) destroy() {
	C.interp_param_io_persist_destroy(&p.io)
}
