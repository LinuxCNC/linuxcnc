package main

import (
	"fmt"
	"strings"
)

// LayoutPin describes a single leaf symbol with its pre-computed byte offset
// in the ADS process image.
type LayoutPin struct {
	// Dir is the HAL pin direction ("in", "out", "inout", or "pad").
	Dir PinDir
	// HALPath is the dot-separated path for the HAL pin name,
	// e.g. "DISPLAY_DATA.stData.aPools.1.fTemp".
	HALPath string
	// ADSName is the ADS symbol name with bracket notation for array indices,
	// e.g. "DISPLAY_DATA.stData.aPools[1].fTemp".
	ADSName string
	// TypeName is the ADS/TwinCAT type name, e.g. "BOOL", "REAL", "STRING(31)".
	TypeName string
	// Offset is the byte offset of this field in the process image.
	Offset uint32
	// Size is the wire size in bytes.
	Size uint32
	// Align is the natural alignment of this type (1, 2, 4, or 8).
	Align uint32
}

// TypeSize returns the wire size and natural alignment for an ADS/TwinCAT type
// name. Both size and align are in bytes.
//
// Alignment table (TwinCAT default pack mode 0, natural alignment):
//
//	BOOL, BYTE, SINT, USINT  → size 1, align 1
//	WORD, UINT, INT          → size 2, align 2
//	DWORD, UDINT, DINT,
//	REAL, TIME, TOD, DATE, DT → size 4, align 4
//	LREAL                    → size 8, align 8
//	STRING(n)                → size n+1, align 1
func TypeSize(typeName string) (size, align uint32, err error) {
	return typeSizeResolved(typeName, nil)
}

// typeSizeResolved is the internal implementation of TypeSize that also
// accepts an optional TypeAliasMap for resolving @type aliases.
func typeSizeResolved(typeName string, aliases TypeAliasMap) (size, align uint32, err error) {
	// Resolve alias first (aliases take priority to preserve the caller's intent).
	if aliases != nil {
		if alias, ok := aliases[typeName]; ok {
			return typeSizeResolved(alias.BaseType, nil)
		}
	}
	if strings.HasPrefix(typeName, "STRING(") {
		var n int
		if _, err2 := fmt.Sscanf(typeName, "STRING(%d)", &n); err2 != nil || n <= 0 {
			return 0, 0, fmt.Errorf("invalid string type %q", typeName)
		}
		return uint32(n + 1), 1, nil
	}
	switch typeName {
	case "BOOL", "BYTE", "SINT", "USINT":
		return 1, 1, nil
	case "WORD", "UINT", "INT":
		return 2, 2, nil
	case "DWORD", "UDINT", "DINT", "REAL", "TIME", "TOD", "DATE", "DT":
		return 4, 4, nil
	case "LREAL":
		return 8, 8, nil
	default:
		return 0, 0, fmt.Errorf("unsupported type %q", typeName)
	}
}

// ComputeLayout walks a Node tree and assigns byte offsets to every leaf,
// applying TwinCAT default pack mode 0 (natural C-struct alignment) rules:
//
//   - Each field is placed at the next offset that is a multiple of its natural
//     alignment.
//   - A struct's start is aligned to the maximum alignment of its members
//     (recursively into sub-structs).
//   - A struct's total size is padded to a multiple of that maximum alignment
//     (tail padding), which ensures correct layout when structs are arrayed.
//   - Array elements use the aligned (padded) element size so that each element
//     starts on the correct boundary.
//
// pad entries occupy space exactly like regular fields and are emitted as
// LayoutPins with Dir == DirPad. Auto-alignment is applied to them too,
// which means that if a config already contains correctly-placed pad entries
// the result is identical (idempotent).
//
// The optional aliases parameter provides @type alias resolution so that
// user-defined type names (e.g. "EN_DISP_MSGTYPE") are correctly mapped to
// their base types for size/alignment computation.
func ComputeLayout(roots []*Node, aliases ...TypeAliasMap) ([]LayoutPin, error) {
	var am TypeAliasMap
	if len(aliases) > 0 {
		am = aliases[0]
	}
	var pins []LayoutPin
	offset := uint32(0)
	for _, node := range roots {
		var err error
		offset, err = layoutNode(node, offset, "", "", &pins, am)
		if err != nil {
			return nil, err
		}
	}
	return pins, nil
}

// alignUp rounds n up to the next multiple of align (which must be a power of 2).
func alignUp(n, align uint32) uint32 {
	if align <= 1 {
		return n
	}
	return (n + align - 1) &^ (align - 1)
}

// nodeMaxAlign returns the natural alignment of node:
// for a leaf it is the alignment of the field type;
// for a container it is the maximum alignment of all descendants.
func nodeMaxAlign(node *Node, aliases TypeAliasMap) (uint32, error) {
	if len(node.Children) == 0 {
		_, al, err := typeSizeResolved(node.TypeName, aliases)
		return al, err
	}
	var maxAl uint32 = 1
	for _, child := range node.Children {
		al, err := nodeMaxAlign(child, aliases)
		if err != nil {
			return 0, err
		}
		if al > maxAl {
			maxAl = al
		}
	}
	return maxAl, nil
}

// layoutNode places a single node starting at offset (aligning as needed) and
// returns the next available offset. It appends LayoutPins to *pins.
func layoutNode(node *Node, offset uint32, halPfx, adsPfx string, pins *[]LayoutPin, aliases TypeAliasMap) (uint32, error) {
	halName := joinName(halPfx, node.Name)
	adsName := joinName(adsPfx, node.Name)

	if len(node.Children) == 0 {
		// Leaf node.
		sz, al, err := typeSizeResolved(node.TypeName, aliases)
		if err != nil {
			return 0, fmt.Errorf("field %q: %w", node.Name, err)
		}
		offset = alignUp(offset, al)
		if pins != nil {
			*pins = append(*pins, LayoutPin{
				Dir:      node.Dir,
				HALPath:  halName,
				ADSName:  adsName,
				TypeName: node.TypeName,
				Offset:   offset,
				Size:     sz,
				Align:    al,
			})
		}
		return offset + sz, nil
	}

	if node.ArrayStart > 0 {
		return layoutArray(node, offset, halName, adsName, pins, aliases)
	}
	// Struct container.
	end, _, err := layoutStruct(node.Children, offset, halName, adsName, pins, aliases)
	return end, err
}

// layoutStruct lays out a sequence of children as a C struct starting at
// start. It aligns the struct start to the struct's max alignment, lays out
// each member (with alignment gaps), then pads the end to the max alignment.
// Returns (endOffset, maxAlignment, error).
func layoutStruct(children []*Node, start uint32, halPfx, adsPfx string, pins *[]LayoutPin, aliases TypeAliasMap) (uint32, uint32, error) {
	// Compute struct max alignment from all members.
	var maxAl uint32 = 1
	for _, child := range children {
		al, err := nodeMaxAlign(child, aliases)
		if err != nil {
			return 0, 0, err
		}
		if al > maxAl {
			maxAl = al
		}
	}

	// Align struct start.
	offset := alignUp(start, maxAl)

	// Layout each member.
	for _, child := range children {
		var err error
		offset, err = layoutNode(child, offset, halPfx, adsPfx, pins, aliases)
		if err != nil {
			return 0, 0, err
		}
	}

	// Tail padding: ensure struct size is a multiple of maxAl.
	offset = alignUp(offset, maxAl)
	return offset, maxAl, nil
}

// layoutArray lays out all elements of an array node. It performs a dry run
// to determine the element size (including tail padding), then places each
// element at consecutive aligned addresses.
func layoutArray(node *Node, offset uint32, halPfx, adsPfx string, pins *[]LayoutPin, aliases TypeAliasMap) (uint32, error) {
	// Dry run: compute element size and alignment starting at offset 0.
	elemEnd, elemAl, err := layoutStruct(node.Children, 0, "", "", nil, aliases)
	if err != nil {
		return 0, err
	}
	elemSz := elemEnd // start=0 so end == size (includes tail padding)

	// Align array start to element alignment.
	offset = alignUp(offset, elemAl)

	for i := node.ArrayStart; i <= node.ArrayEnd; i++ {
		elemBase := offset
		halElem := fmt.Sprintf("%s.%d", halPfx, i)
		adsElem := fmt.Sprintf("%s[%d]", adsPfx, i)

		if _, _, err := layoutStruct(node.Children, elemBase, halElem, adsElem, pins, aliases); err != nil {
			return 0, err
		}
		offset = elemBase + elemSz
	}
	return offset, nil
}

// joinName returns "prefix.name" or just "name" if prefix is empty.
func joinName(prefix, name string) string {
	if prefix == "" {
		return name
	}
	return prefix + "." + name
}
