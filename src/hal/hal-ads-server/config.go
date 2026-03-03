package main

import (
	"bufio"
	"fmt"
	"io"
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
	depth   int    // indent depth (1 unit = 2 spaces)
	trimmed string // content without leading whitespace
}

// ParseTree reads the HAL-ADS config format from r and returns the tree of
// Nodes representing the symbol hierarchy.
//
// Format (2-space indentation):
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
	lines, err := readConfigLines(r)
	if err != nil {
		return nil, err
	}
	var roots []*Node
	idx := 0
	if err := parseTreeBlock(lines, &idx, -1, &roots); err != nil {
		return nil, err
	}
	return roots, nil
}

// readConfigLines reads and pre-processes all non-blank, non-comment lines.
func readConfigLines(r io.Reader) ([]configLine, error) {
	scanner := bufio.NewScanner(r)
	var lines []configLine
	lineNo := 0
	for scanner.Scan() {
		lineNo++
		rawLine := scanner.Text()
		trimmed := strings.TrimSpace(rawLine)
		if trimmed == "" || strings.HasPrefix(trimmed, "#") {
			continue
		}
		// Calculate indent depth: count leading 2-space pairs.
		depth := 0
		for depth*2+2 <= len(rawLine) && strings.HasPrefix(rawLine[depth*2:], "  ") {
			depth++
		}
		lines = append(lines, configLine{lineNo: lineNo, depth: depth, trimmed: trimmed})
	}
	if err := scanner.Err(); err != nil {
		return nil, fmt.Errorf("config read error: %w", err)
	}
	return lines, nil
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
