// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

// Go-side provider for the mcode_handler GMI API. The IDL (gmi/idl/mcode_handler.gmi)
// specifies "Provider: milltask.so — registers this API in New()", but the API was
// generated C-only (--server-c/--server-meta), so no provider bridge existed and no
// cmod could ever register an M-code handler. This wires it: a cmod calls
// mcode_handler_api_get(env->api, "<milltask>") + register_handler(mcode, fn, ud),
// which lands in goMcodeRegisterHandler below and installs a wrapper in the Go
// mcodeHandler registry. When the interpreter hits M100-M199, the sequencer submits
// the call and the wrapper invokes the cmod's C handler.
//
// Hand-written (not gmicompile --server-go) because that generator currently
// mis-types register_handler's callback (handler) and ptr (user_data) params as
// C.int, truncating the pointers.

/*
#cgo CFLAGS: -I${SRCDIR}/../../generated/gmi/mcode_handler -I${SRCDIR}/../../pkg/cmodule
#define MCODE_HANDLER_API_CGO
#include "mcode_handler_api.h"
#include <stdlib.h>

// C -> Go trampoline for register_handler (defined in Go via //export).
extern int32_t goMcodeRegisterHandler(void *ctx, int32_t mcode,
                                      mcode_handler_handler_cb fn, void *user_data);

static mcode_handler_callbacks_t *build_mcode_provider(uintptr_t ctx) {
    mcode_handler_callbacks_t *cbs = calloc(1, sizeof(*cbs));
    if (!cbs)
        return NULL;
    cbs->ctx = (void *)ctx;
    cbs->register_handler = goMcodeRegisterHandler;
    return cbs;
}

// Go -> C: invoke a registered handler with a freshly built call struct.
static int32_t invoke_mcode_handler(mcode_handler_handler_cb fn, void *user_data,
                                    int32_t abort_fd, int32_t mcode, double p, double q) {
    mcode_handler_mcode_call_t call;
    call.abort_fd = abort_fd;
    call.mcode = mcode;
    call.p_number = p;
    call.q_number = q;
    return fn(&call, user_data);
}
*/
import "C"

import (
	"errors"
	"os"
	"runtime/cgo"
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
)

var errMcodeProviderAlloc = errors.New("mcode_handler: callbacks allocation failed")

// registerMcodeHandlerProvider registers the mcode_handler GMI API under the given
// milltask instance name, backed by the mcodeHandler registry h. Returns a cleanup
// func that unregisters and frees the C callbacks struct.
func registerMcodeHandlerProvider(reg *apiserver.Registry, instance string, h *mcodeHandler) (func(), error) {
	handle := cgo.NewHandle(h)
	cbs := C.build_mcode_provider(C.uintptr_t(handle))
	if cbs == nil {
		handle.Delete()
		return nil, errMcodeProviderAlloc
	}
	if err := reg.Register("mcode_handler", 1, instance, unsafe.Pointer(cbs)); err != nil {
		handle.Delete()
		C.free(unsafe.Pointer(cbs))
		return nil, err
	}
	return func() {
		handle.Delete()
		C.free(unsafe.Pointer(cbs))
	}, nil
}

//export goMcodeRegisterHandler
func goMcodeRegisterHandler(ctx unsafe.Pointer, mcode C.int32_t,
	fn C.mcode_handler_handler_cb, userData unsafe.Pointer) C.int32_t {
	h := cgo.Handle(uintptr(ctx)).Value().(*mcodeHandler)

	cFn := fn
	cUser := userData
	wrapper := func(call *McodeCall) int {
		// Bridge the Go abort channel to a pollable fd for the C handler.
		rfd, wfd, err := os.Pipe()
		if err != nil {
			return -1
		}
		defer rfd.Close()
		stop := make(chan struct{})
		go func() {
			select {
			case <-call.abortCh:
				_, _ = wfd.Write([]byte{1})
			case <-stop:
			}
			_ = wfd.Close()
		}()
		rc := C.invoke_mcode_handler(cFn, cUser, C.int32_t(rfd.Fd()),
			C.int32_t(call.Mcode), C.double(call.P), C.double(call.Q))
		close(stop)
		return int(rc)
	}

	if err := h.RegisterHandler(int(mcode), wrapper); err != nil {
		return -1
	}
	return 0
}
