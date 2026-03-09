package main

import (
	"bufio"
	"encoding/binary"
	"fmt"
	"io"
	"strconv"
	"strings"
)

// PinDir is the direction of a HAL pin from the component's perspective.
type PinDir string

const (
	// DirIn means the HAL component receives this value (HMI writes TO the PLC/HAL).
	DirIn PinDir = "in"
	// DirOut means the HAL component produces this value (HMI reads FROM the PLC/HAL).
	DirOut PinDir = "out"
	// DirInOut means the HAL component can both read and write the value (bidirectional).
	DirInOut PinDir = "inout"
	// DirPad marks a padding/reserved field that occupies space in the ADS process
	// image but does not create a HAL pin. Reads return zero bytes; writes are
	// silently discarded.
	DirPad PinDir = "pad"
)

// TypeAlias maps a custom/derived type name to its base ADS type and data-type GUID.
// @type directives in the config file populate this.
type TypeAlias struct {
	// BaseType is the underlying ADS primitive type, e.g. "WORD".
	BaseType string
	// GUID is the 16-byte data type GUID in Microsoft COM wire format.
	// All zeros if no GUID was specified.
	GUID [16]byte
}

// TypeAliasMap is a map from alias name to TypeAlias definition.
type TypeAliasMap map[string]TypeAlias

// Node is a parsed element from the config file.
//
// Leaf nodes (Dir != "") represent HAL pins or padding fields.
// Container nodes (Children != nil) represent structs or array templates.
//
// For array containers, ArrayStart and ArrayEnd give the element index range
// (e.g. [1..4] -> ArrayStart=1, ArrayEnd=4); Children holds the per-element
// template (shared across all instances).
type Node struct {
	// Name is the field name, e.g. "bGlobalErr", "aPools", "stMsg".
	Name string
	// Dir is the direction for leaf nodes ("in", "out", "inout", "pad").
	// Empty for container nodes.
	Dir PinDir
	// TypeName is the ADS/TwinCAT type name for leaf nodes, e.g. "BOOL", "REAL".
	// When a @type alias is referenced, TypeName holds the alias name (not the base type).
	// Empty for container nodes.
	TypeName string
	// ArrayStart and ArrayEnd are >0 for array containers, e.g. [1..4] gives
	// ArrayStart=1, ArrayEnd=4. Both are 0 for plain struct containers.
	ArrayStart int
	ArrayEnd   int
	// Children holds child nodes for containers (nil for leaves).
	Children []*Node
}

// configLine is a pre-processed line from the config file.
type configLine struct {
	lineNo  int
	depth   int    // indent depth (1 unit = auto-detected indent size)
	trimmed string // content without leading whitespace
}

// ParseTreeWithAliases reads the HAL-ADS config format from r and returns both
// the TypeAliasMap (from @type directives) and the tree of Nodes representing
// the symbol hierarchy.
//
// @type directives must appear at depth 0 (before any indented block) and have
// the form:
//
//	@type <AliasName> <BaseType> <GUID>
//
// Example:
//
//	@type EN_DISP_MSGTYPE WORD 96656ea5-0db7-49b0-86ec-56cef26b56d0
func ParseTreeWithAliases(r io.Reader) (TypeAliasMap, []*Node, error) {
	aliases, lines, err := readConfigLinesWithAliases(r)
	if err != nil {
		return nil, nil, err
	}
	var roots []*Node
	idx := 0
	if err := parseTreeBlock(lines, &idx, -1, &roots); err != nil {
		return nil, nil, err
	}
	return aliases, roots, nil
}

// ParseTree reads the HAL-ADS config format from r and returns the tree of
// Nodes representing the symbol hierarchy.
//
// @type directives are parsed and silently discarded; use ParseTreeWithAliases
// to retrieve the alias registry alongside the node tree.
//
// Format (any consistent indentation — spaces or tabs):
//
// ContainerName
//
//	in leafName TYPE
//	out leafName TYPE
//	inout leafName TYPE
//	pad leafName TYPE
//	ArrayName[start..end]
//	  in leafName TYPE
func ParseTree(r io.Reader) ([]*Node, error) {
	_, nodes, err := ParseTreeWithAliases(r)
	return nodes, err
}

// readConfigLinesWithAliases reads and pre-processes all non-blank, non-comment
// lines. It strips and collects @type directives into a TypeAliasMap, then
// returns the remaining lines for tree parsing.
func readConfigLinesWithAliases(r io.Reader) (TypeAliasMap, []configLine, error) {
	scanner := bufio.NewScanner(r)
	aliases := make(TypeAliasMap)
	var lines []configLine
	lineNo := 0
	indentUnit := 0       // chars per depth level (0 = not yet detected)
	indentChar := byte(0) // ' ' or '\t' (0 = not yet detected)

	for scanner.Scan() {
		lineNo++
		rawLine := scanner.Text()
		trimmed := strings.TrimSpace(rawLine)
		if trimmed == "" || strings.HasPrefix(trimmed, "#") {
			continue
		}

		// Handle @type directives at depth 0 (no leading whitespace).
		if strings.HasPrefix(trimmed, "@type ") {
			tokens := strings.Fields(trimmed)
			if len(tokens) != 4 {
				return nil, nil, fmt.Errorf("line %d: @type requires exactly 3 arguments: @type <AliasName> <BaseType> <GUID>", lineNo)
			}
			aliasName := strings.ToUpper(tokens[1])
			baseType := strings.ToUpper(tokens[2])
			guid, err := parseGUID(tokens[3])
			if err != nil {
				return nil, nil, fmt.Errorf("line %d: @type %q invalid GUID: %w", lineNo, aliasName, err)
			}
			aliases[aliasName] = TypeAlias{BaseType: baseType, GUID: guid}
			continue
		}

		// Count leading whitespace characters.
		wsLen := 0
		for wsLen < len(rawLine) && (rawLine[wsLen] == ' ' || rawLine[wsLen] == '\t') {
			wsLen++
		}

		depth := 0
		if wsLen > 0 {
			if indentUnit == 0 {
				// First indented line: record indent character and unit size.
				indentChar = rawLine[0]
				indentUnit = wsLen
			}
			// Verify all leading whitespace uses the same character.
			for i := 0; i < wsLen; i++ {
				if rawLine[i] != indentChar {
					return nil, nil, fmt.Errorf("line %d: mixed indentation (indent uses %s but line has %s)",
						lineNo, indentCharName(indentChar), indentCharName(rawLine[i]))
				}
			}
			// Depth must be an exact multiple of the indent unit.
			if wsLen%indentUnit != 0 {
				return nil, nil, fmt.Errorf("line %d: indentation of %d %s(s) is not a multiple of the indent unit (%d)",
					lineNo, wsLen, indentCharName(indentChar), indentUnit)
			}
			depth = wsLen / indentUnit
		}

		lines = append(lines, configLine{lineNo: lineNo, depth: depth, trimmed: trimmed})
	}
	if err := scanner.Err(); err != nil {
		return nil, nil, fmt.Errorf("config read error: %w", err)
	}
	return aliases, lines, nil
}

// readConfigLines reads and pre-processes all non-blank, non-comment lines.
// It auto-detects the indent style from the first indented line (spaces or
// tabs) and rejects mixed indentation or inconsistent indent depths.
func readConfigLines(r io.Reader) ([]configLine, error) {
	_, lines, err := readConfigLinesWithAliases(r)
	return lines, err
}

// parseGUID parses a GUID string in the format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
// (Microsoft COM / Windows GUID format) into a 16-byte array.
//
// Wire encoding:
//   - Data1 (4 bytes, little-endian)
//   - Data2 (2 bytes, little-endian)
//   - Data3 (2 bytes, little-endian)
//   - Data4 (8 bytes, as-is / big-endian)
func parseGUID(s string) ([16]byte, error) {
	var g [16]byte
	parts := strings.Split(s, "-")
	if len(parts) != 5 ||
		len(parts[0]) != 8 || len(parts[1]) != 4 || len(parts[2]) != 4 ||
		len(parts[3]) != 4 || len(parts[4]) != 12 {
		return g, fmt.Errorf("expected format xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx, got %q", s)
	}
	// Data1: uint32 little-endian
	v1, err := strconv.ParseUint(parts[0], 16, 32)
	if err != nil {
		return g, fmt.Errorf("Data1: %w", err)
	}
	binary.LittleEndian.PutUint32(g[0:4], uint32(v1))
	// Data2: uint16 little-endian
	v2, err := strconv.ParseUint(parts[1], 16, 16)
	if err != nil {
		return g, fmt.Errorf("Data2: %w", err)
	}
	binary.LittleEndian.PutUint16(g[4:6], uint16(v2))
	// Data3: uint16 little-endian
	v3, err := strconv.ParseUint(parts[2], 16, 16)
	if err != nil {
		return g, fmt.Errorf("Data3: %w", err)
	}
	binary.LittleEndian.PutUint16(g[6:8], uint16(v3))
	// Data4: 8 bytes in wire order (big-endian / as-is in string)
	data4 := parts[3] + parts[4]
	for i := 0; i < 8; i++ {
		b, err := strconv.ParseUint(data4[i*2:i*2+2], 16, 8)
		if err != nil {
			return g, fmt.Errorf("Data4[%d]: %w", i, err)
		}
		g[8+i] = byte(b)
	}
	return g, nil
}

// indentCharName returns a human-readable name for an indentation character.
func indentCharName(ch byte) string {
	if ch == '\t' {
		return "tab"
	}
	return "space"
}

// parseTreeBlock processes config lines starting at *idx, adding discovered
// Nodes to *nodes. It stops when it encounters a line at depth <= minDepth
// (that line is left for the caller to process).
func parseTreeBlock(lines []configLine, idx *int, minDepth int, nodes *[]*Node) error {
	for *idx < len(lines) {
		cl := lines[*idx]
		if cl.depth <= minDepth {
			// This line belongs to a parent block; stop here.
			return nil
		}

		tokens := strings.Fields(cl.trimmed)
		if len(tokens) == 0 {
			*idx++
			continue
		}

		// Leaf line (starts with a direction keyword).
		if tokens[0] == "in" || tokens[0] == "out" || tokens[0] == "inout" || tokens[0] == "pad" {
			if len(tokens) < 3 {
				return fmt.Errorf("line %d: leaf line requires direction, name, and type", cl.lineNo)
			}
			*nodes = append(*nodes, &Node{
				Name:     tokens[1],
				Dir:      PinDir(tokens[0]),
				TypeName: parseTypeName(tokens[2:]),
			})
			*idx++
			continue
		}

		// Container line: plain struct or array.
		node, err := parseContainerNode(tokens[0], cl.lineNo)
		if err != nil {
			return err
		}
		*idx++ // consume the container line

		var children []*Node
		if err := parseTreeBlock(lines, idx, cl.depth, &children); err != nil {
			return err
		}
		node.Children = children
		*nodes = append(*nodes, node)
	}
	return nil
}

// parseContainerNode parses a container token (plain name or "name[start..end]")
// and returns a Node with Name, ArrayStart, ArrayEnd set (Children left nil).
func parseContainerNode(token string, lineNo int) (*Node, error) {
	lb := strings.Index(token, "[")
	if lb == -1 {
		return &Node{Name: token}, nil
	}
	rb := strings.Index(token, "]")
	if rb == -1 || rb < lb {
		return nil, fmt.Errorf("line %d: invalid array syntax %q", lineNo, token)
	}
	baseName := token[:lb]
	rangeStr := token[lb+1 : rb]
	parts := strings.SplitN(rangeStr, "..", 2)
	if len(parts) != 2 {
		return nil, fmt.Errorf("line %d: invalid array range %q (expected start..end)", lineNo, rangeStr)
	}
	var start, end int
	if _, err := fmt.Sscanf(parts[0], "%d", &start); err != nil {
		return nil, fmt.Errorf("line %d: invalid array start %q", lineNo, parts[0])
	}
	if _, err := fmt.Sscanf(parts[1], "%d", &end); err != nil {
		return nil, fmt.Errorf("line %d: invalid array end %q", lineNo, parts[1])
	}
	if start > end {
		return nil, fmt.Errorf("line %d: array range start %d > end %d", lineNo, start, end)
	}
	return &Node{Name: baseName, ArrayStart: start, ArrayEnd: end}, nil
}

// parseTypeName reconstructs the type name from the remaining tokens on a leaf line.
// Handles "bool", "dint", "string(32)" etc. Returns the normalised upper-case name.
func parseTypeName(tokens []string) string {
	if len(tokens) == 0 {
		return ""
	}
	// Normalize to upper case for ADS type names.
	return strings.ToUpper(strings.Join(tokens, ""))
}
