// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package pyvcpmodule

import (
	"encoding/xml"
	"fmt"
	"os"
	"strconv"
	"strings"

	"github.com/sittner/linuxcnc/src/gomc/pkg/hal"
)

// HAL type/direction constants matching LinuxCNC conventions.
const (
	halBit   = 1
	halFloat = 2
	halS32   = 3
	halU32   = 4

	halIn  = 16
	halOut = 32
)

// pinDef describes a single HAL pin to be created.
type pinDef struct {
	name    string
	halType int
	dir     int
	initval string // initial value from XML (empty means zero/false)

	// Runtime pin handles (set by createPins).
	bitPin   *hal.Pin[bool]
	floatPin *hal.Pin[float64]
	s32Pin   *hal.Pin[int32]
	u32Pin   *hal.Pin[uint32]
}

// readValue returns the current pin value as a string.
func (p *pinDef) readValue() string {
	switch p.halType {
	case halBit:
		if p.bitPin.Get() {
			return "1"
		}
		return "0"
	case halFloat:
		return strconv.FormatFloat(p.floatPin.Get(), 'f', -1, 64)
	case halS32:
		return strconv.FormatInt(int64(p.s32Pin.Get()), 10)
	case halU32:
		return strconv.FormatUint(uint64(p.u32Pin.Get()), 10)
	}
	return ""
}

// writeValue sets the pin value from a string. Only valid for OUT pins.
func (p *pinDef) writeValue(value string) (bool, error) {
	switch p.halType {
	case halBit:
		p.bitPin.Set(value == "1" || strings.EqualFold(value, "true"))
	case halFloat:
		v, err := strconv.ParseFloat(value, 64)
		if err != nil {
			return false, err
		}
		p.floatPin.Set(v)
	case halS32:
		v, err := strconv.ParseInt(value, 10, 32)
		if err != nil {
			return false, err
		}
		p.s32Pin.Set(int32(v))
	case halU32:
		v, err := strconv.ParseUint(value, 10, 32)
		if err != nil {
			return false, err
		}
		p.u32Pin.Set(uint32(v))
	default:
		return false, fmt.Errorf("unknown HAL type %d", p.halType)
	}
	return true, nil
}

// panel holds the parsed state of a PyVCP XML panel.
type panel struct {
	name string
	xml  string
	pins []*pinDef
}

// createPins creates HAL pins on the component for all pin definitions.
func (p *panel) createPins(comp *hal.Component) error {
	for _, pin := range p.pins {
		dir := hal.In
		if pin.dir == halOut {
			dir = hal.Out
		}
		switch pin.halType {
		case halBit:
			h, err := hal.NewPin[bool](comp, pin.name, dir)
			if err != nil {
				return fmt.Errorf("pin %q: %w", pin.name, err)
			}
			pin.bitPin = h
		case halFloat:
			h, err := hal.NewPin[float64](comp, pin.name, dir)
			if err != nil {
				return fmt.Errorf("pin %q: %w", pin.name, err)
			}
			pin.floatPin = h
		case halS32:
			h, err := hal.NewPin[int32](comp, pin.name, dir)
			if err != nil {
				return fmt.Errorf("pin %q: %w", pin.name, err)
			}
			pin.s32Pin = h
		case halU32:
			h, err := hal.NewPin[uint32](comp, pin.name, dir)
			if err != nil {
				return fmt.Errorf("pin %q: %w", pin.name, err)
			}
			pin.u32Pin = h
		default:
			return fmt.Errorf("pin %q: unknown type %d", pin.name, pin.halType)
		}
	}

	// Apply initial values (only meaningful for OUT pins).
	for _, pin := range p.pins {
		if pin.initval == "" || pin.dir != halOut {
			continue
		}
		if _, err := pin.writeValue(pin.initval); err != nil {
			return fmt.Errorf("pin %q: setting initval %q: %w", pin.name, pin.initval, err)
		}
	}

	return nil
}

// parsePanel reads a PyVCP XML file and extracts all pin definitions.
func parsePanel(name, xmlPath string) (*panel, error) {
	data, err := os.ReadFile(xmlPath)
	if err != nil {
		return nil, fmt.Errorf("reading XML: %w", err)
	}

	p := &panel{
		name: name,
		xml:  string(data),
	}

	// Parse XML and walk the element tree.
	decoder := xml.NewDecoder(strings.NewReader(p.xml))
	if err := p.walkXML(decoder); err != nil {
		return nil, fmt.Errorf("parsing XML: %w", err)
	}

	return p, nil
}

// walkXML traverses the XML document and extracts pin definitions.
func (p *panel) walkXML(decoder *xml.Decoder) error {
	// We need to track per-widget-type counters for auto-naming,
	// and collect child elements to extract parameters.
	counters := make(map[string]int)

	type element struct {
		name     string
		children map[string]string // child element name → text content
		attrs    map[string]string // attributes
	}

	var stack []*element

	for {
		tok, err := decoder.Token()
		if err != nil {
			break // EOF
		}

		switch t := tok.(type) {
		case xml.StartElement:
			e := &element{
				name:     t.Name.Local,
				children: make(map[string]string),
				attrs:    make(map[string]string),
			}
			for _, a := range t.Attr {
				e.attrs[a.Name.Local] = a.Value
			}
			stack = append(stack, e)

		case xml.CharData:
			if len(stack) > 0 {
				parent := stack[len(stack)-1]
				text := strings.TrimSpace(string(t))
				if text != "" {
					// Store as child of the parent element for later retrieval.
					// This will be overwritten if there are multiple text nodes,
					// but pyvcp XML typically has one text node per element.
					parent.children["_text"] = text
				}
			}

		case xml.EndElement:
			if len(stack) == 0 {
				continue
			}
			e := stack[len(stack)-1]
			stack = stack[:len(stack)-1]

			// If this element is a child of a widget element, record its text
			// as a parameter of the parent.
			if len(stack) > 0 {
				parent := stack[len(stack)-1]
				if text, ok := e.children["_text"]; ok {
					parent.children[e.name] = text
				}
			}

			// Check if this element is a known widget that creates pins.
			pins := extractPins(e.name, e.children, e.attrs, counters)
			p.pins = append(p.pins, pins...)
		}
	}
	return nil
}

// getParam returns a parameter value from children or attrs, stripping quotes.
func getParam(children, attrs map[string]string, key string) string {
	if v, ok := children[key]; ok {
		return stripQuotes(v)
	}
	if v, ok := attrs[key]; ok {
		return stripQuotes(v)
	}
	return ""
}

// getBoolParam returns a boolean parameter (true if "1", "true", "True").
func getBoolParam(children, attrs map[string]string, key string) bool {
	v := getParam(children, attrs, key)
	return v == "1" || strings.EqualFold(v, "true")
}

// stripQuotes removes surrounding single or double quotes.
func stripQuotes(s string) string {
	if len(s) >= 2 {
		if (s[0] == '"' && s[len(s)-1] == '"') || (s[0] == '\'' && s[len(s)-1] == '\'') {
			return s[1 : len(s)-1]
		}
	}
	return s
}

// autoName generates a default pin name for a widget when none is specified.
func autoName(widgetType string, counters map[string]int) string {
	n := counters[widgetType]
	counters[widgetType] = n + 1
	return widgetType + "." + strconv.Itoa(n)
}

// extractPins returns pin definitions for a known widget element.
// This mirrors the pin creation logic in pyvcp_widgets.py.
func extractPins(widgetName string, children, attrs map[string]string, counters map[string]int) []*pinDef {
	halpin := getParam(children, attrs, "halpin")

	switch widgetName {
	case "led", "rectled":
		// BIT IN, optional .disable BIT IN
		if halpin == "" {
			halpin = autoName("led", counters)
		}
		pins := []*pinDef{{name: halpin, halType: halBit, dir: halIn}}
		if getBoolParam(children, attrs, "disable_pin") {
			pins = append(pins, &pinDef{name: halpin + ".disable", halType: halBit, dir: halIn})
		}
		return pins

	case "button":
		// BIT OUT, optional .disable BIT IN
		if halpin == "" {
			halpin = autoName("button", counters)
		}
		pins := []*pinDef{{name: halpin, halType: halBit, dir: halOut}}
		if getBoolParam(children, attrs, "disable_pin") || getBoolParam(children, attrs, "disablepin") {
			pins = append(pins, &pinDef{name: halpin + ".disable", halType: halBit, dir: halIn})
		}
		return pins

	case "checkbutton":
		// BIT OUT + .changepin BIT IN
		if halpin == "" {
			halpin = autoName("checkbutton", counters)
		}
		initval := getParam(children, attrs, "initval")
		var bitInit string
		if initval != "" {
			v, err := strconv.ParseFloat(initval, 64)
			if err == nil && v >= 0.5 {
				bitInit = "1"
			}
		}
		return []*pinDef{
			{name: halpin, halType: halBit, dir: halOut, initval: bitInit},
			{name: halpin + ".changepin", halType: halBit, dir: halIn},
		}

	case "radiobutton":
		// One BIT OUT per choice: halpin.choicename
		if halpin == "" {
			halpin = autoName("radiobutton", counters)
		}
		choices := getParam(children, attrs, "choices")
		initval := getParam(children, attrs, "initval")
		initIdx := 0
		if initval != "" {
			if v, err := strconv.Atoi(initval); err == nil {
				initIdx = v
			}
		}
		var pins []*pinDef
		for i, c := range parseList(choices) {
			iv := ""
			if i == initIdx {
				iv = "1"
			}
			pins = append(pins, &pinDef{name: halpin + "." + c, halType: halBit, dir: halOut, initval: iv})
		}
		return pins

	case "dial":
		// FLOAT OUT + param_pin FLOAT IN (always created, like Python pyvcp)
		autoBase := autoName("dial", counters) // always increment counter
		if halpin == "" {
			halpin = autoBase + ".out"
		}
		halparam := getParam(children, attrs, "halparam")
		if halparam == "" {
			halparam = autoBase + ".param_pin"
		}
		initval := getParam(children, attrs, "initval")
		pins := []*pinDef{
			{name: halparam, halType: halFloat, dir: halIn},
			{name: halpin, halType: halFloat, dir: halOut, initval: initval},
		}
		return pins

	case "jogwheel":
		// FLOAT OUT (count), optional .reset BIT IN, optional .scale FLOAT IN
		if halpin == "" {
			base := autoName("jogwheel", counters)
			halpin = base + ".count"
		}
		initval := getParam(children, attrs, "initval")
		pins := []*pinDef{{name: halpin, halType: halFloat, dir: halOut, initval: initval}}
		// Derive base name for sub-pins
		base := halpin
		if strings.HasSuffix(base, ".count") {
			base = base[:len(base)-6]
		}
		if getBoolParam(children, attrs, "clear_pin") {
			pins = append(pins, &pinDef{name: base + ".reset", halType: halBit, dir: halIn})
		}
		if getBoolParam(children, attrs, "scale_pin") {
			pins = append(pins, &pinDef{name: base + ".scale", halType: halFloat, dir: halIn})
		}
		return pins

	case "spinbox":
		// FLOAT OUT, optional param_pin FLOAT IN
		autoBase := autoName("spinbox", counters) // always increment counter
		if halpin == "" {
			halpin = autoBase
		}
		initval := getParam(children, attrs, "initval")
		pins := []*pinDef{{name: halpin, halType: halFloat, dir: halOut, initval: initval}}
		if getBoolParam(children, attrs, "param_pin") {
			halparam := getParam(children, attrs, "halparam")
			if halparam == "" {
				halparam = autoBase + ".param_pin"
			}
			pins = append(pins, &pinDef{name: halparam, halType: halFloat, dir: halIn})
		}
		return pins

	case "scale":
		// S32 OUT (-i) + FLOAT OUT (-f), optional param_pin FLOAT IN
		autoBase := autoName("scale", counters) // always increment counter
		if halpin == "" {
			halpin = autoBase
		}
		initval := getParam(children, attrs, "initval")
		pins := []*pinDef{
			{name: halpin + "-i", halType: halS32, dir: halOut, initval: initval},
			{name: halpin + "-f", halType: halFloat, dir: halOut, initval: initval},
		}
		if getBoolParam(children, attrs, "param_pin") {
			halparam := getParam(children, attrs, "halparam")
			if halparam == "" {
				halparam = autoBase + ".param_pin"
			}
			pins = append(pins, &pinDef{name: halparam, halType: halFloat, dir: halIn})
		}
		return pins

	case "meter":
		// FLOAT IN
		if halpin == "" {
			halpin = autoName("meter", counters) + ".value"
		}
		return []*pinDef{{name: halpin, halType: halFloat, dir: halIn}}

	case "bar":
		// FLOAT IN
		if halpin == "" {
			halpin = autoName("bar", counters)
		}
		return []*pinDef{{name: halpin, halType: halFloat, dir: halIn}}

	case "number":
		// FLOAT IN
		if halpin == "" {
			halpin = autoName("number", counters)
		}
		return []*pinDef{{name: halpin, halType: halFloat, dir: halIn}}

	case "u32":
		// U32 IN
		if halpin == "" {
			halpin = autoName("number", counters) // shares counter with number
		}
		return []*pinDef{{name: halpin, halType: halU32, dir: halIn}}

	case "s32":
		// S32 IN
		if halpin == "" {
			halpin = autoName("number", counters) // shares counter with number
		}
		return []*pinDef{{name: halpin, halType: halS32, dir: halIn}}

	case "timer":
		// .reset BIT IN + .run BIT IN
		if halpin == "" {
			halpin = autoName("timer", counters)
		}
		return []*pinDef{
			{name: halpin + ".reset", halType: halBit, dir: halIn},
			{name: halpin + ".run", halType: halBit, dir: halIn},
		}

	case "multilabel":
		// One BIT IN per legend: halpin.legendN, optional .disable BIT IN
		if halpin == "" {
			halpin = autoName("multilabel", counters)
		}
		legends := getParam(children, attrs, "legends")
		var pins []*pinDef
		for i := range parseList(legends) {
			pins = append(pins, &pinDef{
				name:    halpin + ".legend" + strconv.Itoa(i),
				halType: halBit,
				dir:     halIn,
			})
			if i >= 5 { // limit to 6 legends
				break
			}
		}
		if getBoolParam(children, attrs, "disable_pin") {
			pins = append(pins, &pinDef{name: halpin + ".disable", halType: halBit, dir: halIn})
		}
		return pins

	case "label":
		// Optional .disable BIT IN (only if disable_pin is set)
		if getBoolParam(children, attrs, "disable_pin") {
			if halpin == "" {
				halpin = autoName("label", counters)
			}
			return []*pinDef{{name: halpin + ".disable", halType: halBit, dir: halIn}}
		}
		return nil

	case "image_bit":
		// BIT IN
		if halpin == "" {
			halpin = autoName("number", counters) // shares counter with number
		}
		return []*pinDef{{name: halpin, halType: halBit, dir: halIn}}

	case "image_u32":
		// U32 IN
		if halpin == "" {
			halpin = autoName("number", counters) // shares counter with number
		}
		return []*pinDef{{name: halpin, halType: halU32, dir: halIn}}

	default:
		// Layout widgets (vbox, hbox, labelframe, tabs, table, etc.) and
		// meta widgets (title, axisoptions, option, image) create no pins.
		return nil
	}
}

// parseList parses a Python-style list literal like ["a", "b", "c"] or
// ("a", "b", "c") into a slice of strings, stripping quotes.
func parseList(s string) []string {
	s = strings.TrimSpace(s)
	if s == "" {
		return nil
	}
	// Strip outer brackets/parens
	if (s[0] == '[' || s[0] == '(') && len(s) > 1 {
		s = s[1 : len(s)-1]
	}
	parts := strings.Split(s, ",")
	var result []string
	for _, p := range parts {
		p = strings.TrimSpace(p)
		p = stripQuotes(p)
		if p != "" {
			result = append(result, p)
		}
	}
	return result
}
