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
)

// ConfigPin describes a single leaf symbol that maps to a HAL pin.
type ConfigPin struct {
	// Dir is the HAL pin direction ("in" or "out").
	Dir PinDir
	// HALPath is the dot-separated path for the HAL pin name, e.g.
	// "stDISPLAY_DATA.stPOOL.1.bReady".
	HALPath string
	// ADSName is the full ADS symbol name with bracket notation, e.g.
	// "stDISPLAY_DATA.stPOOL[1].bReady".
	ADSName string
	// TypeName is the ADS/TwinCAT type name, e.g. "BOOL", "DINT", "STRING(32)".
	TypeName string
}

// configLine is a pre-processed line from the config file.
type configLine struct {
	lineNo  int
	depth   int    // indent depth (1 unit = 2 spaces)
	trimmed string // content without leading whitespace
}

// pathFrame tracks one level of the nesting hierarchy during parsing.
type pathFrame struct {
	// halSeg is the path segment used in HAL pin names (e.g. "stPOOL" or "stPOOL.1").
	halSeg string
	// adsSeg is the path segment used in ADS symbol names (e.g. "stPOOL" or "stPOOL[1]").
	adsSeg string
	// depth is the indent depth at which this frame was pushed.
	depth int
}

// ParseConfig reads the HAL-ADS config format from r and returns the list of
// leaf symbols to create as HAL pins.
//
// Format (2-space indentation):
//
//	ContainerName
//	  in leafName TYPE
//	  out leafName TYPE
//	  ArrayName[start..end]
//	    in leafName TYPE
func ParseConfig(r io.Reader) ([]ConfigPin, error) {
	lines, err := readConfigLines(r)
	if err != nil {
		return nil, err
	}
	stack := []pathFrame{{halSeg: "", adsSeg: "", depth: -1}}
	var pins []ConfigPin
	idx := 0
	if err := parseBlock(lines, &idx, -1, stack, &pins); err != nil {
		return nil, err
	}
	return pins, nil
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

// parseBlock processes lines from idx up to (but not including) the first line at
// depth <= minDepth, appending any discovered pins to *pins.
// It advances *idx past all consumed lines.
func parseBlock(lines []configLine, idx *int, minDepth int, stack []pathFrame, pins *[]ConfigPin) error {
	for *idx < len(lines) {
		cl := lines[*idx]
		if cl.depth <= minDepth {
			// This line belongs to a parent block; leave it for the caller.
			return nil
		}

		// Pop stack frames that are deeper than or equal to current depth.
		for len(stack) > 1 && stack[len(stack)-1].depth >= cl.depth {
			stack = stack[:len(stack)-1]
		}

		tokens := strings.Fields(cl.trimmed)
		if len(tokens) == 0 {
			*idx++
			continue
		}

		// Leaf line (starts with "in" or "out").
		if tokens[0] == "in" || tokens[0] == "out" {
			if len(tokens) < 3 {
				return fmt.Errorf("line %d: leaf line requires direction, name, and type", cl.lineNo)
			}
			dir := PinDir(tokens[0])
			name := tokens[1]
			typeName := parseTypeName(tokens[2:])
			halPath := buildPath(stack, name, false)
			adsName := buildPath(stack, name, true)
			*pins = append(*pins, ConfigPin{
				Dir:      dir,
				HALPath:  halPath,
				ADSName:  adsName,
				TypeName: typeName,
			})
			*idx++
			continue
		}

		// Container line: plain struct or array.
		expanded, err := expandContainer(tokens[0])
		if err != nil {
			return fmt.Errorf("line %d: %w", cl.lineNo, err)
		}

		*idx++ // consume this container line

		if len(expanded) == 1 {
			// Simple struct: push a frame and continue parsing child lines.
			stack = append(stack, pathFrame{
				halSeg: expanded[0].halSeg,
				adsSeg: expanded[0].adsSeg,
				depth:  cl.depth,
			})
		} else {
			// Array: collect the sub-block indices, then expand for each instance.
			subStart := *idx
			// Advance idx past all lines belonging to this sub-block.
			for *idx < len(lines) && lines[*idx].depth > cl.depth {
				*idx++
			}
			subLines := lines[subStart:*idx]

			for _, inst := range expanded {
				innerStack := make([]pathFrame, len(stack))
				copy(innerStack, stack)
				innerStack = append(innerStack, pathFrame{
					halSeg: inst.halSeg,
					adsSeg: inst.adsSeg,
					depth:  cl.depth,
				})
				subIdx := 0
				if err := parseBlock(subLines, &subIdx, cl.depth-1, innerStack, pins); err != nil {
					return err
				}
			}
		}
	}
	return nil
}

// containerInstance represents one element of a parsed container path.
type containerInstance struct {
	halSeg string
	adsSeg string
}

// expandContainer parses a container token.
// For "name" it returns [{halSeg:"name", adsSeg:"name"}].
// For "name[1..9]" it returns nine instances.
func expandContainer(token string) ([]containerInstance, error) {
	// Detect array syntax: name[start..end]
	lb := strings.Index(token, "[")
	if lb == -1 {
		return []containerInstance{{halSeg: token, adsSeg: token}}, nil
	}
	rb := strings.Index(token, "]")
	if rb == -1 || rb < lb {
		return nil, fmt.Errorf("invalid array syntax %q", token)
	}
	baseName := token[:lb]
	rangeStr := token[lb+1 : rb]
	parts := strings.SplitN(rangeStr, "..", 2)
	if len(parts) != 2 {
		return nil, fmt.Errorf("invalid array range %q (expected start..end)", rangeStr)
	}
	var start, end int
	if _, err := fmt.Sscanf(parts[0], "%d", &start); err != nil {
		return nil, fmt.Errorf("invalid array start %q", parts[0])
	}
	if _, err := fmt.Sscanf(parts[1], "%d", &end); err != nil {
		return nil, fmt.Errorf("invalid array end %q", parts[1])
	}
	if start > end {
		return nil, fmt.Errorf("array range start %d > end %d", start, end)
	}

	var instances []containerInstance
	for i := start; i <= end; i++ {
		instances = append(instances, containerInstance{
			halSeg: fmt.Sprintf("%s.%d", baseName, i),
			adsSeg: fmt.Sprintf("%s[%d]", baseName, i),
		})
	}
	return instances, nil
}

// buildPath constructs a dot-separated path from the current stack + leaf name.
// If adsNotation is false, HAL dot notation is used (halSeg).
// If adsNotation is true, ADS bracket notation is used (adsSeg).
func buildPath(stack []pathFrame, leafName string, adsNotation bool) string {
	var parts []string
	for _, f := range stack {
		if f.halSeg == "" {
			continue
		}
		if adsNotation {
			parts = append(parts, f.adsSeg)
		} else {
			parts = append(parts, f.halSeg)
		}
	}
	parts = append(parts, leafName)
	return strings.Join(parts, ".")
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

