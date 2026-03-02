package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"math"
	"strings"

	"linuxcnc.org/hal"

	"linuxcnc.org/hal-ads-server/ads"
)

// typeInfo holds the ADS/TwinCAT type metadata for a single symbol.
type typeInfo struct {
	adsTypeName string // normalised ADS type name, e.g. "BOOL", "DINT", "STRING(32)"
	adstID      uint32 // ADST constant (see ads.ADST*)
	byteSize    uint32 // wire size in bytes
	strLen      int    // for STRING(n): n (chars); 0 for non-string types
}

// parseTypeInfo converts a config type token (already upper-cased) to typeInfo.
func parseTypeInfo(typeName string) (typeInfo, error) {
	// Handle STRING(n) specially.
	if strings.HasPrefix(typeName, "STRING(") {
		var n int
		if _, err := fmt.Sscanf(typeName, "STRING(%d)", &n); err != nil || n <= 0 {
			return typeInfo{}, fmt.Errorf("invalid string type %q", typeName)
		}
		return typeInfo{
			adsTypeName: typeName,
			adstID:      ads.ADSTString,
			byteSize:    uint32(n + 1), // null terminator
			strLen:      n,
		}, nil
	}

	switch typeName {
	case "BOOL":
		return typeInfo{adsTypeName: "BOOL", adstID: ads.ADSTBool, byteSize: 1}, nil
	case "BYTE", "USINT":
		return typeInfo{adsTypeName: typeName, adstID: ads.ADSTUInt8, byteSize: 1}, nil
	case "WORD", "UINT":
		return typeInfo{adsTypeName: typeName, adstID: ads.ADSTUInt16, byteSize: 2}, nil
	case "DWORD", "UDINT", "TIME", "TOD", "DATE", "DT":
		return typeInfo{adsTypeName: typeName, adstID: ads.ADSTUInt32, byteSize: 4}, nil
	case "SINT":
		return typeInfo{adsTypeName: "SINT", adstID: ads.ADSTInt8, byteSize: 1}, nil
	case "INT":
		return typeInfo{adsTypeName: "INT", adstID: ads.ADSTInt16, byteSize: 2}, nil
	case "DINT":
		return typeInfo{adsTypeName: "DINT", adstID: ads.ADSTInt32, byteSize: 4}, nil
	case "REAL":
		return typeInfo{adsTypeName: "REAL", adstID: ads.ADSTReal32, byteSize: 4}, nil
	case "LREAL":
		return typeInfo{adsTypeName: "LREAL", adstID: ads.ADSTReal64, byteSize: 8}, nil
	default:
		return typeInfo{}, fmt.Errorf("unsupported ADS type %q", typeName)
	}
}

// halPinAccessor is the ads.PinAccessor implementation backed by a HAL pin.
// Since hal.Pin[T] is generic and Go does not allow interface variables to hold
// generic types directly, we use a closure-based approach: each accessor stores
// read and write functions that capture the typed pin.
type halPinAccessor struct {
	ti      typeInfo
	readFn  func() ([]byte, error)
	writeFn func([]byte) error
}

func (a *halPinAccessor) ReadBytes() ([]byte, error) { return a.readFn() }
func (a *halPinAccessor) WriteBytes(d []byte) error  { return a.writeFn(d) }
func (a *halPinAccessor) Size() uint32               { return a.ti.byteSize }
func (a *halPinAccessor) TypeName() string           { return a.ti.adsTypeName }
func (a *halPinAccessor) TypeID() uint32             { return a.ti.adstID }

// newBitAccessor creates a PinAccessor for a bool HAL pin.
func newBitAccessor(pin *hal.Pin[bool], ti typeInfo) *halPinAccessor {
	return &halPinAccessor{
		ti: ti,
		readFn: func() ([]byte, error) {
			v := pin.Get()
			b := byte(0)
			if v {
				b = 1
			}
			return []byte{b}, nil
		},
		writeFn: func(data []byte) error {
			if len(data) < 1 {
				return fmt.Errorf("bool write: need 1 byte, got %d", len(data))
			}
			pin.Set(data[0] != 0)
			return nil
		},
	}
}

// newU32Accessor creates a PinAccessor for a uint32 HAL pin with a given ADS wire size.
func newU32Accessor(pin *hal.Pin[uint32], ti typeInfo) *halPinAccessor {
	return &halPinAccessor{
		ti: ti,
		readFn: func() ([]byte, error) {
			v := pin.Get()
			switch ti.byteSize {
			case 1:
				return []byte{byte(v)}, nil
			case 2:
				b := make([]byte, 2)
				binary.LittleEndian.PutUint16(b, uint16(v))
				return b, nil
			default: // 4
				b := make([]byte, 4)
				binary.LittleEndian.PutUint32(b, v)
				return b, nil
			}
		},
		writeFn: func(data []byte) error {
			if uint32(len(data)) < ti.byteSize {
				return fmt.Errorf("u32 write: need %d byte(s), got %d", ti.byteSize, len(data))
			}
			var v uint32
			switch ti.byteSize {
			case 1:
				v = uint32(data[0])
			case 2:
				v = uint32(binary.LittleEndian.Uint16(data[:2]))
			default: // 4
				v = binary.LittleEndian.Uint32(data[:4])
			}
			pin.Set(v)
			return nil
		},
	}
}

// newS32Accessor creates a PinAccessor for an int32 HAL pin with a given ADS wire size.
func newS32Accessor(pin *hal.Pin[int32], ti typeInfo) *halPinAccessor {
	return &halPinAccessor{
		ti: ti,
		readFn: func() ([]byte, error) {
			v := pin.Get()
			switch ti.byteSize {
			case 1:
				return []byte{byte(int8(v))}, nil
			case 2:
				b := make([]byte, 2)
				binary.LittleEndian.PutUint16(b, uint16(int16(v)))
				return b, nil
			default: // 4
				b := make([]byte, 4)
				binary.LittleEndian.PutUint32(b, uint32(v))
				return b, nil
			}
		},
		writeFn: func(data []byte) error {
			if uint32(len(data)) < ti.byteSize {
				return fmt.Errorf("s32 write: need %d byte(s), got %d", ti.byteSize, len(data))
			}
			var v int32
			switch ti.byteSize {
			case 1:
				v = int32(int8(data[0]))
			case 2:
				v = int32(int16(binary.LittleEndian.Uint16(data[:2])))
			default: // 4
				v = int32(binary.LittleEndian.Uint32(data[:4]))
			}
			pin.Set(v)
			return nil
		},
	}
}

// newFloatAccessor creates a PinAccessor for a float64 HAL pin.
// REAL uses 4-byte IEEE 754 float32 on the wire; LREAL uses 8-byte float64.
func newFloatAccessor(pin *hal.Pin[float64], ti typeInfo) *halPinAccessor {
	return &halPinAccessor{
		ti: ti,
		readFn: func() ([]byte, error) {
			v := pin.Get()
			if ti.byteSize == 4 {
				b := make([]byte, 4)
				binary.LittleEndian.PutUint32(b, math.Float32bits(float32(v)))
				return b, nil
			}
			b := make([]byte, 8)
			binary.LittleEndian.PutUint64(b, math.Float64bits(v))
			return b, nil
		},
		writeFn: func(data []byte) error {
			if uint32(len(data)) < ti.byteSize {
				return fmt.Errorf("float write: need %d byte(s), got %d", ti.byteSize, len(data))
			}
			var v float64
			if ti.byteSize == 4 {
				v = float64(math.Float32frombits(binary.LittleEndian.Uint32(data[:4])))
			} else {
				v = math.Float64frombits(binary.LittleEndian.Uint64(data[:8]))
			}
			pin.Set(v)
			return nil
		},
	}
}

// newStringAccessor creates a PinAccessor for a string HAL pin.
// ADS STRING(n) is stored as n+1 bytes (null-terminated).
func newStringAccessor(pin *hal.Pin[string], ti typeInfo) *halPinAccessor {
	buf := make([]byte, ti.byteSize) // persistent fixed buffer, zero-initialized (null-terminated)

	return &halPinAccessor{
		ti: ti,
		readFn: func() ([]byte, error) {
			// Return a copy of the fixed buffer (always correct size, always null-terminated).
			out := make([]byte, ti.byteSize)
			copy(out, buf)
			return out, nil
		},
		writeFn: func(data []byte) error {
			// Clear buffer completely (zero-fill ensures null-termination).
			clear(buf)
			// Clamp input length to max string length (strLen = n, not n+1).
			n := len(data)
			if n > ti.strLen {
				n = ti.strLen
			}
			copy(buf[:n], data[:n])
			// Sync clamped string to HAL pin, truncating at embedded null if any.
			s := string(buf[:n])
			if idx := bytes.IndexByte(buf[:n], 0); idx >= 0 {
				s = s[:idx]
			}
			pin.Set(s)
			return nil
		},
	}
}

// Bridge holds all HAL pins and their corresponding ADS symbol registrations.
type Bridge struct {
	// pins retains references so the GC does not collect them.
	pins []interface{}
}

// NewBridge creates HAL pins for all ConfigPins and registers them in the
// provided SymbolTable. The component name prefix is prepended to all HAL pin names.
func NewBridge(comp *hal.Component, pins []ConfigPin, st *ads.SymbolTable) (*Bridge, error) {
	b := &Bridge{}
	for _, cp := range pins {
		ti, err := parseTypeInfo(cp.TypeName)
		if err != nil {
			return nil, fmt.Errorf("symbol %q: %w", cp.ADSName, err)
		}

		dir := hal.In
		if cp.Dir == DirOut {
			dir = hal.Out
		}

		var acc ads.PinAccessor

		switch {
		case ti.adstID == ads.ADSTBool:
			p, err := hal.NewPin[bool](comp, cp.HALPath, dir)
			if err != nil {
				return nil, fmt.Errorf("create HAL pin %q: %w", cp.HALPath, err)
			}
			acc = newBitAccessor(p, ti)
			b.pins = append(b.pins, p)

		case ti.adstID == ads.ADSTUInt8 || ti.adstID == ads.ADSTUInt16 || ti.adstID == ads.ADSTUInt32:
			p, err := hal.NewPin[uint32](comp, cp.HALPath, dir)
			if err != nil {
				return nil, fmt.Errorf("create HAL pin %q: %w", cp.HALPath, err)
			}
			acc = newU32Accessor(p, ti)
			b.pins = append(b.pins, p)

		case ti.adstID == ads.ADSTInt8 || ti.adstID == ads.ADSTInt16 || ti.adstID == ads.ADSTInt32:
			p, err := hal.NewPin[int32](comp, cp.HALPath, dir)
			if err != nil {
				return nil, fmt.Errorf("create HAL pin %q: %w", cp.HALPath, err)
			}
			acc = newS32Accessor(p, ti)
			b.pins = append(b.pins, p)

		case ti.adstID == ads.ADSTReal32 || ti.adstID == ads.ADSTReal64:
			p, err := hal.NewPin[float64](comp, cp.HALPath, dir)
			if err != nil {
				return nil, fmt.Errorf("create HAL pin %q: %w", cp.HALPath, err)
			}
			acc = newFloatAccessor(p, ti)
			b.pins = append(b.pins, p)

		case ti.adstID == ads.ADSTString:
			p, err := hal.NewPin[string](comp, cp.HALPath, dir)
			if err != nil {
				return nil, fmt.Errorf("create HAL pin %q: %w", cp.HALPath, err)
			}
			acc = newStringAccessor(p, ti)
			b.pins = append(b.pins, p)

		default:
			return nil, fmt.Errorf("symbol %q: unsupported type %q", cp.ADSName, cp.TypeName)
		}

		st.Register(cp.ADSName, acc)
	}
	return b, nil
}
