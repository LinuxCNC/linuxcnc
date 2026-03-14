// Package halfile implements loading and execution of LinuxCNC HAL files.
//
// HAL files contain commands for the Hardware Abstraction Layer (HAL).  During
// startup, the launcher loads each file listed in [HAL]HALFILE from the INI
// configuration, performs INI variable substitution on each line, and then
// executes the result via the halcmd binary.
package halfile

import (
	"bufio"
	"fmt"
	"log/slog"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/sittner/linuxcnc/src/launcher/inifile"
)

// Executor loads and executes HAL files for a LinuxCNC configuration.
type Executor struct {
	ini         *inifile.IniFile
	iniFilePath string // effective INI path for -i args (may be .expanded)
	halcmdPath  string
	halibPath   string
	logger      *slog.Logger
	configDir   string
}

// New creates a new Executor for HAL file loading.
//
//   - ini is the parsed INI configuration file.
//   - halcmdPath is the absolute path to the halcmd binary.
//   - halibPath is the colon-separated HALLIB_PATH search list.
//   - logger is used for diagnostic output; if nil a default stderr logger is used.
//   - iniFilePath overrides the INI path passed to subprocesses via -i.
//     Pass the expanded INI path here when #INCLUDE directives have been
//     resolved; pass "" to fall back to ini.SourceFile().
func New(ini *inifile.IniFile, halcmdPath string, halibPath string, logger *slog.Logger, iniFilePath string) *Executor {
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
		halcmdPath:  halcmdPath,
		halibPath:   halibPath,
		logger:      logger,
		configDir:   configDir,
	}
}

// effectiveIniPath returns the INI file path to pass to subprocesses via -i.
// This may be the expanded (.expanded) path when #INCLUDE directives were resolved.
func (e *Executor) effectiveIniPath() string {
	return e.iniFilePath
}

// ExecuteAll reads all [HAL]HALFILE entries from the INI file and executes
// them in order.  Only HALFILE entries are processed; HALCMD entries are
// skipped here and must be executed separately via ExecuteHalCommands().
//
// This matches the bash launcher (scripts/linuxcnc.in step 4.3.6) which
// iterates only HALFILE keys via "$INIVAR -var HALFILE" in a separate loop
// from HALCMD keys.
//
// When [HAL]TWOPASS is set to a non-empty value, execution is delegated
// entirely to the legacy twopass.tcl script via haltcl.
func (e *Executor) ExecuteAll() error {
	if e.ini == nil {
		return nil
	}

	// When TWOPASS is set, delegate entirely to the legacy twopass.tcl.
	if tp := e.ini.Get("HAL", "TWOPASS"); tp != "" {
		return e.executeTwopass()
	}

	// Iterate [HAL] section entries, processing only HALFILE keys.
	// This mirrors the bash launcher's separate "$INIVAR -var HALFILE" loop.
	for _, entry := range e.ini.GetSection("HAL") {
		if entry.Key != "HALFILE" {
			continue
		}
		// Split into filename and optional arguments (e.g. "LIB:basic_sim.tcl -no_sim_spindle").
		fields := strings.Fields(entry.Value)
		if len(fields) == 0 {
			continue
		}
		f := fields[0]
		args := fields[1:]
		resolved, err := e.resolvePath(f)
		if err != nil {
			return fmt.Errorf("resolving HAL file %q: %w", f, err)
		}
		e.logger.Info("loading HAL file", "path", resolved)
		if strings.HasSuffix(resolved, ".tcl") {
			if err := e.runHaltcl(resolved, args); err != nil {
				return fmt.Errorf("executing HAL file %q: %w", resolved, err)
			}
		} else {
			if err := e.ExecuteFile(resolved); err != nil {
				return fmt.Errorf("executing HAL file %q: %w", resolved, err)
			}
		}
	}

	return nil
}

// ExecuteHalCommands reads all [HAL]HALCMD entries from the INI file and
// executes them in order as discrete halcmd commands.
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

	for _, raw := range cmds {
		cmd := strings.TrimSpace(raw)
		if cmd == "" {
			continue
		}
		e.logger.Debug("executing HAL command", "cmd", cmd)
		if err := e.runHalcmd(cmd); err != nil {
			return fmt.Errorf("executing HALCMD %q: %w", cmd, err)
		}
	}

	return nil
}

// ExecuteFile reads a single HAL file, performs INI variable substitution on
// every line, and then executes the result via halcmd.
//
// When no substitution is needed the original file is executed directly with
// "halcmd -i <inifile> -f <file>".  When substitution is needed the
// substituted content is written to a temporary file first.
func (e *Executor) ExecuteFile(path string) error {
	f, err := os.Open(path)
	if err != nil {
		return fmt.Errorf("opening HAL file %q: %w", path, err)
	}
	defer f.Close()

	var lines []string
	needsSubstitution := false
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		orig := scanner.Text()
		subst := e.substituteLine(orig)
		if subst != orig {
			needsSubstitution = true
		}
		lines = append(lines, subst)
	}
	if err := scanner.Err(); err != nil {
		return fmt.Errorf("reading HAL file %q: %w", path, err)
	}

	if !needsSubstitution {
		// Fast path: execute the file directly without creating a temp copy.
		e.logger.Debug("executing HAL file", "path", path)
		return e.runHalcmdFile(path)
	}

	// Slow path: write substituted content to a temp file, then execute it.
	tmp, err := os.CreateTemp("", "halfile-*.hal")
	if err != nil {
		return fmt.Errorf("creating temp HAL file: %w", err)
	}
	tmpName := tmp.Name()
	defer os.Remove(tmpName)

	w := bufio.NewWriter(tmp)
	for _, l := range lines {
		fmt.Fprintln(w, l)
	}
	if err := w.Flush(); err != nil {
		tmp.Close()
		return fmt.Errorf("writing temp HAL file: %w", err)
	}
	if err := tmp.Close(); err != nil {
		return fmt.Errorf("closing temp HAL file: %w", err)
	}

	e.logger.Debug("executing substituted HAL file", "original", path, "temp", tmpName)
	return e.runHalcmdFile(tmpName)
}

// runHalcmdFile executes a HAL file via "halcmd [-i <inifile>] -f <file>".
func (e *Executor) runHalcmdFile(path string) error {
	var args []string
	if p := e.effectiveIniPath(); p != "" {
		args = append(args, "-i", p)
	}
	args = append(args, "-f", path)
	return e.RunHalcmdArgs(args)
}

// runHalcmd executes a single halcmd command string.
// Note: command splitting uses strings.Fields which does not handle quoted
// arguments containing spaces; this matches the behaviour of the legacy bash
// launcher which also does not quote-process HALCMD values.
func (e *Executor) runHalcmd(cmd string) error {
	parts := strings.Fields(cmd)
	if len(parts) == 0 {
		return nil
	}
	var args []string
	if p := e.effectiveIniPath(); p != "" {
		args = append(args, "-i", p)
	}
	args = append(args, parts...)
	return e.RunHalcmdArgs(args)
}

// RunHalcmdArgs runs halcmd with the given arguments, wiring stdout/stderr to
// the process's own stdout/stderr so that halcmd output is visible.
func (e *Executor) RunHalcmdArgs(args []string) error {
	e.logger.Debug("running halcmd", "args", strings.Join(args, " "))
	cmd := exec.Command(e.halcmdPath, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("halcmd %s: %w", strings.Join(args, " "), err)
	}
	return nil
}

// runHaltcl executes a TCL HAL file via "haltcl [-i <inifile>] <file> [args...]".
// In a RIP (run-in-place) build, haltcl lives in scripts/ which is prepended to
// PATH by setupEnvironment, so exec.LookPath will find it. For installed systems,
// haltcl lives in the same directory as halcmd (the fallback).
// HALLIB_DIR and INI_FILE_NAME are set in the subprocess environment so that
// TCL scripts can access them via $::env(HALLIB_DIR) and $::env(INI_FILE_NAME).
func (e *Executor) runHaltcl(path string, args []string) error {
	haltclPath, err := exec.LookPath("haltcl")
	if err != nil {
		haltclPath = filepath.Join(filepath.Dir(e.halcmdPath), "haltcl")
	}

	var cmdArgs []string
	iniFile := ""
	if p := e.effectiveIniPath(); p != "" {
		iniFile = p
		cmdArgs = append(cmdArgs, "-i", iniFile)
	}
	cmdArgs = append(cmdArgs, path)
	cmdArgs = append(cmdArgs, args...)

	e.logger.Debug("running haltcl", "args", strings.Join(cmdArgs, " "))
	cmd := exec.Command(haltclPath, cmdArgs...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	env := append(os.Environ(), "HALLIB_DIR="+e.halibDir())
	if iniFile != "" {
		env = append(env, "INI_FILE_NAME="+iniFile)
	}
	cmd.Env = env
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("haltcl %s: %w", strings.Join(cmdArgs, " "), err)
	}
	return nil
}

// halibDir returns the system HAL library directory extracted from halibPath.
// By convention, halibPath is built as ".:HALLIB_DIR" so the last non-empty
// entry is HALLIB_DIR.
func (e *Executor) halibDir() string {
	dirs := strings.Split(e.halibPath, ":")
	for i := len(dirs) - 1; i >= 0; i-- {
		d := strings.TrimSpace(dirs[i])
		if d != "" {
			return d
		}
	}
	return ""
}
