package hal

/*
#cgo CFLAGS: -I${SRCDIR}/.. -I${SRCDIR}/../../rtapi -I${SRCDIR}/../../../include -DULAPI
#cgo LDFLAGS: -L${SRCDIR}/../../../lib -llinuxcnchal

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include "config.h"
#include "hal.h"
#include "../hal_priv.h"

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

// hal_shim_list_comps walks the HAL component list and writes each component
// name as a null-terminated string into buf (strings are concatenated).
// Returns the number of components found, or a negative errno value on error.
// Returns -ENOSPC if the buffer is too small to hold all names.
static int hal_shim_list_comps(char *buf, int buf_size) {
    int next;
    hal_comp_t *comp;
    int count = 0;
    int pos = 0;
    int name_len;

    if (hal_data == NULL) {
        return -EINVAL;
    }

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
        comp = (hal_comp_t *)SHMPTR(next);
        name_len = (int)strlen(comp->name) + 1; // include null terminator
        if (pos + name_len > buf_size) {
            rtapi_mutex_give(&(hal_data->mutex));
            return -ENOSPC;
        }
        memcpy(buf + pos, comp->name, name_len);
        pos += name_len;
        count++;
        next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return count;
}

// HAL_SHIM_MAX_COMPS is the maximum number of components tracked by the shim
// helpers. 256 exceeds any realistic LinuxCNC machine configuration.
#define HAL_SHIM_MAX_COMPS 256

// shim_systemv forks and execs argv[0] with the given argument vector,
// waits for it to exit, and returns its exit status.
// This replicates what hal_systemv() does in halcmd_commands.cc, but
// hal_systemv() is not part of liblinuxcnchal so we implement it inline.
static int shim_systemv(const char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

// hal_shim_unload_all unloads all HAL components, exactly as halcmd's
// "unload all" command does:
//   - Userspace components: send SIGTERM to their owning process
//   - Realtime components: call "rtapi_app unload <name>" via shim_systemv
// The component identified by except_id is skipped (pass 0 to not skip any).
// Returns 0 on success, or a negative errno value on error.
static int hal_shim_unload_all(int except_id) {
    int next;
    hal_comp_t *comp;
    pid_t ourpid = getpid();

    if (hal_data == NULL) {
        return -EINVAL;
    }

    // Phase 1: send SIGTERM to userspace components (same as do_unloadusr_cmd)
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
        comp = (hal_comp_t *)SHMPTR(next);
        if (comp->type == COMPONENT_TYPE_USER && comp->pid != ourpid) {
            if (comp->comp_id != except_id) {
                kill(abs(comp->pid), SIGTERM);
            }
        }
        next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));

    // Phase 2: collect realtime component names then unload via rtapi_app
    // (same as do_unloadrt_cmd)
    {
        char comps[HAL_SHIM_MAX_COMPS][HAL_NAME_LEN+1];
        int n = 0;
        int i;

        rtapi_mutex_get(&(hal_data->mutex));
        next = hal_data->comp_list_ptr;
        while (next != 0) {
            comp = (hal_comp_t *)SHMPTR(next);
            if (comp->type == COMPONENT_TYPE_REALTIME) {
                if (comp->comp_id != except_id && n < HAL_SHIM_MAX_COMPS) {
                    // skip pseudo-components (names starting with "__")
                    if (strstr(comp->name, HAL_PSEUDO_COMP_PREFIX) != comp->name) {
                        snprintf(comps[n], sizeof(comps[n]), "%s", comp->name);
                        n++;
                    }
                }
            }
            next = comp->next_ptr;
        }
        rtapi_mutex_give(&(hal_data->mutex));

        // unload each realtime component via rtapi_app
        for (i = 0; i < n; i++) {
            const char *argv[4];
            argv[0] = EMC2_BIN_DIR "/rtapi_app";
            argv[1] = "unload";
            argv[2] = comps[i];
            argv[3] = NULL;
            shim_systemv(argv);
        }
    }

    return 0;
}
*/
import "C"
import (
	"bytes"
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

// halStartThreads wraps hal_start_threads() to start all HAL realtime threads.
func halStartThreads() error {
	ret := C.hal_start_threads()
	return halError(int(ret), "hal_start_threads")
}

// halStopThreads wraps hal_stop_threads() to stop all HAL realtime threads.
func halStopThreads() error {
	ret := C.hal_stop_threads()
	return halError(int(ret), "hal_stop_threads")
}

// halListComponents wraps hal_shim_list_comps() to return all HAL component names.
// Returns a slice of component name strings, or an error on failure.
func halListComponents() ([]string, error) {
	// Allocate a buffer sized for HAL_SHIM_MAX_COMPS components, each with a
	// name up to HAL_NAME_LEN+1 bytes (HAL_NAME_LEN is 127, defined in hal.h).
	bufSize := int(C.HAL_SHIM_MAX_COMPS) * (int(C.HAL_NAME_LEN) + 1)
	buf := make([]byte, bufSize)

	ret := C.hal_shim_list_comps((*C.char)(unsafe.Pointer(unsafe.SliceData(buf))), C.int(bufSize))
	if ret < 0 {
		return nil, halError(int(ret), "hal_shim_list_comps")
	}
	if ret == 0 {
		return []string{}, nil
	}

	// Scan only the portion of the buffer that was actually written.
	// The C function writes exactly `ret` null-terminated strings starting at
	// offset 0.  Find the end by counting null terminators.
	end := 0
	remaining := int(ret)
	for end < len(buf) && remaining > 0 {
		if buf[end] == 0 {
			remaining--
		}
		end++
	}

	// Split the written portion on null bytes and collect non-empty strings.
	parts := bytes.Split(buf[:end], []byte{0})
	names := make([]string, 0, int(ret))
	for _, p := range parts {
		if len(p) > 0 {
			names = append(names, string(p))
		}
	}
	return names, nil
}

// halUnloadAll wraps hal_shim_unload_all() to exit all HAL components except
// the one identified by exceptID.
func halUnloadAll(exceptID int) error {
	ret := C.hal_shim_unload_all(C.int(exceptID))
	return halError(int(ret), "hal_shim_unload_all")
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
		message = "general HAL error or operation not permitted (HAL may be locked)"
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
