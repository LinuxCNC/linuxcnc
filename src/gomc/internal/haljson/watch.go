// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package haljson

import (
	"encoding/json"
	"fmt"
	"math"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
)

// watchPin is a flattened pin reference for efficient polling.
type watchPin struct {
	path string    // JSON path key, e.g. "heightpot.pos" or "bevels[0].axisPos"
	item *jsonItem // reference to the pin's jsonItem (has the hal.Pin pointers)
}

// watchState holds per-connection shadow state for change detection.
type watchState struct {
	pins    []watchPin
	shadows []uint64
}

// newWatchFactory creates a WatchFactory for a given root.
// Each subscriber gets its own shadow state — no shared locks needed.
func newWatchFactory(root *jsonRoot) apiserver.WatchFactory {
	// Pre-flatten the pin tree into a slice (shared across connections, read-only).
	pins := flattenPins(root.items, "")

	return func(_ json.RawMessage) (apiserver.WatchFunc, error) {
		// Per-connection state.
		state := &watchState{
			pins:    pins,
			shadows: make([]uint64, len(pins)),
		}
		// Initialize shadows to impossible value to force first full send.
		for i := range state.shadows {
			state.shadows[i] = ^uint64(0)
		}
		first := true

		return func() (json.RawMessage, error) {
			if first {
				first = false
				// First tick: send full structured JSON.
				return root.buildJSON(), nil
			}
			return state.poll()
		}, nil
	}
}

// poll checks all pins against their shadows. Returns nil if nothing changed,
// or a flat JSON object with only changed pin paths and their new values.
func (s *watchState) poll() (json.RawMessage, error) {
	var changed map[string]interface{}

	for i := range s.pins {
		cur := readPinRaw(s.pins[i].item)
		if cur == s.shadows[i] {
			continue
		}
		s.shadows[i] = cur
		if changed == nil {
			changed = make(map[string]interface{})
		}
		changed[s.pins[i].path] = s.pins[i].item.readPin()
	}

	if changed == nil {
		return nil, nil // nothing changed — tick suppressed
	}

	data, err := json.Marshal(changed)
	if err != nil {
		return nil, err
	}
	return data, nil
}

// readPinRaw reads a pin value as a raw uint64 for bitwise comparison.
// No string formatting, no allocation — just a register-width comparison.
func readPinRaw(item *jsonItem) uint64 {
	switch item.pinType {
	case pinTypeBit:
		if item.bitPin != nil {
			if item.bitPin.Get() {
				return 1
			}
			return 0
		}
	case pinTypeFloat:
		if item.fltPin != nil {
			return math.Float64bits(item.fltPin.Get())
		}
	case pinTypeS32:
		if item.s32Pin != nil {
			return uint64(item.s32Pin.Get())
		}
	case pinTypeU32:
		if item.u32Pin != nil {
			return uint64(item.u32Pin.Get())
		}
	}
	return 0
}

// flattenPins recursively collects all pin items with their dotted path.
func flattenPins(items []*jsonItem, prefix string) []watchPin {
	var pins []watchPin
	for _, item := range items {
		path := item.name
		if prefix != "" {
			path = prefix + "." + item.name
		}
		switch item.kind {
		case itemPin:
			pins = append(pins, watchPin{path: path, item: item})
		case itemObject:
			pins = append(pins, flattenPins(item.children, path)...)
		case itemArray:
			for i, elem := range item.children {
				elemPath := fmt.Sprintf("%s[%d]", path, i)
				pins = append(pins, flattenPins(elem.children, elemPath)...)
			}
		}
	}
	return pins
}
