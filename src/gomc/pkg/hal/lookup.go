package hal

/*
#include <stdlib.h>
#include "hal.h"
#include <stdint.h>

// Lookup a HAL pin/signal/param value by name.
// Returns 0 on success (value written to *val), -1 if not found.
static inline int go_hal_lookup_value(const char *name, double *val) {
    hal_type_t type = HAL_TYPE_UNINITIALIZED;
    hal_data_u *ptr;
    bool conn;

    if (hal_get_pin_value_by_name(name, &type, &ptr, &conn) == 0)
        goto found;
    if (hal_get_signal_value_by_name(name, &type, &ptr, &conn) == 0)
        goto found;
    if (hal_get_param_value_by_name(name, &type, &ptr) == 0)
        goto found;
    return -1;

found:
    switch (type) {
    case HAL_BIT:   *val = (double)ptr->b; return 0;
    case HAL_U32:   *val = (double)ptr->u; return 0;
    case HAL_S32:   *val = (double)ptr->s; return 0;
    case HAL_FLOAT: *val = (double)ptr->f; return 0;
    default:        return -1;
    }
}
*/
import "C"
import "unsafe"

// LookupValue looks up a HAL pin, signal, or param by name and returns its
// value as float64. Returns (value, true) if found, (0, false) if not.
func LookupValue(name string) (float64, bool) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	var val C.double
	rc := C.go_hal_lookup_value(cname, &val)
	if rc != 0 {
		return 0, false
	}
	return float64(val), true
}
