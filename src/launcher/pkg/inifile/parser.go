package inifile

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"
)

// substitutePattern matches [SECTION]KEY references used in HAL files.
// TODO: Consider adding '-' to the key character class if LinuxCNC
// INI keys with hyphens are encountered in practice.
var substitutePattern = regexp.MustCompile(`\[([^\]]+)\]([A-Za-z0-9_]+)`)

// Parse reads and parses an INI file, recursively handling #INCLUDE directives.
// Relative paths in #INCLUDE are resolved relative to the directory of the
// file containing the directive.  Environment variables (e.g. $HOME) in
// #INCLUDE paths are expanded before resolution.
func Parse(filename string) (*IniFile, error) {
	abs, err := filepath.Abs(filename)
	if err != nil {
		return nil, fmt.Errorf("inifile: resolving path %q: %w", filename, err)
	}
	ini := &IniFile{sourceFile: abs}
	if err := ini.parseFile(abs, map[string]bool{}); err != nil {
		return nil, err
	}
	return ini, nil
}

// parseFile parses a single file and appends its content into ini.
// visited tracks already-included files to detect simple import cycles.
func (ini *IniFile) parseFile(filename string, visited map[string]bool) error {
	if visited[filename] {
		return fmt.Errorf("inifile: circular #INCLUDE detected: %s", filename)
	}
	visited[filename] = true

	f, err := os.Open(filename)
	if err != nil {
		return fmt.Errorf("inifile: opening %q: %w", filename, err)
	}
	defer f.Close()

	dir := filepath.Dir(filename)
	var currentSection *Section

	scanner := bufio.NewScanner(f)
	lineNum := 0
	for scanner.Scan() {
		lineNum++
		line := strings.TrimSpace(scanner.Text())

		// Empty line – skip.
		if line == "" {
			continue
		}

		// #INCLUDE directive (must come before the generic # comment check).
		if strings.HasPrefix(line, "#INCLUDE") {
			rest := strings.TrimSpace(line[len("#INCLUDE"):])
			if rest == "" {
				return fmt.Errorf("inifile: %s:%d: empty #INCLUDE path", filename, lineNum)
			}
			// Expand environment variables in the path.
			rest = os.ExpandEnv(rest)
			if !filepath.IsAbs(rest) {
				rest = filepath.Join(dir, rest)
			}
			abs, err := filepath.Abs(rest)
			if err != nil {
				return fmt.Errorf("inifile: %s:%d: resolving #INCLUDE path %q: %w", filename, lineNum, rest, err)
			}
			// Pass a copy of visited so sibling includes don't interfere.
			visited2 := make(map[string]bool, len(visited))
			for k, v := range visited {
				visited2[k] = v
			}
			if err := ini.parseFile(abs, visited2); err != nil {
				return err
			}
			// Restore current section pointer – parseFile may have appended
			// new sections; our current section still lives at its original index.
			if currentSection != nil {
				// Re-point to the last section that matches our current name
				// (section may have been continued by the included file – keep
				// appending to the latest section with that name to stay correct).
				for i := len(ini.Sections) - 1; i >= 0; i-- {
					if ini.Sections[i].Name == currentSection.Name {
						currentSection = &ini.Sections[i]
						break
					}
				}
			}
			continue
		}

		// Generic comment lines (# not followed by INCLUDE, and ; lines).
		if strings.HasPrefix(line, "#") || strings.HasPrefix(line, ";") {
			continue
		}

		// Section header: [NAME]
		if strings.HasPrefix(line, "[") {
			end := strings.Index(line, "]")
			if end < 0 {
				return fmt.Errorf("inifile: %s:%d: malformed section header %q", filename, lineNum, line)
			}
			name := line[1:end]
			ini.Sections = append(ini.Sections, Section{Name: name})
			currentSection = &ini.Sections[len(ini.Sections)-1]
			continue
		}

		// Key-value pair: KEY = VALUE
		eqIdx := strings.Index(line, "=")
		if eqIdx < 0 {
			// Not a comment, not a section, not a key-value – ignore unknown lines.
			continue
		}
		key := strings.TrimSpace(line[:eqIdx])
		rawVal := strings.TrimSpace(line[eqIdx+1:])
		value := stripInlineComment(rawVal)

		if currentSection == nil {
			// Key before any section header – create an anonymous section.
			ini.Sections = append(ini.Sections, Section{Name: ""})
			currentSection = &ini.Sections[len(ini.Sections)-1]
		}
		currentSection.Entries = append(currentSection.Entries, Entry{Key: key, Value: value})
	}
	if err := scanner.Err(); err != nil {
		return fmt.Errorf("inifile: reading %q: %w", filename, err)
	}
	return nil
}

// stripInlineComment removes trailing inline comments from a value string.
// Per LinuxCNC convention:
//   - A ';' anywhere after the value starts an inline comment.
//   - A '#' starts an inline comment only when preceded by whitespace
//     (to avoid misinterpreting values that legitimately start with '#').
//
// The function finds whichever valid comment marker appears first and
// truncates there, so mixed cases like "value #comment ; more" are handled
// correctly (truncated at the '#', not the later ';').
func stripInlineComment(s string) string {
	minIdx := len(s)

	// Check for ';' inline comment.
	if idx := strings.Index(s, ";"); idx >= 0 && idx < minIdx {
		minIdx = idx
	}
	// Check for '#' inline comment preceded by whitespace (" #" or "\t#").
	for _, prefix := range []string{" #", "\t#"} {
		if idx := strings.Index(s, prefix); idx >= 0 && idx < minIdx {
			minIdx = idx
		}
	}
	if minIdx < len(s) {
		s = strings.TrimRight(s[:minIdx], " \t")
	}
	return s
}

// --------------------------------------------------------------------------
// Lookup helpers
// --------------------------------------------------------------------------

// Get returns the first value for the given section and key, or an empty
// string if not found.
func (ini *IniFile) Get(section, key string) string {
	for i := range ini.Sections {
		if ini.Sections[i].Name != section {
			continue
		}
		for j := range ini.Sections[i].Entries {
			if ini.Sections[i].Entries[j].Key == key {
				return ini.Sections[i].Entries[j].Value
			}
		}
	}
	return ""
}

// GetAll returns all values for the given section and key, in the order they
// appear in the file.  Returns nil if the key is not present.
func (ini *IniFile) GetAll(section, key string) []string {
	var result []string
	for i := range ini.Sections {
		if ini.Sections[i].Name != section {
			continue
		}
		for j := range ini.Sections[i].Entries {
			if ini.Sections[i].Entries[j].Key == key {
				result = append(result, ini.Sections[i].Entries[j].Value)
			}
		}
	}
	return result
}

// GetN returns the n-th occurrence of key in section (1-based), matching the
// behaviour of `inivar -num N`.  Returns an empty string if there is no n-th
// occurrence.
func (ini *IniFile) GetN(section, key string, n int) string {
	count := 0
	for i := range ini.Sections {
		if ini.Sections[i].Name != section {
			continue
		}
		for j := range ini.Sections[i].Entries {
			if ini.Sections[i].Entries[j].Key == key {
				count++
				if count == n {
					return ini.Sections[i].Entries[j].Value
				}
			}
		}
	}
	return ""
}

// GetWithFallback tries each (section, key) pair in order and returns the
// first match, along with a boolean indicating whether any match was found.
// Each element of pairs must be a two-element slice [section, key].
// This mirrors the GetFromIniEx behaviour used in the bash script.
func (ini *IniFile) GetWithFallback(pairs [][2]string) (string, bool) {
	for _, p := range pairs {
		for i := range ini.Sections {
			if ini.Sections[i].Name != p[0] {
				continue
			}
			for j := range ini.Sections[i].Entries {
				if ini.Sections[i].Entries[j].Key == p[1] {
					return ini.Sections[i].Entries[j].Value, true
				}
			}
		}
	}
	return "", false
}

// Set updates the first occurrence of key in the named section to the given
// value.  If the key does not exist in the section, a new entry is appended.
// If the section itself does not exist, it is created with the single entry.
// Returns true if an existing entry was updated, false if a new entry was
// added.
//
// When the same section name appears more than once (e.g. via #INCLUDE), only
// entries in the first matching section are considered.
func (ini *IniFile) Set(section, key, value string) bool {
	for i := range ini.Sections {
		if ini.Sections[i].Name != section {
			continue
		}
		for j := range ini.Sections[i].Entries {
			if ini.Sections[i].Entries[j].Key == key {
				ini.Sections[i].Entries[j].Value = value
				return true
			}
		}
		// Section exists but key not found — append to first matching section.
		ini.Sections[i].Entries = append(ini.Sections[i].Entries, Entry{Key: key, Value: value})
		return false
	}
	// Section does not exist — create it with the single entry.
	ini.Sections = append(ini.Sections, Section{
		Name:    section,
		Entries: []Entry{{Key: key, Value: value}},
	})
	return false
}

// Substitute replaces [SECTION]KEY patterns in input with the corresponding
// values from the INI file.  This is used when processing HAL files that
// reference INI variables, e.g.:
//
//	loadrt motmod servo_period_nsec=[EMCMOT]SERVO_PERIOD
func (ini *IniFile) Substitute(input string) string {
	return substitutePattern.ReplaceAllStringFunc(input, func(match string) string {
		parts := substitutePattern.FindStringSubmatch(match)
		if len(parts) != 3 {
			return match
		}
		val := ini.Get(parts[1], parts[2])
		if val == "" {
			return match
		}
		return val
	})
}
