// Package cmodtogomod tests cmod→gomod calls: C code calling Go callbacks.
// This uses //export to make Go functions callable from C.
package cmodtogomod

/*
#include <stdint.h>
#include <stdlib.h>

// Forward declarations of exported Go functions.
// These are implemented in Go with //export directives.
// Note: cgo exports use non-const char*, so we match that.
extern int32_t go_item_count(void);
extern int32_t go_item_get(char *name, double *value_out);
extern int32_t go_item_set(char *name, double value);

// C wrapper that simulates a cmod calling into gomod.
// Returns: 0 on success, -1 on error.
static int32_t cmod_test_roundtrip(const char *name, double set_value, double *get_value) {
    // Step 1: Set value (cmod → gomod)
    int32_t rc = go_item_set((char*)name, set_value);
    if (rc != 0) return rc;

    // Step 2: Get value back (cmod → gomod)
    rc = go_item_get((char*)name, get_value);
    return rc;
}

static int32_t cmod_get_count(void) {
    return go_item_count();
}
*/
import "C"
import (
	"sync"
	"unsafe"
)

// ─── Go Implementation (gomod) ───

var (
	itemsMu sync.RWMutex
	items   = make(map[string]float64)
)

// ResetItems clears the item store (for test isolation).
func ResetItems() {
	itemsMu.Lock()
	items = make(map[string]float64)
	itemsMu.Unlock()
}

// SetItemDirect sets an item directly from Go (for test setup).
func SetItemDirect(name string, value float64) {
	itemsMu.Lock()
	items[name] = value
	itemsMu.Unlock()
}

// GetItemDirect gets an item directly from Go (for test verification).
func GetItemDirect(name string) (float64, bool) {
	itemsMu.RLock()
	v, ok := items[name]
	itemsMu.RUnlock()
	return v, ok
}

// ItemCount returns the number of items (for test verification).
func ItemCount() int {
	itemsMu.RLock()
	n := len(items)
	itemsMu.RUnlock()
	return n
}

// ─── Exported Go Functions (callable from C) ───

//export go_item_count
func go_item_count() C.int32_t {
	itemsMu.RLock()
	n := len(items)
	itemsMu.RUnlock()
	return C.int32_t(n)
}

//export go_item_get
func go_item_get(name *C.char, valueOut *C.double) C.int32_t {
	goName := C.GoString(name)
	itemsMu.RLock()
	v, ok := items[goName]
	itemsMu.RUnlock()
	if !ok {
		return -2 // ENOENT
	}
	*valueOut = C.double(v)
	return 0
}

//export go_item_set
func go_item_set(name *C.char, value C.double) C.int32_t {
	goName := C.GoString(name)
	itemsMu.Lock()
	items[goName] = float64(value)
	itemsMu.Unlock()
	return 0
}

// ─── C Wrapper Calls (for tests) ───

// CmodTestRoundtrip calls the C function that calls back into Go.
// This simulates: cmod code → Go callbacks → return to cmod.
func CmodTestRoundtrip(name string, setValue float64) (float64, int) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	var getValue C.double
	rc := C.cmod_test_roundtrip(cName, C.double(setValue), &getValue)
	return float64(getValue), int(rc)
}

// CmodGetCount calls C which calls go_item_count().
func CmodGetCount() int {
	return int(C.cmod_get_count())
}
