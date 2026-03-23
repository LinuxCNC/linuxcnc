// Package emcsvr provides a cgo wrapper for the in-process NML server
// (libemcsvr.so).  It replaces the former linuxcncsvr external subprocess.
package emcsvr

/*
#cgo CFLAGS: -I${SRCDIR}/../../../emc/task
#cgo LDFLAGS: -L${SRCDIR}/../../../../lib -lemcsvr -lnml -llinuxcncini -llinuxcnc
#include <stdlib.h>
#include "emcsvr_lib.h"
*/
import "C"

import (
	"fmt"
	"sync"
	"unsafe"
)

var cleanupOnce sync.Once

// Init parses the INI file and creates the NML channels.
// Must be called once before Run.
func Init(iniFile string) error {
	cs := C.CString(iniFile)
	defer C.free(unsafe.Pointer(cs))
	if ret := C.emcsvr_init(cs); ret != 0 {
		return fmt.Errorf("emcsvr_init returned %d", ret)
	}
	return nil
}

// Run blocks in the NML server loop until Stop is called.
// It should be called in a dedicated goroutine.
func Run() error {
	if ret := C.emcsvr_run(); ret != 0 {
		return fmt.Errorf("emcsvr_run returned %d", ret)
	}
	return nil
}

// Stop requests the NML server loop to exit by setting the shutdown flag.
func Stop() {
	C.emcsvr_stop()
}

// Cleanup deletes the NML channel objects and cleans up server state.
// Idempotent — safe to call more than once.
func Cleanup() {
	cleanupOnce.Do(func() {
		C.emcsvr_cleanup()
	})
}
