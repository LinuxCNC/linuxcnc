// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

// canon_bridge_init.go provides the function to build and install
// the canon callback table into the interpreter.

import (
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/canon"
)

// canonCallbackTable holds a persistent canon_callbacks_t that points
// to Go export functions. The cgo.Handle stored inside keeps the
// Canon alive for as long as the table is in use.
type canonCallbackTable struct {
	cbs unsafe.Pointer
}

// newCanonCallbackTable creates a C callback table that dispatches
// to the given Canon instance. The caller must call release() when done.
func newCanonCallbackTable(c *Canon) *canonCallbackTable {
	return &canonCallbackTable{cbs: canon.BuildCanonCallbacks(c)}
}

// ptr returns an unsafe.Pointer to the C callback table,
// suitable for passing to CInterp.SetCanonCallbacks().
func (ct *canonCallbackTable) ptr() unsafe.Pointer {
	return ct.cbs
}

// release frees the cgo handle. After this, the callback table
// must not be used.
func (ct *canonCallbackTable) release() {
	canon.FreeCanonCallbacks(ct.cbs)
	ct.cbs = nil
}
