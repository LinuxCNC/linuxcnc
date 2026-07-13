// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

// Go-side provider for the interp_ext GMI API. The IDL declares milltask.so as
// the provider, and stdglue.c / cmods look the API up via
// interp_ext_api_get(env->api, "<milltask>") in their Start(), but nothing ever
// registered the provider — so register_oword / register_remap_prolog /
// _remap_epilog were unreachable (C stdglue remaps and C O-word subs never
// worked). This wires it: register the API under the milltask instance with
// ctx = the interpreter's raw Interp*, and the callback fields pointing at the
// librs274 C routing functions, which static_cast ctx back to Interp* and call
// the matching Interp::ext_register_* method.
//
// No Go trampoline is needed (unlike mcode_handler): the registered handlers are
// invoked directly by the C++ interpreter, not from Go.

/*
#cgo CFLAGS: -I${SRCDIR}/../../generated/gmi/interp_ext -I${SRCDIR}/../../generated/gmi/interp_ctx
#define INTERP_EXT_API_CGO
#include "interp_ext_api.h"
#include <stdlib.h>

// Routing functions exported from librs274 (interp_ext.cc). Their signatures
// match the interp_ext_callbacks_t field types (int vs int32_t is the same
// width here; cast to silence the pointer-type check).
extern int interp_ext_register_oword(void *interp, const char *name,
                                     interp_ext_oword_fn_cb fn, void *user);
extern int interp_ext_register_remap_prolog(void *interp, const char *name,
                                            interp_ext_remap_prolog_fn_cb fn, void *user);
extern int interp_ext_register_remap_epilog(void *interp, const char *name,
                                            interp_ext_remap_epilog_fn_cb fn, void *user);

static interp_ext_callbacks_t *build_interp_ext_provider(void *interp) {
    interp_ext_callbacks_t *cbs = calloc(1, sizeof(*cbs));
    if (!cbs)
        return NULL;
    cbs->ctx = interp;
    cbs->register_oword = (interp_ext_register_oword_fn)interp_ext_register_oword;
    cbs->register_remap_prolog =
        (interp_ext_register_remap_prolog_fn)interp_ext_register_remap_prolog;
    cbs->register_remap_epilog =
        (interp_ext_register_remap_epilog_fn)interp_ext_register_remap_epilog;
    return cbs;
}
*/
import "C"

import (
	"errors"
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
)

var errInterpExtProviderAlloc = errors.New("interp_ext: callbacks allocation failed")

// registerInterpExtProvider registers the interp_ext GMI API under the given
// milltask instance, backed by the interpreter handle interp (a raw Interp*).
// Returns a cleanup func that unregisters and frees the C callbacks struct.
func registerInterpExtProvider(reg *apiserver.Registry, instance string, interp unsafe.Pointer) (func(), error) {
	cbs := C.build_interp_ext_provider(interp)
	if cbs == nil {
		return nil, errInterpExtProviderAlloc
	}
	if err := reg.Register("interp_ext", 1, instance, unsafe.Pointer(cbs)); err != nil {
		C.free(unsafe.Pointer(cbs))
		return nil, err
	}
	return func() {
		C.free(unsafe.Pointer(cbs))
	}, nil
}
