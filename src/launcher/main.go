// Command linuxcnc is the Go-based launcher for LinuxCNC.
//
// It is designed as a drop-in replacement for the legacy scripts/linuxcnc.in
// bash script and accepts the same command-line flags.
//
// Usage:
//
//	linuxcnc [Options] [path/to/ini_file]
//
// Options:
//
//	-d          Turn on "debug" mode
//	-v          Turn on "verbose" mode
//	-r          Disable redirection of stdout/stderr (for tests)
//	-l          Use the last-used INI file
//	-k          Continue in the presence of errors in HAL files
//	-t "name"   Custom trajectory planning module (overrides [TRAJ]TPMOD)
//	-m "name"   Custom homing module (overrides [EMCMOT]HOMEMOD)
//	-H "dir"    Prepend dir to HALLIB_PATH (may be specified multiple times)
//	-h          Show help
package main

import (
	"flag"
	"fmt"
	"log/slog"
	"os"
	"path/filepath"

	"github.com/sittner/linuxcnc/src/launcher/launcher"
)

// multiFlag is a flag.Value that accumulates repeated string flags (e.g. -H).
type multiFlag []string

func (m *multiFlag) String() string {
	if m == nil {
		return ""
	}
	result := ""
	for i, v := range *m {
		if i > 0 {
			result += ":"
		}
		result += v
	}
	return result
}

func (m *multiFlag) Set(value string) error {
	*m = append(*m, value)
	return nil
}

func main() {
	os.Exit(run(os.Args[1:]))
}

func run(args []string) int {
	fs := flag.NewFlagSet("linuxcnc", flag.ContinueOnError)
	fs.Usage = func() {
		fmt.Fprintf(os.Stderr, `linuxcnc: Run LinuxCNC

Usage:
  linuxcnc [Options] [path/to/ini_file]

  path/to/ini_file  Path to the INI configuration file.
                    Pass '-' to use the last-used INI file (same as -l).

Options:
`)
		fs.PrintDefaults()
	}

	var (
		debug          = fs.Bool("d", false, `Turn on "debug" mode`)
		verbose        = fs.Bool("v", false, `Turn on "verbose" mode`)
		noRedirect     = fs.Bool("r", false, "Disable redirection of stdout/stderr to log files (use for tests)")
		useLast        = fs.Bool("l", false, "Use the last-used INI file")
		continueOnErr  = fs.Bool("k", false, "Continue in the presence of errors in HAL files")
		tpMod          = fs.String("t", "", `Custom trajectory planning module name (overrides [TRAJ]TPMOD)`)
		homeMod        = fs.String("m", "", `Custom homing module name (overrides [EMCMOT]HOMEMOD)`)
		halLibDirs     multiFlag
	)
	fs.Var(&halLibDirs, "H", "Prepend `dir` to HALLIB_PATH (may be specified multiple times)")

	if err := fs.Parse(args); err != nil {
		if err == flag.ErrHelp {
			return 0
		}
		return 2
	}

	// Validate -H directories.
	for _, d := range halLibDirs {
		if info, err := os.Stat(d); err != nil || !info.IsDir() {
			fmt.Fprintf(os.Stderr, "linuxcnc: invalid directory specified with -H: %s\n", d)
			return 1
		}
	}

	// Resolve the INI file path.
	var iniFile string
	if fs.NArg() > 0 {
		arg := fs.Arg(0)
		if arg == "-" {
			*useLast = true
		} else {
			abs, err := filepath.Abs(arg)
			if err != nil {
				fmt.Fprintf(os.Stderr, "linuxcnc: resolving INI file path: %v\n", err)
				return 1
			}
			iniFile = abs
		}
	}

	// TODO (M7): if useLast && iniFile == "", look up the last-used INI file
	// from ~/.linuxcncrc or similar.
	if *useLast && iniFile == "" {
		fmt.Fprintln(os.Stderr, "linuxcnc: -l / last-used INI file not yet implemented")
		return 1
	}

	// TODO (M7): if no INI file specified, launch pickconfig.tcl GUI.
	if iniFile == "" {
		fmt.Fprintln(os.Stderr, "linuxcnc: no INI file specified (GUI picker not yet implemented)")
		return 1
	}

	// Configure logger.
	logLevel := slog.LevelInfo
	if *debug || *verbose {
		logLevel = slog.LevelDebug
	}
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: logLevel}))

	opts := launcher.Options{
		Debug:           *debug,
		Verbose:         *verbose,
		NoRedirect:      *noRedirect,
		UseLast:         *useLast,
		ContinueOnError: *continueOnErr,
		TpMod:           *tpMod,
		HomeMod:         *homeMod,
		HalLibDirs:      []string(halLibDirs),
		IniFile:         iniFile,
	}

	l := launcher.New(opts, logger)
	if err := l.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "linuxcnc: %v\n", err)
		return 1
	}
	return 0
}
