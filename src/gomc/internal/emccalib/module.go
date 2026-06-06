// Package emccalib implements the live calibration tuning gomod.
//
// It reads the calibreg registry (populated during HAL file loading) to
// discover which HAL pins were set from INI [SECTION]KEY references, then
// exposes REST endpoints to query tunables, apply new values, and save
// changes back to the INI file(s).
package emccalib

import (
	"bufio"
	"fmt"
	"log/slog"
	"os"
	"strconv"
	"strings"
	"sync"

	emccalibapi "github.com/sittner/linuxcnc/src/gomc/generated/gmi/emccalib"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/internal/calibreg"
	"github.com/sittner/linuxcnc/src/gomc/internal/halcmd"
	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

func init() {
	gomc.RegisterModule("emccalib", newEmccalib)
	apiserver.RegisterMeta(emccalibapi.EmccalibMeta)
}

// tunable is an internal representation of a discovered tunable item.
type tunable struct {
	section    string
	key        string
	pin        string
	iniValue   float64
	sourceFile string // file to write back to
	sourceLine int    // line number in that file
}

type emccalib struct {
	logger   *slog.Logger
	ini      *inifile.IniFile
	mu       sync.Mutex
	tunables []tunable           // all discovered tunables
	index    map[string]*tunable // "SECTION\x00KEY" → tunable ptr
}

func newEmccalib(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	e := &emccalib{
		logger: logger,
		ini:    ini,
		index:  make(map[string]*tunable),
	}

	// Build tunable list from calibreg (populated during HAL file loading).
	mappings := calibreg.GetAll()
	for _, m := range mappings {
		t := tunable{
			section:  m.Section,
			key:      m.Key,
			pin:      m.Pin,
			iniValue: m.IniValue,
		}
		// Find provenance — which file and line this entry came from.
		for i := range ini.Sections {
			if ini.Sections[i].Name != m.Section {
				continue
			}
			for j := range ini.Sections[i].Entries {
				entry := &ini.Sections[i].Entries[j]
				if entry.Key == m.Key {
					t.sourceFile = entry.SourceFile
					t.sourceLine = entry.SourceLine
					break
				}
			}
			if t.sourceFile != "" {
				break
			}
		}
		e.tunables = append(e.tunables, t)
		e.index[m.Section+"\x00"+m.Key] = &e.tunables[len(e.tunables)-1]
	}

	logger.Info("emccalib: discovered tunables", "count", len(e.tunables))

	// Register with apiserver.
	reg := apiserver.DefaultRegistry()
	if err := emccalibapi.RegisterEmccalibAPI(reg, "emccalib", e); err != nil {
		return nil, fmt.Errorf("emccalib: register API: %w", err)
	}

	return e, nil
}

func (e *emccalib) Start() error { return nil }
func (e *emccalib) Stop()        {}
func (e *emccalib) Destroy()     {}

// --- EmccalibCallbacks implementation ---

func (e *emccalib) GetTunables() ([]emccalibapi.TunableSection, error) {
	e.mu.Lock()
	defer e.mu.Unlock()

	// Group tunables by section.
	sectionMap := make(map[string]*emccalibapi.TunableSection)
	var sectionOrder []string

	for i := range e.tunables {
		t := &e.tunables[i]
		sec, ok := sectionMap[t.section]
		if !ok {
			sec = &emccalibapi.TunableSection{Name: t.section}
			sectionMap[t.section] = sec
			sectionOrder = append(sectionOrder, t.section)
		}

		// Read current HAL pin value.
		valStr, err := halcmd.GetP(t.pin)
		var val float64
		if err == nil {
			val, _ = strconv.ParseFloat(strings.TrimSpace(valStr), 64)
		}

		sec.Items = append(sec.Items, emccalibapi.TunableItem{
			Section:  t.section,
			Key:      t.key,
			HalPin:   t.pin,
			Value:    val,
			IniValue: t.iniValue,
		})
	}

	// Return in discovery order.
	result := make([]emccalibapi.TunableSection, 0, len(sectionOrder))
	for _, name := range sectionOrder {
		result = append(result, *sectionMap[name])
	}
	return result, nil
}

func (e *emccalib) SetPin(section, key string, value float64) (bool, error) {
	e.mu.Lock()
	t := e.index[section+"\x00"+key]
	e.mu.Unlock()

	if t == nil {
		return false, fmt.Errorf("emccalib: %s/%s not in tunable list", section, key)
	}

	valStr := strconv.FormatFloat(value, 'f', -1, 64)
	if err := halcmd.SetP(t.pin, valStr); err != nil {
		return false, fmt.Errorf("emccalib: setp %s %s: %w", t.pin, valStr, err)
	}
	return true, nil
}

func (e *emccalib) SaveIni() (bool, error) {
	e.mu.Lock()
	defer e.mu.Unlock()

	// Group tunables by source file.
	fileUpdates := make(map[string][]tunable)
	for i := range e.tunables {
		t := &e.tunables[i]
		if t.sourceFile == "" {
			continue
		}
		// Read current value from HAL.
		valStr, err := halcmd.GetP(t.pin)
		if err != nil {
			e.logger.Warn("emccalib: cannot read pin for save", "pin", t.pin, "err", err)
			continue
		}
		val, err := strconv.ParseFloat(strings.TrimSpace(valStr), 64)
		if err != nil {
			continue
		}
		// Only save if value differs from original INI value.
		if val != t.iniValue {
			updated := *t
			updated.iniValue = val // new value to write
			fileUpdates[t.sourceFile] = append(fileUpdates[t.sourceFile], updated)
		}
	}

	// Write each modified file.
	for file, updates := range fileUpdates {
		if err := e.updateINIFile(file, updates); err != nil {
			return false, fmt.Errorf("emccalib: saving %s: %w", file, err)
		}
	}

	// Update in-memory ini_value to reflect the saved state.
	for i := range e.tunables {
		t := &e.tunables[i]
		valStr, err := halcmd.GetP(t.pin)
		if err == nil {
			val, err2 := strconv.ParseFloat(strings.TrimSpace(valStr), 64)
			if err2 == nil {
				t.iniValue = val
			}
		}
	}

	return true, nil
}

func (e *emccalib) Revert(section, key string) (bool, error) {
	e.mu.Lock()
	t := e.index[section+"\x00"+key]
	e.mu.Unlock()

	if t == nil {
		return false, fmt.Errorf("emccalib: %s/%s not in tunable list", section, key)
	}

	valStr := strconv.FormatFloat(t.iniValue, 'f', -1, 64)
	if err := halcmd.SetP(t.pin, valStr); err != nil {
		return false, fmt.Errorf("emccalib: revert setp %s %s: %w", t.pin, valStr, err)
	}
	return true, nil
}

// updateINIFile rewrites a single INI file, replacing values at specific lines.
// Creates a .bak backup before modification.
func (e *emccalib) updateINIFile(path string, updates []tunable) error {
	// Build line→value map for O(1) lookup during scan.
	lineUpdates := make(map[int]string, len(updates))
	for _, u := range updates {
		lineUpdates[u.sourceLine] = strconv.FormatFloat(u.iniValue, 'f', -1, 64)
	}

	// Read original file.
	f, err := os.Open(path)
	if err != nil {
		return err
	}
	defer f.Close()

	var lines []string
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		lines = append(lines, scanner.Text())
	}
	if err := scanner.Err(); err != nil {
		return err
	}
	f.Close()

	// Apply updates.
	for lineNum, newVal := range lineUpdates {
		idx := lineNum - 1 // 1-based → 0-based
		if idx < 0 || idx >= len(lines) {
			continue
		}
		lines[idx] = replaceINIValue(lines[idx], newVal)
	}

	// Create backup.
	bakPath := path + ".bak"
	if err := copyFile(path, bakPath); err != nil {
		return fmt.Errorf("creating backup: %w", err)
	}

	// Write modified file.
	out, err := os.Create(path)
	if err != nil {
		return err
	}
	defer out.Close()

	w := bufio.NewWriter(out)
	for i, line := range lines {
		w.WriteString(line)
		if i < len(lines)-1 {
			w.WriteByte('\n')
		}
	}
	// Preserve trailing newline if original had one.
	w.WriteByte('\n')
	return w.Flush()
}

// replaceINIValue replaces the value portion of a "KEY = VALUE" line,
// preserving the key name and any leading whitespace/formatting.
func replaceINIValue(line, newVal string) string {
	eqIdx := strings.Index(line, "=")
	if eqIdx < 0 {
		return line
	}
	// Preserve everything up to and including "= " (with one space after =).
	prefix := line[:eqIdx+1]
	// Check if there was a space after =.
	rest := line[eqIdx+1:]
	if len(rest) > 0 && rest[0] == ' ' {
		prefix += " "
	}
	return prefix + newVal
}

// copyFile copies src to dst.
func copyFile(src, dst string) error {
	data, err := os.ReadFile(src)
	if err != nil {
		return err
	}
	return os.WriteFile(dst, data, 0o644)
}
