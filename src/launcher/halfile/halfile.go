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

// IniLookup returns a hal.INILookup adapter for the INI file associated with
// this Executor. Returns nil when no INI file is set.
// This is the public accessor used by callers (e.g. cleanup.go) that need to
// pass an INILookup to hal.NewSingleFileParser directly.
func (e *Executor) IniLookup() hal.INILookup {
	return e.iniLookup()
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

// ExecuteShutdown reads the [HAL]SHUTDOWN entry from the INI file and executes
// it via the native Go HAL file parser. This is the Go-native replacement for
// the "halcmd -f $SHUTDOWN" shell-out in the bash launcher.
//
// This mirrors scripts/linuxcnc.in lines 697–701:
//
//	SHUTDOWN=`$INIVAR -ini "$INIFILE" -var SHUTDOWN -sec HAL 2> /dev/null`
//	if [ -n "$SHUTDOWN" ]; then
//	    $HALCMD -f $SHUTDOWN
//	fi
func (e *Executor) ExecuteShutdown() error {
	if e.ini == nil {
		return nil
	}
	shutdown := e.ini.Get("HAL", "SHUTDOWN")
	if shutdown == "" {
		return nil
	}
	resolved, err := e.resolvePath(shutdown)
	if err != nil {
		return fmt.Errorf("resolving HAL shutdown script %q: %w", shutdown, err)
	}
	e.logger.Info("running HAL shutdown script", "script", resolved)
	sp := hal.NewSingleFileParser(e.iniLookup(), e)
	result, err := sp.Parse(resolved)
	if err != nil {
		return fmt.Errorf("parsing HAL shutdown script %q: %w", resolved, err)
	}
	return result.Execute()
}

// ExecutePostGUI reads [HAL]POSTGUI_HALFILE entries from the INI file and
// executes them via the native Go HAL file parser.
//
// POSTGUI_HALFILE files are typically executed after the GUI has created its
// HAL pins. GUIs that manage this themselves (AXIS, QtVCP, gmoccapy, etc.)
// do not need to call this method. It is provided for configurations that
// delegate post-GUI HAL setup to the launcher.
//
// This method does NOT run as part of the normal ExecuteAll() startup path.
// Call it explicitly after the display is ready.
func (e *Executor) ExecutePostGUI() error {
	if e.ini == nil {
		return nil
	}

	var paths []string
	for _, entry := range e.ini.GetSection("HAL") {
		if entry.Key != "POSTGUI_HALFILE" {
			continue
		}
		fields := strings.Fields(entry.Value)
		if len(fields) == 0 {
			continue
		}
		resolved, err := e.resolvePath(fields[0])
		if err != nil {
			return fmt.Errorf("resolving POSTGUI_HALFILE %q: %w", fields[0], err)
		}
		if strings.HasSuffix(strings.ToLower(resolved), ".tcl") {
			return fmt.Errorf("POSTGUI_HALFILE %q: TCL HAL files (.tcl) are not supported", resolved)
		}
		e.logger.Info("loading POSTGUI_HALFILE", "path", resolved)
		paths = append(paths, resolved)
	}

	if len(paths) == 0 {
		return nil
	}

	mp := hal.NewMultiFileParser(e.iniLookup(), e)
	result, err := mp.Parse(paths)
	if err != nil {
		return fmt.Errorf("parsing POSTGUI_HALFILE: %w", err)
	}
	return result.Execute()
}
