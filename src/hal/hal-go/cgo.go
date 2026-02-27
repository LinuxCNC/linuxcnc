package hal

/*
#cgo CFLAGS: -I${SRCDIR}/.. -I${SRCDIR}/../../rtapi -I${SRCDIR}/../../../include -DULAPI
#cgo LDFLAGS: -L${SRCDIR}/../../../lib -llinuxcnchal

#include <stdlib.h>
#include "hal.h"

// Helper to convert hal_type_t to int for Go
static inline int get_hal_type(hal_type_t t) { return (int)t; }

// Helper to convert hal_pin_dir_t to int for Go
static inline int get_hal_dir(hal_pin_dir_t d) { return (int)d; }

// Port helper wrappers: take hal_port_t* and dereference for the C API.
static inline bool go_hal_port_write(hal_port_t* p, const char* src, unsigned count) {
    return hal_port_write(*p, src, count);
}
static inline bool go_hal_port_peek(hal_port_t* p, char* dest, unsigned count) {
    return hal_port_peek(*p, dest, count);
}
static inline unsigned go_hal_port_readable(hal_port_t* p) {
    return hal_port_readable(*p);
}
static inline void go_hal_port_clear(hal_port_t* p) {
    hal_port_clear(*p);
}
*/
import "C"
import (
	"fmt"
	"unsafe"
)

// halInit wraps hal_init() to create a new HAL component.
// Returns the component ID on success, or an error on failure.
func halInit(name string) (int, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	compID := C.hal_init(cName)
	if compID < 0 {
		return 0, halError(int(compID), "hal_init")
	}

	return int(compID), nil
}

// halReady wraps hal_ready() to mark a component as ready.
// Returns an error on failure.
func halReady(compID int) error {
	ret := C.hal_ready(C.int(compID))
	return halError(int(ret), "hal_ready")
}

// halExit wraps hal_exit() to clean up a component.
// Returns an error on failure.
func halExit(compID int) error {
	ret := C.hal_exit(C.int(compID))
	return halError(int(ret), "hal_exit")
}

// halMalloc wraps hal_malloc() to allocate memory in HAL shared memory.
// Returns an unsafe.Pointer to the allocated memory, or nil on failure.
// The allocated memory is freed when the component exits.
func halMalloc(size int) unsafe.Pointer {
	return C.hal_malloc(C.long(size))
}

// halPinBitNew wraps hal_pin_bit_new() to create a new bit (boolean) pin.
// Returns a pointer to the HAL shared memory for the pin value.
func halPinBitNew(name string, dir Direction, compID int) (*C.hal_bit_t, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// Allocate space for the pointer in HAL shared memory
	// hal_pin_bit_new expects hal_bit_t**, so we need sizeof(hal_bit_t *)
	ptrPtr := (**C.hal_bit_t)(halMalloc(int(unsafe.Sizeof((*C.hal_bit_t)(nil)))))
	if ptrPtr == nil {
		return nil, newError("hal_malloc", "failed to allocate HAL shared memory", -12)
	}

	// hal_pin_bit_new will set *ptrPtr to point to the actual data
	ret := C.hal_pin_bit_new(cName, C.hal_pin_dir_t(dir), ptrPtr, C.int(compID))
	if ret < 0 {
		return nil, halError(int(ret), "hal_pin_bit_new")
	}

	// Return the pointer to the actual pin data
	return *ptrPtr, nil
}

// halPinFloatNew wraps hal_pin_float_new() to create a new float pin.
// Returns a pointer to the HAL shared memory for the pin value.
func halPinFloatNew(name string, dir Direction, compID int) (*C.hal_float_t, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// Allocate space for the pointer in HAL shared memory
	// hal_pin_float_new expects hal_float_t**, so we need sizeof(hal_float_t *)
	ptrPtr := (**C.hal_float_t)(halMalloc(int(unsafe.Sizeof((*C.hal_float_t)(nil)))))
	if ptrPtr == nil {
		return nil, newError("hal_malloc", "failed to allocate HAL shared memory", -12)
	}

	// hal_pin_float_new will set *ptrPtr to point to the actual data
	ret := C.hal_pin_float_new(cName, C.hal_pin_dir_t(dir), ptrPtr, C.int(compID))
	if ret < 0 {
		return nil, halError(int(ret), "hal_pin_float_new")
	}

	// Return the pointer to the actual pin data
	return *ptrPtr, nil
}

// halPinS32New wraps hal_pin_s32_new() to create a new signed 32-bit integer pin.
// Returns a pointer to the HAL shared memory for the pin value.
func halPinS32New(name string, dir Direction, compID int) (*C.hal_s32_t, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// Allocate space for the pointer in HAL shared memory
	// hal_pin_s32_new expects hal_s32_t**, so we need sizeof(hal_s32_t *)
	ptrPtr := (**C.hal_s32_t)(halMalloc(int(unsafe.Sizeof((*C.hal_s32_t)(nil)))))
	if ptrPtr == nil {
		return nil, newError("hal_malloc", "failed to allocate HAL shared memory", -12)
	}

	// hal_pin_s32_new will set *ptrPtr to point to the actual data
	ret := C.hal_pin_s32_new(cName, C.hal_pin_dir_t(dir), ptrPtr, C.int(compID))
	if ret < 0 {
		return nil, halError(int(ret), "hal_pin_s32_new")
	}

	// Return the pointer to the actual pin data
	return *ptrPtr, nil
}

// halPinU32New wraps hal_pin_u32_new() to create a new unsigned 32-bit integer pin.
// Returns a pointer to the HAL shared memory for the pin value.
func halPinU32New(name string, dir Direction, compID int) (*C.hal_u32_t, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// Allocate space for the pointer in HAL shared memory
	// hal_pin_u32_new expects hal_u32_t**, so we need sizeof(hal_u32_t *)
	ptrPtr := (**C.hal_u32_t)(halMalloc(int(unsafe.Sizeof((*C.hal_u32_t)(nil)))))
	if ptrPtr == nil {
		return nil, newError("hal_malloc", "failed to allocate HAL shared memory", -12)
	}

	// hal_pin_u32_new will set *ptrPtr to point to the actual data
	ret := C.hal_pin_u32_new(cName, C.hal_pin_dir_t(dir), ptrPtr, C.int(compID))
	if ret < 0 {
		return nil, halError(int(ret), "hal_pin_u32_new")
	}

	// Return the pointer to the actual pin data
	return *ptrPtr, nil
}

// halPinPortNew wraps hal_pin_port_new() to create a new port pin.
// Returns the double-pointer (unsafe.Pointer to **hal_port_t) so the caller
// can dereference at access time. HAL updates *ptrPtr when the pin is linked
// to a signal via net, so the double-pointer must be preserved.
func halPinPortNew(name string, dir Direction, compID int) (unsafe.Pointer, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// Allocate space for the pointer in HAL shared memory
	// hal_pin_port_new expects hal_port_t**, so we need sizeof(hal_port_t *)
	ptrPtr := (**C.hal_port_t)(halMalloc(int(unsafe.Sizeof((*C.hal_port_t)(nil)))))
	if ptrPtr == nil {
		return nil, newError("hal_malloc", "failed to allocate HAL shared memory", -12)
	}

	// hal_pin_port_new will set *ptrPtr to point to the actual data
	ret := C.hal_pin_port_new(cName, C.hal_pin_dir_t(dir), ptrPtr, C.int(compID))
	if ret < 0 {
		return nil, halError(int(ret), "hal_pin_port_new")
	}

	// Return the double-pointer itself — the caller must dereference at access time
	// because HAL updates *ptrPtr when the pin is linked to a signal via net.
	return unsafe.Pointer(ptrPtr), nil
}

// halPortWrite writes data bytes to the port referenced by portPtr.
// Returns true if all bytes were written successfully.
func halPortWrite(portPtr *C.hal_port_t, data []byte) bool {
	if len(data) == 0 {
		// Nothing to write; avoid passing a nil/empty-slice pointer to the C function.
		return true
	}
	ret := C.go_hal_port_write(portPtr, (*C.char)(unsafe.Pointer(unsafe.SliceData(data))), C.uint(len(data)))
	return bool(ret)
}

// halPortPeek reads count bytes from the port without consuming them.
// Returns the bytes read, or nil if not enough data is available.
func halPortPeek(portPtr *C.hal_port_t, count uint) []byte {
	if count == 0 {
		return []byte{}
	}
	buf := make([]byte, count)
	ret := C.go_hal_port_peek(portPtr, (*C.char)(unsafe.Pointer(unsafe.SliceData(buf))), C.uint(count))
	if !bool(ret) {
		return nil
	}
	return buf
}

// halPortReadable returns the number of bytes available to read from the port.
func halPortReadable(portPtr *C.hal_port_t) uint {
	return uint(C.go_hal_port_readable(portPtr))
}

// halPortClear empties the port of all data.
func halPortClear(portPtr *C.hal_port_t) {
	C.go_hal_port_clear(portPtr)
}

// halError translates a HAL C error code to a Go error.
// Returns nil if the code is 0 (success).
func halError(code int, op string) error {
	if code == 0 {
		return nil
	}

	// Map common HAL error codes to meaningful messages
	// These are based on standard errno values and HAL conventions
	var message string
	switch code {
	case -1:
		message = "general HAL error"
	case -12: // -ENOMEM
		message = "insufficient HAL shared memory"
	case -16: // -EBUSY
		message = "resource busy or already in use"
	case -22: // -EINVAL
		message = "invalid argument or name"
	case -23: // -ENFILE
		message = "too many open files or components"
	case -28: // -ENOSPC
		message = "no space left in HAL shared memory"
	default:
		message = fmt.Sprintf("HAL error (code %d)", code)
	}

	return newError(op, message, code)
}
