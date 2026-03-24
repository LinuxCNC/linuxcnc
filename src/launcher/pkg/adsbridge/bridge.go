package adsbridge

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"math"
	"strings"

	"github.com/sittner/linuxcnc/src/launcher/pkg/hal"

	"github.com/sittner/linuxcnc/src/launcher/pkg/ads"
	"github.com/sittner/linuxcnc/src/launcher/pkg/adsconfig"
)

// typeInfo holds the ADS/TwinCAT type metadata for a single symbol.
type typeInfo struct {
	adsTypeName string   // normalised ADS type name, e.g. "BOOL", "DINT", "STRING(32)"
	adstID      uint32   // ADST constant (see ads.ADST*)
	byteSize    uint32   // wire size in bytes
	strLen      int      // for STRING(n): n (chars); 0 for non-string types
	typeGUID    [16]byte // 16-byte data type GUID; all zeros for primitive types
}

// parseTypeInfo converts a config type token (already upper-cased) to typeInfo.
func parseTypeInfo(typeName string) (typeInfo, error) {
	return parseTypeInfoResolved(typeName, nil)
}

// parseTypeInfoResolved is the internal implementation of parseTypeInfo that
// also resolves @enum/@struct aliases via the provided map. When typeName matches an
// alias, the base type is used for adstID/byteSize but adsTypeName retains the
// alias name so TwinCAT clients receive the correct derived type name.
func parseTypeInfoResolved(typeName string, aliases adsconfig.TypeAliasMap) (typeInfo, error) {
	// Resolve alias: use base type for wire encoding but keep alias name and GUID.
	if aliases != nil {
		if alias, ok := aliases[typeName]; ok {
			ti, err := parseTypeInfoResolved(alias.BaseType, nil)
			if err != nil {
				return typeInfo{}, fmt.Errorf("alias %q base type %q: %w", typeName, alias.BaseType, err)
			}
			ti.adsTypeName = typeName // preserve alias name for HMI
			ti.typeGUID = alias.GUID  // attach GUID from alias directive
			return ti, nil
		}
	}

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
func (a *halPinAccessor) TypeGUID() [16]byte         { return a.ti.typeGUID }

// newBitAccessor creates a PinAccessor for a bool HAL pin.
func newBitAccessor(pin *hal.Pin[bool], ti typeInfo) *halPinAccessor {
	return &halPinAccessor{
		ti: ti,
		readFn: func() ([]byte, error) {
			b := byte(0)
			if pin.Get() {
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
	return &halPinAccessor{
		ti: ti,
		readFn: func() ([]byte, error) {
			v := pin.Get()
			out := make([]byte, ti.byteSize) // zero-initialized = null-terminated
			n := len(v)
			if n > ti.strLen {
				n = ti.strLen
			}
			copy(out[:n], v[:n])
			return out, nil
		},
		writeFn: func(data []byte) error {
			n := len(data)
			if n > ti.strLen {
				n = ti.strLen
			}
			s := string(data[:n])
			if idx := bytes.IndexByte(data[:n], 0); idx >= 0 {
				s = s[:idx]
			}
			pin.Set(s)
			return nil
		},
	}
}

// ApplyContainerTypeInfo walks the parsed Node tree and, for every container
// node whose TypeName resolves to a @struct alias, sets the override type name
// and GUID on the corresponding groupAccessor in the symbol table. This
// propagates declared TwinCAT struct names and GUIDs to ADS group symbols so
// that ADSIGRP_SYM_INFOBYNAMEEX responses include the correct metadata.
//
// It must be called after NewBridge has registered all symbols (so that
// groupAccessors exist in the table), but before the ADS server starts
// serving requests.
func ApplyContainerTypeInfo(nodes []*adsconfig.Node, adsPrefix string, st *ads.SymbolTable, aliases adsconfig.TypeAliasMap) {
	for _, node := range nodes {
		if len(node.Children) == 0 {
			continue // leaf node — type info is set on leaf accessors directly
		}
		adsPath := joinName(adsPrefix, node.Name)

		if node.ArrayStart > 0 {
			// Array container: each array element [i] gets the struct type info
			// when the array was declared with "struct name[s..e] TypeName" syntax.
			for i := node.ArrayStart; i <= node.ArrayEnd; i++ {
				elemPath := fmt.Sprintf("%s[%d]", adsPath, i)
				if node.TypeName != "" {
					if alias, ok := aliases[node.TypeName]; ok && alias.StructDef != nil {
						st.SetGroupTypeInfo(elemPath, node.TypeName, alias.GUID)
					}
				}
				// Recursively apply to children within each array element.
				ApplyContainerTypeInfo(node.Children, elemPath, st, aliases)
			}
		} else {
			// Struct container: set type info if TypeName refers to a @struct alias.
			if node.TypeName != "" {
				if alias, ok := aliases[node.TypeName]; ok && alias.StructDef != nil {
					st.SetGroupTypeInfo(adsPath, node.TypeName, alias.GUID)
				}
			}
			// Recursively apply to children.
			ApplyContainerTypeInfo(node.Children, adsPath, st, aliases)
		}
	}
}

// Bridge holds all HAL pins and their corresponding ADS symbol registrations.
type Bridge struct {
	// pins retains references so the GC does not collect them.
	pins []any
}

// NewBridge creates HAL pins for all LayoutPins and registers them in the
// provided SymbolTable using pre-computed byte offsets from the layout.
// Pad entries (Dir == DirPad) occupy process-image space but do not create
// HAL pins and are not registered in the ADS symbol list.
//
// The optional aliases parameter provides @enum/@struct alias resolution so that
// user-defined type names are correctly mapped to their base ADS types while
// preserving the alias name and data-type GUID in the symbol info response.
func NewBridge(comp *hal.Component, pins []adsconfig.LayoutPin, st *ads.SymbolTable, aliasOpts ...adsconfig.TypeAliasMap) (*Bridge, error) {
	var aliases adsconfig.TypeAliasMap
	if len(aliasOpts) > 0 {
		aliases = aliasOpts[0]
	}

	b := &Bridge{}
	for _, cp := range pins {
		// Padding: reserve process-image space only (no HAL pin, no ADS name).
		if cp.Dir == adsconfig.DirPad {
			st.RegisterPadAt(cp.Offset, cp.Size)
			continue
		}

		ti, err := parseTypeInfoResolved(cp.TypeName, aliases)
		if err != nil {
			return nil, fmt.Errorf("symbol %q: %w", cp.ADSName, err)
		}

		dir := hal.In
		switch cp.Dir {
		case adsconfig.DirOut:
			dir = hal.Out
		case adsconfig.DirInOut:
			dir = hal.IO
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

		st.RegisterAt(cp.ADSName, cp.Offset, acc)
	}

	// Compute padded group sizes from layout information, including tail padding.
	// For each unique parent prefix, find the start offset, the end of the last
	// member, and the maximum field alignment. The padded size is then
	// alignUp(lastEnd, maxAlign) - startOffset, matching TwinCAT pack mode 0.
	type groupBounds struct {
		startOffset uint32
		lastEnd     uint32
		maxAlign    uint32
	}
	groups := make(map[string]*groupBounds)

	for _, cp := range pins {
		segs := strings.Split(cp.ADSName, ".")
		for prefixLen := 1; prefixLen < len(segs); prefixLen++ {
			prefix := strings.Join(segs[:prefixLen], ".")
			end := cp.Offset + cp.Size
			if gb, ok := groups[prefix]; !ok {
				groups[prefix] = &groupBounds{
					startOffset: cp.Offset,
					lastEnd:     end,
					maxAlign:    cp.Align,
				}
			} else {
				if end > gb.lastEnd {
					gb.lastEnd = end
				}
				if cp.Align > gb.maxAlign {
					gb.maxAlign = cp.Align
				}
			}
		}
	}
	for name, gb := range groups {
		st.SetGroupSize(name, alignUp(gb.lastEnd, gb.maxAlign)-gb.startOffset)
	}

	// Detect array container groups from bracket notation in ADSNames.
	// For each segment "X[N]", the group name is the prefix path up to "X"
	// (without brackets), and N is one element index. We compute the lower
	// and upper bounds across all pins to determine lBound and elemCount.
	type arrayBounds struct{ lo, hi uint32 }
	arrayGroups := make(map[string]*arrayBounds)

	for _, cp := range pins {
		segs := strings.Split(cp.ADSName, ".")
		accumulated := ""
		for _, seg := range segs {
			if accumulated == "" {
				accumulated = seg
			} else {
				accumulated = accumulated + "." + seg
			}
			lb := strings.Index(seg, "[")
			if lb < 0 {
				continue
			}
			rb := strings.Index(seg, "]")
			if rb <= lb {
				continue
			}
			var idx int
			if _, err := fmt.Sscanf(seg[lb+1:rb], "%d", &idx); err != nil {
				continue
			}
			// Group name: accumulated path up to (but not including) the bracket.
			// accumulated = "...prefix.X[N]", so group = accumulated[:len-len(seg)+lb]
			groupName := accumulated[:len(accumulated)-(len(seg)-lb)]
			groupName = strings.TrimSuffix(groupName, ".")
			ui := uint32(idx)
			if ab, ok := arrayGroups[groupName]; !ok {
				arrayGroups[groupName] = &arrayBounds{lo: ui, hi: ui}
			} else {
				if ui < ab.lo {
					ab.lo = ui
				}
				if ui > ab.hi {
					ab.hi = ui
				}
			}
		}
	}
	for name, ab := range arrayGroups {
		elems := ab.hi - ab.lo + 1
		st.SetGroupArrayInfo(name, 1, ab.lo, elems)
	}

	return b, nil
}

func joinName(prefix, name string) string {
	if prefix == "" {
		return name
	}
	return prefix + "." + name
}

func alignUp(offset, align uint32) uint32 {
	if align == 0 {
		return offset
	}
	return (offset + align - 1) &^ (align - 1)
}
