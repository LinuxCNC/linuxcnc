// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package haljson

import (
	"encoding/json"
	"fmt"

	"github.com/sittner/linuxcnc/src/gomc/pkg/hal"
)

// pinType represents a HAL pin type.
type pinType int

const (
	pinTypeBit pinType = iota
	pinTypeFloat
	pinTypeS32
	pinTypeU32
)

// pinDir represents a HAL pin direction.
type pinDir int

const (
	pinDirIn pinDir = iota
	pinDirOut
	pinDirIO
)

// jsonItem is a node in the JSON tree. It can be a pin, object, or array.
type jsonItem struct {
	kind     itemKind
	name     string
	children []*jsonItem

	// Pin fields (only used when kind == itemPin)
	pinType pinType
	pinDir  pinDir
	bitPin  *hal.Pin[bool]
	fltPin  *hal.Pin[float64]
	s32Pin  *hal.Pin[int32]
	u32Pin  *hal.Pin[uint32]

	// Array fields (only used when kind == itemArray)
	arraySize int
}

type itemKind int

const (
	itemPin itemKind = iota
	itemObject
	itemArray
)

// jsonRoot is one top-level endpoint root.
type jsonRoot struct {
	path  string
	items []*jsonItem
}

// parseItems converts XML items into jsonItem tree nodes.
func (r *jsonRoot) parseItems(xmlItems []xmlItem) error {
	items, err := convertItems(xmlItems)
	if err != nil {
		return err
	}
	r.items = items
	return nil
}

func convertItems(xmlItems []xmlItem) ([]*jsonItem, error) {
	var items []*jsonItem
	for _, xi := range xmlItems {
		item, err := convertItem(xi)
		if err != nil {
			return nil, err
		}
		items = append(items, item)
	}
	return items, nil
}

func convertItem(xi xmlItem) (*jsonItem, error) {
	switch xi.XMLName.Local {
	case "halJsonPin":
		return convertPin(xi)
	case "halJsonParam":
		// Params are treated same as pins for JSON purposes.
		return convertPin(xi)
	case "halJsonObject":
		return convertObject(xi)
	case "halJsonArray":
		return convertArray(xi)
	default:
		return nil, fmt.Errorf("unknown element %q", xi.XMLName.Local)
	}
}

func convertPin(xi xmlItem) (*jsonItem, error) {
	if xi.Name == "" {
		return nil, fmt.Errorf("halJsonPin missing name attribute")
	}
	pt, err := parseHalType(xi.Type)
	if err != nil {
		return nil, fmt.Errorf("pin %q: %w", xi.Name, err)
	}
	pd, err := parseHalDir(xi.Dir)
	if err != nil {
		return nil, fmt.Errorf("pin %q: %w", xi.Name, err)
	}
	return &jsonItem{
		kind:    itemPin,
		name:    xi.Name,
		pinType: pt,
		pinDir:  pd,
	}, nil
}

func convertObject(xi xmlItem) (*jsonItem, error) {
	if xi.Name == "" {
		return nil, fmt.Errorf("halJsonObject missing name attribute")
	}
	children, err := convertItems(xi.Children)
	if err != nil {
		return nil, fmt.Errorf("object %q: %w", xi.Name, err)
	}
	return &jsonItem{
		kind:     itemObject,
		name:     xi.Name,
		children: children,
	}, nil
}

func convertArray(xi xmlItem) (*jsonItem, error) {
	if xi.Name == "" {
		return nil, fmt.Errorf("halJsonArray missing name attribute")
	}
	if xi.Size < 1 {
		return nil, fmt.Errorf("array %q: size must be >= 1", xi.Name)
	}
	// Parse the template children (used for each array element).
	template, err := convertItems(xi.Children)
	if err != nil {
		return nil, fmt.Errorf("array %q: %w", xi.Name, err)
	}
	return &jsonItem{
		kind:      itemArray,
		name:      xi.Name,
		arraySize: xi.Size,
		children:  template,
	}, nil
}

// createPins creates HAL pins for all items in this root.
// The pin names are relative to the component (NewPin prepends comp name).
func (r *jsonRoot) createPins(comp *hal.Component) error {
	return createItemPins(comp, r.path, r.items)
}

func createItemPins(comp *hal.Component, prefix string, items []*jsonItem) error {
	for _, item := range items {
		switch item.kind {
		case itemPin:
			pinName := prefix + "." + item.name
			if err := item.createPin(comp, pinName); err != nil {
				return err
			}
		case itemObject:
			objPrefix := prefix + "." + item.name
			if err := createItemPins(comp, objPrefix, item.children); err != nil {
				return err
			}
		case itemArray:
			if err := item.expandArray(comp, prefix); err != nil {
				return err
			}
		}
	}
	return nil
}

// expandArray expands a template array into N copies, each with its own HAL pins.
// After expansion, item.children contains N object items, each with the template's children cloned.
func (item *jsonItem) expandArray(comp *hal.Component, prefix string) error {
	template := item.children
	expanded := make([]*jsonItem, item.arraySize)
	for i := 0; i < item.arraySize; i++ {
		elemPrefix := fmt.Sprintf("%s.%s-%d", prefix, item.name, i)
		children, err := cloneItems(template)
		if err != nil {
			return fmt.Errorf("array %q[%d]: %w", item.name, i, err)
		}
		if err := createItemPins(comp, elemPrefix, children); err != nil {
			return err
		}
		expanded[i] = &jsonItem{
			kind:     itemObject,
			name:     fmt.Sprintf("%d", i),
			children: children,
		}
	}
	item.children = expanded
	return nil
}

// cloneItems deep-copies a slice of items (for array expansion).
func cloneItems(items []*jsonItem) ([]*jsonItem, error) {
	cloned := make([]*jsonItem, len(items))
	for i, src := range items {
		c := *src
		if src.children != nil {
			children, err := cloneItems(src.children)
			if err != nil {
				return nil, err
			}
			c.children = children
		}
		cloned[i] = &c
	}
	return cloned, nil
}

// createPin creates the actual HAL pin on the component.
func (item *jsonItem) createPin(comp *hal.Component, name string) error {
	dir := halDirection(item.pinDir)
	switch item.pinType {
	case pinTypeBit:
		p, err := hal.NewPin[bool](comp, name, dir)
		if err != nil {
			return fmt.Errorf("creating pin %q: %w", name, err)
		}
		item.bitPin = p
	case pinTypeFloat:
		p, err := hal.NewPin[float64](comp, name, dir)
		if err != nil {
			return fmt.Errorf("creating pin %q: %w", name, err)
		}
		item.fltPin = p
	case pinTypeS32:
		p, err := hal.NewPin[int32](comp, name, dir)
		if err != nil {
			return fmt.Errorf("creating pin %q: %w", name, err)
		}
		item.s32Pin = p
	case pinTypeU32:
		p, err := hal.NewPin[uint32](comp, name, dir)
		if err != nil {
			return fmt.Errorf("creating pin %q: %w", name, err)
		}
		item.u32Pin = p
	}
	return nil
}

func halDirection(d pinDir) hal.Direction {
	switch d {
	case pinDirIn:
		return hal.In
	case pinDirOut:
		return hal.Out
	case pinDirIO:
		return hal.IO
	default:
		return hal.In
	}
}

// buildJSON produces a JSON snapshot of all pins under this root.
func (r *jsonRoot) buildJSON() json.RawMessage {
	obj := buildItemsJSON(r.items)
	data, _ := json.Marshal(obj)
	return data
}

func buildItemsJSON(items []*jsonItem) map[string]interface{} {
	obj := make(map[string]interface{}, len(items))
	for _, item := range items {
		switch item.kind {
		case itemPin:
			obj[item.name] = item.readPin()
		case itemObject:
			obj[item.name] = buildItemsJSON(item.children)
		case itemArray:
			arr := make([]interface{}, len(item.children))
			for i, elem := range item.children {
				arr[i] = buildItemsJSON(elem.children)
			}
			obj[item.name] = arr
		}
	}
	return obj
}

// readPin reads the current value from a HAL pin.
func (item *jsonItem) readPin() interface{} {
	switch item.pinType {
	case pinTypeBit:
		if item.bitPin != nil {
			return item.bitPin.Get()
		}
	case pinTypeFloat:
		if item.fltPin != nil {
			return item.fltPin.Get()
		}
	case pinTypeS32:
		if item.s32Pin != nil {
			return item.s32Pin.Get()
		}
	case pinTypeU32:
		if item.u32Pin != nil {
			return item.u32Pin.Get()
		}
	}
	return nil
}

// applyJSON writes values from a JSON payload to output pins.
func (r *jsonRoot) applyJSON(data json.RawMessage) error {
	var obj map[string]json.RawMessage
	if err := json.Unmarshal(data, &obj); err != nil {
		return fmt.Errorf("invalid JSON object: %w", err)
	}
	return applyItemsJSON(r.items, obj)
}

func applyItemsJSON(items []*jsonItem, obj map[string]json.RawMessage) error {
	for _, item := range items {
		raw, ok := obj[item.name]
		if !ok {
			continue
		}
		switch item.kind {
		case itemPin:
			if item.pinDir == pinDirIn {
				// Can't write to input pins from the client.
				continue
			}
			if err := item.writePin(raw); err != nil {
				return fmt.Errorf("pin %q: %w", item.name, err)
			}
		case itemObject:
			var child map[string]json.RawMessage
			if err := json.Unmarshal(raw, &child); err != nil {
				return fmt.Errorf("object %q: %w", item.name, err)
			}
			if err := applyItemsJSON(item.children, child); err != nil {
				return err
			}
		case itemArray:
			var arr []json.RawMessage
			if err := json.Unmarshal(raw, &arr); err != nil {
				return fmt.Errorf("array %q: %w", item.name, err)
			}
			for i, elem := range arr {
				if i >= len(item.children) {
					break
				}
				var child map[string]json.RawMessage
				if err := json.Unmarshal(elem, &child); err != nil {
					return fmt.Errorf("array %q[%d]: %w", item.name, i, err)
				}
				if err := applyItemsJSON(item.children[i].children, child); err != nil {
					return err
				}
			}
		}
	}
	return nil
}

// writePin writes a JSON value to a HAL pin.
func (item *jsonItem) writePin(raw json.RawMessage) error {
	switch item.pinType {
	case pinTypeBit:
		if item.bitPin == nil {
			return nil
		}
		var v bool
		if err := json.Unmarshal(raw, &v); err != nil {
			return err
		}
		item.bitPin.Set(v)
	case pinTypeFloat:
		if item.fltPin == nil {
			return nil
		}
		var v float64
		if err := json.Unmarshal(raw, &v); err != nil {
			return err
		}
		item.fltPin.Set(v)
	case pinTypeS32:
		if item.s32Pin == nil {
			return nil
		}
		var v int32
		if err := json.Unmarshal(raw, &v); err != nil {
			return err
		}
		item.s32Pin.Set(v)
	case pinTypeU32:
		if item.u32Pin == nil {
			return nil
		}
		var v uint32
		if err := json.Unmarshal(raw, &v); err != nil {
			return err
		}
		item.u32Pin.Set(v)
	}
	return nil
}
