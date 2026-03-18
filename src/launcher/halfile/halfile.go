// Package halfile implements loading and execution of LinuxCNC HAL files.
//
// HAL files contain commands for the Hardware Abstraction Layer (HAL).  During
// startup, the launcher loads each file listed in [HAL]HALFILE from the INI
// configuration and executes them via the hal-go package's native Go parser
// and executor.
package halfile

import (
	"fmt"
	"log/slog"
	"os"
	"path/filepath"
	"strings"

	hal "linuxcnc.org/hal"

	"github.com/sittner/linuxcnc/src/launcher/inifile"
)

// iniLookupAdapter wraps *inifile.IniFile so that it satisfies the
// hal.INILookup interface required by the hal-go parser.
type iniLookupAdapter struct {
	ini *inifile.IniFile
}

// Get implements hal.INILookup. It returns ("", nil) when the key is absent
// (the hal parser treats an empty string the same as "not found" for
// substitution purposes).
func (a *iniLookupAdapter) Get(section, key string) (string, error) {
	return a.ini.Get(section, key), nil
}

// GetAll implements hal.INILookup. It returns the full INI content as a
// nested section→key→value map, needed for Go-template rendering.
func (a *iniLookupAdapter) GetAll() map[string]map[string]string {
	m := make(map[string]map[string]string)
	for _, sec := range a.ini.Sections {
		if _, ok := m[sec.Name]; !ok {
			m[sec.Name] = make(map[string]string)
		}
		for _, entry := range sec.Entries {
			m[sec.Name][entry.Key] = entry.Value
		}
	}
	return m
}

// Executor loads and executes HAL files for a LinuxCNC configuration.
type Executor struct {
	ini         *inifile.IniFile
	iniFilePath string // effective INI path for error messages (may be .expanded)
	halibPath   string
	logger      *slog.Logger
	configDir   string
}

// New creates a new Executor for HAL file loading.
//
//   - ini is the parsed INI configuration file.
//   - halibPath is the colon-separated HALLIB_PATH search list.
//   - logger is used for diagnostic output; if nil a default stderr logger is used.
//   - iniFilePath overrides the INI path used in error messages and source locations.
//     Pass the expanded INI path here when #INCLUDE directives have been
//     resolved; pass "" to fall back to ini.SourceFile().
func New(ini *inifile.IniFile, halibPath string, logger *slog.Logger, iniFilePath string) *Executor {
	if logger == nil {
		logger = slog.New(slog.NewTextHandler(os.Stderr, nil))
	}
	configDir := ""
	if ini != nil && ini.SourceFile() != "" {
		configDir = filepath.Dir(ini.SourceFile())
	}
	if iniFilePath == "" && ini != nil {
		iniFilePath = ini.SourceFile()
	}
	return &Executor{
		ini:         ini,
		iniFilePath: iniFilePath,
		halibPath:   halibPath,
		logger:      logger,
		configDir:   configDir,
	}
}

// Resolve implements hal.PathResolver so that Executor can be passed directly
// to hal.NewMultiFileParser and hal.NewSingleFileParser.
func (e *Executor) Resolve(path string) (string, error) {
	return e.resolvePath(path)
}

// iniLookup returns a hal.INILookup adapter, or nil when no INI file is set.
func (e *Executor) iniLookup() hal.INILookup {
	if e.ini == nil {
		return nil
	}
	return &iniLookupAdapter{ini: e.ini}
}

// ExecuteAll reads all [HAL]HALFILE entries from the INI file and executes
// them in order using the hal-go native Go parser and executor.
//
// [HAL]TWOPASS support: the Go MultiFileParser collects all loadrt tokens
// across every file into a merged set before executing them, which is the
// Go-native equivalent of the legacy TCL twopass mechanism.
//
// TCL (.tcl) HAL files are not supported and cause a hard error.
//
// This matches the bash launcher (scripts/linuxcnc.in step 4.3.6) which
// iterates only HALFILE keys via "$INIVAR -var HALFILE" in a separate loop
// from HALCMD keys.
func (e *Executor) ExecuteAll() error {
	if e.ini == nil {
		return nil
	}

	var paths []string
	for _, entry := range e.ini.GetSection("HAL") {
		if entry.Key != "HALFILE" {
			continue
		}
		// Split into filename and optional arguments (e.g. "LIB:basic_sim.hal -no_sim_spindle").
		// Arguments after the filename are not used by the Go parser (they were
		// a haltcl convention); we read only the filename here.
		fields := strings.Fields(entry.Value)
		if len(fields) == 0 {
			continue
		}
		f := fields[0]
		resolved, err := e.resolvePath(f)
		if err != nil {
			return fmt.Errorf("resolving HAL file %q: %w", f, err)
		}
		if strings.HasSuffix(strings.ToLower(resolved), ".tcl") {
			return fmt.Errorf("HAL file %q: TCL HAL files (.tcl) are no longer supported; use .hal files only", resolved)
		}
		e.logger.Info("loading HAL file", "path", resolved)
		paths = append(paths, resolved)
	}

	if len(paths) == 0 {
		return nil
	}

	mp := hal.NewMultiFileParser(e.iniLookup(), e)
	result, err := mp.Parse(paths)
	if err != nil {
		return fmt.Errorf("parsing HAL files: %w", err)
	}
	return result.Execute()
}

// ExecuteHalCommands reads all [HAL]HALCMD entries from the INI file and
// executes them in order.
//
// Each entry is a single HAL command string (e.g. "setp foo.bar 1").  INI
// variable substitution is applied by the hal-go parser.
//
// This mirrors the bash launcher (scripts/linuxcnc.in step 4.3.8) which
// iterates HALCMD keys via "$INIVAR -var HALCMD" after the task controller
// has been started.  It must be called after startTask() and before
// startHalThreads().
func (e *Executor) ExecuteHalCommands() error {
	if e.ini == nil {
		return nil
	}

	cmds := e.ini.GetAll("HAL", "HALCMD")
	if len(cmds) == 0 {
		e.logger.Debug("no HALCMD entries found")
		return nil
	}

	sp := hal.NewSingleFileParser(e.iniLookup(), e)

	for i, raw := range cmds {
		cmd := strings.TrimSpace(raw)
		if cmd == "" {
			continue
		}
		e.logger.Debug("executing HAL command", "cmd", cmd)
		// ParseContent treats the string as a virtual one-line HAL file.
		result, err := sp.ParseContent(fmt.Sprintf("<HALCMD:%d>", i+1), cmd)
		if err != nil {
			return fmt.Errorf("parsing HALCMD %q: %w", cmd, err)
		}
		if err := result.Execute(); err != nil {
			return fmt.Errorf("executing HALCMD %q: %w", cmd, err)
		}
	}

	return nil
}
