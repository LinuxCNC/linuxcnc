package task

// This file wires M-code handler registration from Go into the interpreter's
// user_defined_function slots, and provides the C callback that the interpreter
// calls when it encounters M100-M199 during execution.

/*
#include "interp_shim.h"

// Forward declaration of the Go callback.
extern void goMcodeCallback(int num, double arg1, double arg2);

// C trampoline that matches the user_defined_function signature.
static void mcode_trampoline(int num, double arg1, double arg2) {
    goMcodeCallback(num, arg1, arg2);
}

// Register the trampoline for a given slot.
static void register_mcode_slot(void *handle, int idx) {
    interp_set_user_defined_function(handle, idx, mcode_trampoline);
}
*/
import "C"
import (
	"sync/atomic"
)

// activeCanonPtr stores the Canon instance for C callbacks that lack a ctx parameter.
// Set before interpreter Read/Execute, cleared after.
var activeCanonPtr atomic.Pointer[Canon]

// goMcodeCallback is called from the interpreter (via C trampoline) when
// M100-M199 is encountered during Read/Execute. num is 0-99 (= mcode - 100).
//
//export goMcodeCallback
func goMcodeCallback(num C.int, arg1, arg2 C.double) {
	mcode := int32(num) + 100
	p := float64(arg1)
	q := float64(arg2)

	// Get the active canon instance (set per-thread via cgo.Handle).
	// The canon's task has the mcode handler.
	canon := activeCanon()
	if canon == nil {
		return
	}

	// First enqueue a WaitForMotion (canon Finish) to drain motion queue
	// before the M-code handler starts.
	canon.enqueue(waitForMotionSingleton)

	// Enqueue the M-code command for the sequencer.
	canon.enqueue(&McodeCmd{
		Mcode: mcode,
		P:     p,
		Q:     q,
	})
}

// RegisterMcodeSlot registers the Go callback trampoline in the interpreter
// for a specific M-code slot (0-99, corresponding to M100-M199).
func (i *CInterp) RegisterMcodeSlot(idx int) {
	C.register_mcode_slot(i.handle, C.int(idx))
}

// RegisterAllMcodeSlots registers the Go trampoline for all 100 M-code slots.
func (i *CInterp) RegisterAllMcodeSlots() {
	for idx := 0; idx < mcodeHandlerNum; idx++ {
		C.register_mcode_slot(i.handle, C.int(idx))
	}
}

// activeCanon returns the Canon instance for the current interpreter execution.
func activeCanon() *Canon {
	return activeCanonPtr.Load()
}

// setActiveCanon stores the canon pointer for C callbacks during interp execution.
func setActiveCanon(c *Canon) {
	activeCanonPtr.Store(c)
}

// clearActiveCanon removes the active canon pointer.
func clearActiveCanon() {
	activeCanonPtr.Store(nil)
}
