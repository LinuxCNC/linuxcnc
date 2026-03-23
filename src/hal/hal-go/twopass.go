package hal

import (
	"fmt"
	"sort"
	"strconv"
	"strings"
)

// TwopassLoadRT represents a collected loadrt command with its arguments.
type TwopassLoadRT struct {
	ModName string
	Args    map[string]string // key=value args
	Names   []string          // accumulated names= values
	Count   int               // maximum count= value seen
}

// TwopassCollector collects loadrt/loadusr commands during pass 0.
type TwopassCollector struct {
	loadrtMap   map[string]*TwopassLoadRT // module name → merged loadrt
	loadrtOrder []string                  // preserve first-seen order
}

// NewTwopassCollector creates a new collector for twopass processing.
func NewTwopassCollector() *TwopassCollector {
	return &TwopassCollector{
		loadrtMap: make(map[string]*TwopassLoadRT),
	}
}

// CollectLoadRT adds a loadrt command to the collector, merging with any
// existing entry for the same module.
func (c *TwopassCollector) CollectLoadRT(modName string, args []string) {
	existing, ok := c.loadrtMap[modName]
	if !ok {
		existing = &TwopassLoadRT{
			ModName: modName,
			Args:    make(map[string]string),
		}
		c.loadrtMap[modName] = existing
		c.loadrtOrder = append(c.loadrtOrder, modName)
	}

	for _, arg := range args {
		key, value, hasEquals := strings.Cut(arg, "=")
		if !hasEquals {
			continue
		}

		switch key {
		case "names":
			// Accumulate unique names
			for _, name := range strings.Split(value, ",") {
				name = strings.TrimSpace(name)
				if name != "" {
					found := false
					for _, n := range existing.Names {
						if n == name {
							found = true
							break
						}
					}
					if !found {
						existing.Names = append(existing.Names, name)
					}
				}
			}
		case "count":
			if n, err := parseInt(value); err == nil && n > existing.Count {
				existing.Count = n
			}
		case "num_chan":
			if n, err := parseInt(value); err == nil {
				if cur, ok := existing.Args[key]; ok {
					if curN, err := parseInt(cur); err == nil && n <= curN {
						continue
					}
				}
				existing.Args[key] = value
			}
		default:
			// First-wins for other parameters
			if _, ok := existing.Args[key]; !ok {
				existing.Args[key] = value
			}
		}
	}
}

// MergedLoadRTCommands returns the merged loadrt commands as string slices
// (each slice is [modname, arg1, arg2, ...]) in first-seen order.
func (c *TwopassCollector) MergedLoadRTCommands() [][]string {
	var result [][]string
	for _, modName := range c.loadrtOrder {
		entry := c.loadrtMap[modName]
		cmd := []string{entry.ModName}

		if len(entry.Names) > 0 {
			cmd = append(cmd, fmt.Sprintf("names=%s", strings.Join(entry.Names, ",")))
		}
		if entry.Count > 0 {
			cmd = append(cmd, fmt.Sprintf("count=%d", entry.Count))
		}
		// Sort keys for deterministic output
		keys := make([]string, 0, len(entry.Args))
		for k := range entry.Args {
			keys = append(keys, k)
		}
		sort.Strings(keys)
		for _, k := range keys {
			cmd = append(cmd, fmt.Sprintf("%s=%s", k, entry.Args[k]))
		}

		result = append(result, cmd)
	}
	return result
}

// CollectLoadRTToken converts a LoadRTToken into the CollectLoadRT call format,
// enabling ParseResult execution to feed LoadRTToken structs to the twopass collector.
func (c *TwopassCollector) CollectLoadRTToken(tok *LoadRTToken) {
	var args []string
	if tok.Count > 0 {
		args = append(args, fmt.Sprintf("count=%d", tok.Count))
	}
	if len(tok.Names) > 0 {
		args = append(args, fmt.Sprintf("names=%s", strings.Join(tok.Names, ",")))
	}
	for k, v := range tok.Params {
		args = append(args, fmt.Sprintf("%s=%s", k, v))
	}
	c.CollectLoadRT(tok.Comp, args)
}

// IsLoadRT returns true if the tokenized command line is a loadrt command.
func IsLoadRT(tokens []string) bool {
	return len(tokens) > 0 && tokens[0] == "loadrt"
}

// parseInt is a helper that parses a trimmed decimal integer string.
func parseInt(s string) (int, error) {
	return strconv.Atoi(strings.TrimSpace(s))
}
