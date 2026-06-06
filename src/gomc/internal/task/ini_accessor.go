package task

// INI accessor bridge: provides C callback functions backed by a Go *inifile.IniFile.
// The callbacks are called by the C++ interpreter for INI lookups at init time
// and at runtime (#<_ini[SECTION]KEY> named parameters).

/*
#include "interp_shim.h"
#include <stdlib.h>
#include <string.h>

// Forward declarations for the Go-exported callbacks.
// cgo does not emit const qualifiers, so we declare without const here
// and cast when assigning to the accessor struct.
extern char* goIniAccessorGet(void *ctx, char *section, char *key);
extern char* goIniAccessorGetNth(void *ctx, char *section, char *key, int n);

// Build the accessor struct with Go-implemented callbacks.
static inline interp_ini_accessor_t make_ini_accessor(void *ctx) {
    interp_ini_accessor_t acc;
    acc.ctx = ctx;
    acc.get = (const char* (*)(void*, const char*, const char*))goIniAccessorGet;
    acc.get_nth = (const char* (*)(void*, const char*, const char*, int))goIniAccessorGetNth;
    return acc;
}
*/
import "C"
import (
	"fmt"
	"runtime/cgo"
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

// iniAccessorHandle wraps a *inifile.IniFile for use as the ctx in C callbacks.
// It also holds a reusable buffer for returning strings to C (the C code
// must consume the string before the next call to get/get_nth).
// iniAccessorHandle wraps a *inifile.IniFile for use as the ctx in C callbacks.
// It holds a C-allocated buffer for returning strings (the C code must consume
// the string before the next call to get/get_nth).
type iniAccessorHandle struct {
	ini    *inifile.IniFile
	cbuf   *C.char // C-heap buffer for string returns
	cbufSz C.size_t
}

func (h *iniAccessorHandle) free() {
	if h.cbuf != nil {
		C.free(unsafe.Pointer(h.cbuf))
		h.cbuf = nil
	}
}

// returnStr copies a Go string into the C-heap buffer and returns it.
func (h *iniAccessorHandle) returnStr(s string) *C.char {
	need := C.size_t(len(s) + 1)
	if need > h.cbufSz {
		if h.cbuf != nil {
			C.free(unsafe.Pointer(h.cbuf))
		}
		h.cbuf = (*C.char)(C.malloc(need))
		h.cbufSz = need
	}
	C.memcpy(unsafe.Pointer(h.cbuf), unsafe.Pointer(unsafe.StringData(s)), C.size_t(len(s)))
	*(*C.char)(unsafe.Pointer(uintptr(unsafe.Pointer(h.cbuf)) + uintptr(len(s)))) = 0
	return h.cbuf
}

// newIniAccessor creates a C interp_ini_accessor_t backed by the given IniFile.
// The returned cgo.Handle must be kept alive and eventually deleted.
func newIniAccessor(ini *inifile.IniFile) (C.interp_ini_accessor_t, cgo.Handle) {
	h := &iniAccessorHandle{ini: ini}
	handle := cgo.NewHandle(h)
	acc := C.make_ini_accessor(unsafe.Pointer(uintptr(handle)))
	return acc, handle
}

//export goIniAccessorGet
func goIniAccessorGet(ctx unsafe.Pointer, section *C.char, key *C.char) *C.char {
	h := cgo.Handle(uintptr(ctx)).Value().(*iniAccessorHandle)
	goSec := C.GoString(section)
	goKey := C.GoString(key)
	val := h.ini.Get(goSec, goKey)
	if val == "" {
		return nil
	}
	return h.returnStr(val)
}

//export goIniAccessorGetNth
func goIniAccessorGetNth(ctx unsafe.Pointer, section *C.char, key *C.char, n C.int) *C.char {
	h := cgo.Handle(uintptr(ctx)).Value().(*iniAccessorHandle)
	goSec := C.GoString(section)
	goKey := C.GoString(key)
	val := h.ini.GetN(goSec, goKey, int(n))
	if val == "" {
		return nil
	}
	return h.returnStr(val)
}

// IniLoadAccessor loads INI config into the interpreter using the accessor
// callbacks backed by the given IniFile. This replaces IniLoad(filename).
func (i *CInterp) IniLoadAccessor(ini *inifile.IniFile) (cgo.Handle, error) {
	acc, handle := newIniAccessor(ini)
	rc := int(C.interp_ini_load_accessor(i.handle, &acc))
	if rc != 0 {
		handle.Delete()
		return 0, fmt.Errorf("interp_ini_load_accessor: rc=%d", rc)
	}
	return handle, nil
}

// SetIniAccessor sets the INI accessor for runtime lookups without replacing
// the ini_load step.  Call this if ini_load was done by file path but you want
// runtime #<_ini[SEC]KEY> to use the Go accessor.
func (i *CInterp) SetIniAccessor(ini *inifile.IniFile) cgo.Handle {
	acc, handle := newIniAccessor(ini)
	C.interp_set_ini_accessor(i.handle, &acc)
	return handle
}

// FreeIniAccessor releases the C buffer and deletes the cgo handle.
func FreeIniAccessor(handle cgo.Handle) {
	if handle == 0 {
		return
	}
	h := handle.Value().(*iniAccessorHandle)
	h.free()
	handle.Delete()
}
