// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// halcmd - HAL command-line tool using REST API
//
// This is the Go-based replacement for the legacy C halcmd and halrmt.
// It communicates with the LinuxCNC launcher via REST API.
//
// Environment variables:
//   GMC_REST_URL - Base URL of the REST server (default: http://127.0.0.1:5080)
//
// Usage:
//   halcmd [options] [command] [args...]
//   halcmd -f <halfile>
//   halcmd (interactive mode)

package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/halcmdclient"
)

const (
	defaultRestURL = "http://127.0.0.1:5080"
	envRestURL     = "GMC_REST_URL"
)

var (
	client     *halcmdclient.HalcmdClient
	echoMode   bool
	keepGoing  bool
	quietMode  bool
	scriptMode bool
)

func main() {
	// Get REST URL from environment or use default
	restURL := os.Getenv(envRestURL)
	if restURL == "" {
		restURL = defaultRestURL
	}

	client = halcmdclient.NewHalcmdClient(restURL)

	args := os.Args[1:]

	// Handle options
	for len(args) > 0 && strings.HasPrefix(args[0], "-") {
		switch args[0] {
		case "-h", "--help":
			printUsage()
			os.Exit(0)
		case "-f":
			if len(args) < 2 || strings.HasPrefix(args[1], "-") {
				// No filename: read from stdin
				if err := runStream(os.Stdin, "<stdin>"); err != nil {
					fatal(err.Error())
				}
			} else {
				if err := runFile(args[1]); err != nil {
					fatal(err.Error())
				}
			}
			os.Exit(0)
		case "-k", "--keep-going":
			keepGoing = true
			args = args[1:]
		case "-q", "--quiet":
			quietMode = true
			args = args[1:]
		case "-Q":
			echoMode = true
			args = args[1:]
		case "-C":
			runCompletion()
			os.Exit(0)
		case "-s":
			scriptMode = true
			args = args[1:]
		case "-v", "--version":
			fmt.Println("halcmd (Go REST) version 2.0")
			os.Exit(0)
		case "-U":
			// URL override on command line
			if len(args) < 2 {
				fatal("-U requires a URL")
			}
			client = halcmdclient.NewHalcmdClient(args[1])
			args = args[2:]
		default:
			fatal("unknown option: " + args[0])
		}
	}

	// If remaining args, execute as single command
	if len(args) > 0 {
		if err := executeCommand(args); err != nil {
			fatal(err.Error())
		}
		os.Exit(0)
	}

	// Interactive mode
	runInteractive()
}

func printUsage() {
	fmt.Printf(`halcmd - HAL command-line tool (REST-based)

Usage:
  halcmd [options] [command] [args...]
  halcmd -f <halfile>
  halcmd              (interactive mode)

Options:
  -h, --help        Show this help
  -v, --version     Show version
  -f <file>         Execute commands from file
  -k, --keep-going  Keep going after errors
  -q, --quiet       Quiet mode (less output)
  -Q                Echo commands in -f mode
  -s                Script mode (no prompt)
  -U <url>          Override REST URL

Commands:
  show <type> [pattern]   List items (pin|sig|param|comp|funct|thread|all)
  list <type> [pattern]   Simple list of names
  getp <pin|param>        Get pin or parameter value
  setp <pin|param> <val>  Set pin or parameter value
  gets <signal>           Get signal value
  sets <signal> <value>   Set signal value
  ptype <pin>             Get pin type
  stype <signal>          Get signal type
  
  newsig <name> <type>    Create signal (bit|float|s32|u32)
  delsig <name>           Delete signal
  net <sig> <pin> [pins]  Link signal to pin(s), create signal if needed
  linksp <sig> <pin>      Link signal to pin
  linkps <pin> <sig>      Link pin to signal (same as linksp)
  linkpp <pin1> <pin2>    Link pin to pin (implicit signal)
  unlinkp <pin>           Unlink pin from signal
  
  load <mod> [args]       Load cmod plugin
  
  newthread <n> <period>  Create thread (period in ns)
  delthread <name>        Delete thread
  addf <func> <thread>    Add function to thread
  delf <func> <thread>    Remove function from thread
  start                   Start all threads
  stop                    Stop all threads
  
  alias pin <name> <al>   Create pin alias
  alias param <name> <al> Create param alias
  unalias pin <name>      Remove pin alias
  unalias param <name>    Remove param alias
  
  lock [all|tune|none]    Lock HAL
  unlock [all|tune]       Unlock HAL
  debug <level>           Set debug level
  status                  Show HAL status
  save [type]             Save HAL config (all|comp|sig|link|net|param|thread)
  
  source <file>           Execute commands from file
  echo                    Enable command echo
  unecho                  Disable command echo
  help [command]          Show help
  quit/exit               Exit interactive mode

Environment:
  GMC_REST_URL   REST server URL (default: %s)
`, defaultRestURL)
}

func fatal(msg string) {
	fmt.Fprintf(os.Stderr, "halcmd: %s\n", msg)
	os.Exit(1)
}

func warn(msg string) {
	if !quietMode {
		fmt.Fprintf(os.Stderr, "halcmd: %s\n", msg)
	}
}

func runFile(filename string) error {
	f, err := os.Open(filename)
	if err != nil {
		return err
	}
	defer f.Close()
	return runStream(f, filename)
}

func runStream(r io.Reader, source string) error {
	scanner := bufio.NewScanner(r)
	lineNum := 0
	var continued strings.Builder
	continuedFrom := 0

	for scanner.Scan() {
		lineNum++
		raw := scanner.Text()
		line := strings.TrimSpace(raw)

		// Line continuation: trailing backslash joins next line
		if strings.HasSuffix(line, "\\") {
			line = strings.TrimSuffix(line, "\\")
			if continued.Len() == 0 {
				continuedFrom = lineNum
			}
			continued.WriteString(line)
			continued.WriteByte(' ')
			continue
		}

		if continued.Len() > 0 {
			continued.WriteString(line)
			line = continued.String()
			continued.Reset()
			lineNum = continuedFrom
		}

		// Skip empty lines and comments
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		if echoMode {
			fmt.Printf("%d: %s\n", lineNum, line)
		}
		args := parseCommandLine(line)
		if len(args) == 0 {
			continue
		}
		if err := executeCommand(args); err != nil {
			if keepGoing {
				warn(fmt.Sprintf("%s:%d: %s", source, lineNum, err.Error()))
			} else {
				return fmt.Errorf("%s:%d: %w", source, lineNum, err)
			}
		}
	}
	return scanner.Err()
}

func runInteractive() {
	scanner := bufio.NewScanner(os.Stdin)
	if !scriptMode {
		fmt.Println("halcmd: Type 'help' for help, 'quit' to exit")
	}

	for {
		if !scriptMode {
			fmt.Print("halcmd> ")
		}
		if !scanner.Scan() {
			break
		}
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}
		args := parseCommandLine(line)
		if len(args) == 0 {
			continue
		}
		if args[0] == "quit" || args[0] == "exit" {
			break
		}
		if err := executeCommand(args); err != nil {
			fmt.Fprintf(os.Stderr, "error: %s\n", err)
		}
	}
}

// stripArrows removes direction arrows (<=, =>, <=>) from argument lists.
// These are used in HAL files for documentation but have no semantic meaning.
func stripArrows(args []string) []string {
	result := make([]string, 0, len(args))
	for _, a := range args {
		if a == "=>" || a == "<=" || a == "<=>" {
			continue
		}
		result = append(result, a)
	}
	return result
}

// parseCommandLine splits a command line respecting quotes
func parseCommandLine(line string) []string {
	var args []string
	var current strings.Builder
	inQuote := false
	quoteChar := rune(0)

	for _, r := range line {
		switch {
		case r == '"' || r == '\'':
			if !inQuote {
				inQuote = true
				quoteChar = r
			} else if r == quoteChar {
				inQuote = false
			} else {
				current.WriteRune(r)
			}
		case r == ' ' || r == '\t':
			if inQuote {
				current.WriteRune(r)
			} else if current.Len() > 0 {
				args = append(args, current.String())
				current.Reset()
			}
		case r == '#' && !inQuote:
			// Rest of line is comment
			if current.Len() > 0 {
				args = append(args, current.String())
			}
			return args
		default:
			current.WriteRune(r)
		}
	}
	if current.Len() > 0 {
		args = append(args, current.String())
	}
	return args
}

func executeCommand(args []string) error {
	if len(args) == 0 {
		return nil
	}

	cmd := strings.ToLower(args[0])
	args = args[1:]

	switch cmd {
	// Help
	case "help":
		return cmdHelp(args)

	// Show/List
	case "show":
		return cmdShow(args)
	case "list":
		return cmdList(args)
	case "status":
		return cmdStatus(args)

	// Pin/Param access
	case "getp":
		return cmdGetP(args)
	case "setp":
		return cmdSetP(args)
	case "gets":
		return cmdGetS(args)
	case "sets":
		return cmdSetS(args)
	case "ptype":
		return cmdPType(args)
	case "stype":
		return cmdSType(args)

	// Signals
	case "newsig":
		return cmdNewSig(args)
	case "delsig":
		return cmdDelSig(args)

	// Linking
	case "net":
		return cmdNet(args)
	case "linksp":
		return cmdLinkSP(args)
	case "linkps":
		return cmdLinkPS(args)
	case "linkpp":
		return cmdLinkPP(args)
	case "unlinkp":
		return cmdUnlinkP(args)

	// Modules
	case "load":
		return cmdLoad(args)
	case "unload":
		return cmdUnload(args)

	// Threads
	case "newthread":
		return cmdNewThread(args)
	case "delthread":
		return cmdDelThread(args)
	case "addf":
		return cmdAddF(args)
	case "delf":
		return cmdDelF(args)
	case "start":
		return cmdStart(args)
	case "stop":
		return cmdStop(args)

	// Alias
	case "alias":
		return cmdAlias(args)
	case "unalias":
		return cmdUnalias(args)

	// Lock/Debug/Save
	case "lock":
		return cmdLock(args)
	case "unlock":
		return cmdUnlock(args)
	case "debug":
		return cmdDebug(args)
	case "save":
		return cmdSave(args)

	// Retain
	case "retain":
		return cmdRetain(args)
	case "unretain":
		return cmdUnretain(args)

	// Scripting
	case "source":
		return cmdSource(args)
	case "echo":
		echoMode = true
		return nil
	case "unecho":
		echoMode = false
		return nil

	default:
		return fmt.Errorf("unknown command: %s (type 'help' for help)", cmd)
	}
}

// ─── Command Implementations ───

func cmdHelp(args []string) error {
	if len(args) == 0 {
		printUsage()
		return nil
	}
	cmd := strings.ToLower(args[0])
	if text, ok := commandHelp[cmd]; ok {
		fmt.Println(text)
		return nil
	}
	return fmt.Errorf("no help for '%s'", cmd)
}

var commandHelp = map[string]string{
	"show": `show [type] [pattern]
  List HAL items of the given type. Type is one of:
    pin sig param comp funct thread alias all
  Optional pattern is a glob to filter results.`,
	"list": `list [type] [pattern]
  Print names of HAL items (one per line). Type is one of:
    pin sig param comp funct thread`,
	"status": `status
  Show HAL overall status (lock state, thread count, etc.)`,
	"getp": `getp <pin-or-param>
  Get the value of a pin or parameter.`,
	"setp": `setp <pin-or-param> <value>
  Set the value of a writable pin or parameter.`,
	"gets": `gets <signal>
  Get the value of a signal.`,
	"sets": `sets <signal> <value>
  Set the value of a signal (only if no writer pin is connected).`,
	"ptype": `ptype <pin-or-param>
  Get the type (bit/float/s32/u32) of a pin or parameter.`,
	"stype": `stype <signal>
  Get the type of a signal.`,
	"newsig": `newsig <name> <type>
  Create a new signal. Type is one of: bit float s32 u32`,
	"delsig": `delsig <name>
  Delete a signal (must have no connected pins).`,
	"net": `net <signal> [arrows] <pin> [[arrows] <pin>...]
  Connect signal to one or more pins, creating the signal if needed.
  Direction arrows (<=, =>, <=>) are allowed but ignored.
  Example: net x-pos-cmd axis.x.pos-cmd => joint.0.motor-pos-cmd`,
	"linksp": `linksp <signal> <pin>
  Link an existing signal to a pin.`,
	"linkps": `linkps <pin> <signal>
  Link a pin to an existing signal (same as linksp, reversed args).`,
	"linkpp": `linkpp <pin1> <pin2>
  Link two pins together (creates an implicit signal).`,
	"unlinkp": `unlinkp <pin>
  Unlink a pin from its signal.`,
	"load": `load <module> [args...]
  Load a cmod plugin module into gomc-server.`,
	"unload": `Unload a module by instance name.
Usage: halcmd unload <name>
Removes the module's RT functions from threads, stops and destroys it.
Returns EBUSY if another module depends on this module's APIs.`,
	"newthread": `newthread <name> <period-ns> [fp] [cpu=N]
  Create a new realtime thread.
  period-ns is the period in nanoseconds.
  fp        enable floating-point support.
  cpu=N     pin to CPU N.`,
	"delthread": `delthread <name>
  Delete a thread (must have no attached functions).`,
	"addf": `addf <function> <thread> [position]
  Add a function to a thread. Position is optional (appends by default).`,
	"delf": `delf <function> <thread>
  Remove a function from a thread.`,
	"start": `start
  Start all realtime threads.`,
	"stop": `stop
  Stop all realtime threads.`,
	"alias": `alias pin|param <name> <alias>
  Create an alias for a pin or parameter.`,
	"unalias": `unalias pin|param <alias>
  Remove an alias from a pin or parameter.`,
	"lock": `lock [none|tune|all]
  Lock HAL against certain modifications.`,
	"unlock": `unlock [tune|all]
  Unlock HAL.`,
	"debug": `debug <level>
  Set the RTAPI message level (integer).`,
	"save": `save [type]
  Output HAL configuration as halcmd commands.
  Type is one of: all comp sig link linka net neta param thread alias`,
	"retain": `retain <signal>
  Set the retain flag on a signal (value preserved across restarts).`,
	"unretain": `unretain <signal>
  Clear the retain flag on a signal.`,
	"source": `source <filename>
  Execute halcmd commands from a file.`,
	"echo": `echo
  Enable command echo (show each command before executing in file mode).`,
	"unecho": `unecho
  Disable command echo.`,
}

func cmdShow(args []string) error {
	if len(args) == 0 {
		return fmt.Errorf("show requires argument: pin|sig|param|comp|funct|thread|all")
	}

	what := strings.ToLower(args[0])
	var pattern *string
	if len(args) > 1 {
		pattern = &args[1]
	}

	switch what {
	case "pin", "pins":
		return showPins(pattern)
	case "sig", "signal", "signals":
		return showSignals(pattern)
	case "param", "params", "parameter", "parameters":
		return showParams(pattern)
	case "comp", "component", "components":
		return showComponents(pattern)
	case "funct", "function", "functions":
		return showFunctions(pattern)
	case "thread", "threads":
		return showThreads(pattern)
	case "alias", "aliases":
		return showAliases(pattern)
	case "all":
		if err := showComponents(pattern); err != nil {
			return err
		}
		if err := showPins(pattern); err != nil {
			return err
		}
		if err := showParams(pattern); err != nil {
			return err
		}
		if err := showSignals(pattern); err != nil {
			return err
		}
		if err := showFunctions(pattern); err != nil {
			return err
		}
		return showThreads(pattern)
	default:
		return fmt.Errorf("show: unknown item '%s'", what)
	}
}

func showPins(pattern *string) error {
	pins, err := client.ListPins(pattern)
	if err != nil {
		return err
	}
	if len(pins) == 0 {
		return nil
	}
	fmt.Printf("Component Pins:\n")
	fmt.Printf("%-6s %-4s %-40s  %-10s  %s\n", "Type", "Dir", "Name", "Value", "Signal")
	for _, p := range pins {
		sig := ""
		if p.Signal != nil {
			sig = *p.Signal
		}
		name := p.Name
		if p.Alias != nil {
			name = fmt.Sprintf("%s (%s)", *p.Alias, p.Name)
		}
		fmt.Printf("%-6s %-4s %-40s  %-10s  %s\n", p.Type, p.Dir, name, p.Value, sig)
	}
	fmt.Println()
	return nil
}

func showParams(pattern *string) error {
	params, err := client.ListParams(pattern)
	if err != nil {
		return err
	}
	if len(params) == 0 {
		return nil
	}
	fmt.Printf("Parameters:\n")
	fmt.Printf("%-6s %-4s %-40s  %s\n", "Type", "Dir", "Name", "Value")
	for _, p := range params {
		name := p.Name
		if p.Alias != nil {
			name = fmt.Sprintf("%s (%s)", *p.Alias, p.Name)
		}
		fmt.Printf("%-6s %-4s %-40s  %s\n", p.Type, p.Dir, name, p.Value)
	}
	fmt.Println()
	return nil
}

func showSignals(pattern *string) error {
	sigs, err := client.ListSignals(pattern)
	if err != nil {
		return err
	}
	if len(sigs) == 0 {
		return nil
	}
	fmt.Printf("Signals:\n")
	fmt.Printf("%-6s %-30s  %s\n", "Type", "Name", "Value")
	for _, s := range sigs {
		fmt.Printf("%-6s %-30s  %s\n", s.Type, s.Name, s.Value)
		if len(s.Writers) > 0 {
			for _, w := range s.Writers {
				fmt.Printf("       <== %s\n", w)
			}
		}
		if len(s.Bidirs) > 0 {
			for _, b := range s.Bidirs {
				fmt.Printf("       <=> %s\n", b)
			}
		}
		if len(s.Readers) > 0 {
			for _, r := range s.Readers {
				fmt.Printf("       ==> %s\n", r)
			}
		}
	}
	fmt.Println()
	return nil
}

func showComponents(pattern *string) error {
	comps, err := client.ListComponents(pattern)
	if err != nil {
		return err
	}
	if len(comps) == 0 {
		return nil
	}
	fmt.Printf("Components:\n")
	fmt.Printf("%-6s %-12s %-30s %s\n", "ID", "Type", "Name", "State")
	for _, c := range comps {
		pid := ""
		if c.Pid != nil {
			pid = fmt.Sprintf(" (PID %d)", *c.Pid)
		}
		fmt.Printf("%-6d %-12s %-30s %s%s\n", c.Id, c.Type, c.Name, c.State, pid)
	}
	fmt.Println()
	return nil
}

func showFunctions(pattern *string) error {
	functs, err := client.ListFunctions(pattern)
	if err != nil {
		return err
	}
	if len(functs) == 0 {
		return nil
	}
	fmt.Printf("Functions:\n")
	fmt.Printf("%-6s %-3s %-20s  %-30s  %s\n", "Users", "FP", "Owner", "Name", "Runtime")
	for _, f := range functs {
		fp := "NO"
		if f.Fp {
			fp = "YES"
		}
		fmt.Printf("%-6d %-3s %-20s  %-30s  %d ns\n", f.Users, fp, f.Owner, f.Name, f.Runtime)
	}
	fmt.Println()
	return nil
}

func showThreads(pattern *string) error {
	threads, err := client.ListThreads(pattern)
	if err != nil {
		return err
	}
	if len(threads) == 0 {
		return nil
	}
	fmt.Printf("Threads:\n")
	for _, t := range threads {
		fp := ""
		if t.Fp {
			fp = " (FP)"
		}
		fmt.Printf("%-30s  period=%d ns%s\n", t.Name, t.Period, fp)
		for i, fn := range t.Functions {
			fmt.Printf("  %3d %s\n", i+1, fn)
		}
	}
	fmt.Println()
	return nil
}

func showAliases(pattern *string) error {
	pins, err := client.ListPins(pattern)
	if err != nil {
		return err
	}
	params, err := client.ListParams(pattern)
	if err != nil {
		return err
	}

	hasAny := false
	for _, p := range pins {
		if p.Alias != nil {
			if !hasAny {
				fmt.Printf("Pin Aliases:\n")
				fmt.Printf("  %-40s  %s\n", "Alias", "Original")
				hasAny = true
			}
			fmt.Printf("  %-40s  %s\n", *p.Alias, p.Name)
		}
	}
	if hasAny {
		fmt.Println()
	}

	hasAny = false
	for _, p := range params {
		if p.Alias != nil {
			if !hasAny {
				fmt.Printf("Parameter Aliases:\n")
				fmt.Printf("  %-40s  %s\n", "Alias", "Original")
				hasAny = true
			}
			fmt.Printf("  %-40s  %s\n", *p.Alias, p.Name)
		}
	}
	if hasAny {
		fmt.Println()
	}
	return nil
}

func cmdList(args []string) error {
	if len(args) == 0 {
		return fmt.Errorf("list requires argument: pin|sig|param|comp|funct|thread")
	}

	what := strings.ToLower(args[0])
	var pattern *string
	if len(args) > 1 {
		pattern = &args[1]
	}

	switch what {
	case "pin", "pins":
		pins, err := client.ListPins(pattern)
		if err != nil {
			return err
		}
		for _, p := range pins {
			fmt.Println(p.Name)
		}
	case "sig", "signal", "signals":
		sigs, err := client.ListSignals(pattern)
		if err != nil {
			return err
		}
		for _, s := range sigs {
			fmt.Println(s.Name)
		}
	case "param", "params":
		params, err := client.ListParams(pattern)
		if err != nil {
			return err
		}
		for _, p := range params {
			fmt.Println(p.Name)
		}
	case "comp", "component", "components":
		comps, err := client.ListComponents(pattern)
		if err != nil {
			return err
		}
		for _, c := range comps {
			fmt.Println(c.Name)
		}
	case "funct", "function", "functions":
		functs, err := client.ListFunctions(pattern)
		if err != nil {
			return err
		}
		for _, f := range functs {
			fmt.Println(f.Name)
		}
	case "thread", "threads":
		threads, err := client.ListThreads(pattern)
		if err != nil {
			return err
		}
		for _, t := range threads {
			fmt.Println(t.Name)
		}
	default:
		return fmt.Errorf("list: unknown item '%s'", what)
	}
	return nil
}

func cmdStatus(args []string) error {
	st, err := client.GetStatus()
	if err != nil {
		return err
	}
	fmt.Printf("HAL Status:\n")
	fmt.Printf("  RT Lock:         %v\n", st.RtLock)
	fmt.Printf("  Mem Lock:        %v\n", st.MemLock)
	fmt.Printf("  Threads Running: %v\n", st.ThreadsRunning)
	fmt.Printf("  Components:      %d\n", st.Components)
	fmt.Printf("  Pins:            %d\n", st.Pins)
	fmt.Printf("  Signals:         %d\n", st.Signals)
	fmt.Printf("  Parameters:      %d\n", st.Params)
	fmt.Printf("  Threads:         %d\n", st.Threads)
	fmt.Printf("  Functions:       %d\n", st.Functions)
	return nil
}

func cmdGetP(args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("getp requires pin/param name")
	}
	name := args[0]

	// Try pin first
	pin, err := client.GetPin(name)
	if err == nil {
		fmt.Printf("%s %s %s = %s\n", pin.Type, pin.Dir, pin.Name, pin.Value)
		return nil
	}

	// Try param
	param, err := client.GetParam(name)
	if err == nil {
		fmt.Printf("%s %s %s = %s\n", param.Type, param.Dir, param.Name, param.Value)
		return nil
	}

	return fmt.Errorf("pin or param '%s' not found", name)
}

func cmdSetP(args []string) error {
	if len(args) < 2 {
		return fmt.Errorf("setp requires: <pin|param> <value>")
	}
	name := args[0]
	value := args[1]

	// Try pin first
	result, err := client.SetPin(name, value)
	if err == nil && result.Success {
		return nil
	}

	// Try param
	result, err = client.SetParam(name, value)
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdGetS(args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("gets requires signal name")
	}
	sig, err := client.GetSignal(args[0])
	if err != nil {
		return err
	}
	fmt.Printf("%s %s = %s\n", sig.Type, sig.Name, sig.Value)
	return nil
}

func cmdSetS(args []string) error {
	if len(args) < 2 {
		return fmt.Errorf("sets requires: <signal> <value>")
	}
	result, err := client.SetSignal(args[0], args[1])
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdPType(args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("ptype requires pin name")
	}
	pin, err := client.GetPin(args[0])
	if err != nil {
		return err
	}
	fmt.Println(pin.Type)
	return nil
}

func cmdSType(args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("stype requires signal name")
	}
	sig, err := client.GetSignal(args[0])
	if err != nil {
		return err
	}
	fmt.Println(sig.Type)
	return nil
}

func cmdNewSig(args []string) error {
	if len(args) < 2 {
		return fmt.Errorf("newsig requires: <name> <type>")
	}
	result, err := client.NewSignal(args[0], args[1])
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdDelSig(args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("delsig requires signal name")
	}
	result, err := client.DeleteSignal(args[0])
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdNet(args []string) error {
	args = stripArrows(args)
	if len(args) < 2 {
		return fmt.Errorf("net requires: <signal> <pin> [pin...]")
	}
	signal := args[0]
	pins := args[1:]

	result, err := client.Net(signal, pins)
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdLinkSP(args []string) error {
	args = stripArrows(args)
	if len(args) < 2 {
		return fmt.Errorf("linksp requires: <signal> <pin>")
	}
	result, err := client.Link(args[1], args[0])
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdLinkPS(args []string) error {
	args = stripArrows(args)
	if len(args) < 2 {
		return fmt.Errorf("linkps requires: <pin> <signal>")
	}
	result, err := client.Link(args[0], args[1])
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdLinkPP(args []string) error {
	args = stripArrows(args)
	if len(args) < 2 {
		return fmt.Errorf("linkpp requires: <pin1> <pin2>")
	}
	result, err := client.LinkPp(args[0], args[1])
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdUnlinkP(args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("unlinkp requires pin name")
	}
	result, err := client.Unlink(args[0])
	if err != nil {
		return err
	}
	return checkResult(result)
}

// resolveArgPath makes a relative file path absolute so the server (which may
// have a different cwd) can find it.  Non-path args (key=value) are unchanged.
func resolveArgPath(arg string) string {
	if strings.Contains(arg, "=") {
		return arg
	}
	if strings.Contains(arg, "/") || strings.Contains(arg, ".") {
		if !filepath.IsAbs(arg) {
			if abs, err := filepath.Abs(arg); err == nil {
				return abs
			}
		}
	}
	return arg
}

func cmdLoad(args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("load requires module name")
	}
	module := args[0]
	var modArgs []*string
	for _, a := range args[1:] {
		s := resolveArgPath(a)
		modArgs = append(modArgs, &s)
	}
	result, err := client.Load(module, modArgs)
	if err != nil {
		return err
	}
	if err := checkResult(result); err != nil {
		return err
	}
	if result.Output != nil && *result.Output != "" && !quietMode {
		fmt.Println(*result.Output)
	}
	return nil
}

func cmdUnload(args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("unload requires module instance name")
	}
	result, err := client.Unload(args[0])
	if err != nil {
		return err
	}
	if err := checkResult(result); err != nil {
		return err
	}
	if result.Output != nil && *result.Output != "" && !quietMode {
		fmt.Println(*result.Output)
	}
	return nil
}

func cmdNewThread(args []string) error {
	if len(args) < 2 {
		return fmt.Errorf("newthread requires: <name> <period_ns> [fp] [cpu]")
	}
	name := args[0]
	period, err := strconv.ParseInt(args[1], 10, 64)
	if err != nil {
		return fmt.Errorf("invalid period: %w", err)
	}

	var fp *bool
	var cpuId *int32
	for _, arg := range args[2:] {
		lower := strings.ToLower(arg)
		if lower == "fp" {
			t := true
			fp = &t
		} else if lower == "nofp" {
			f := false
			fp = &f
		} else if strings.HasPrefix(lower, "cpu=") {
			cpu, err := strconv.ParseInt(arg[4:], 10, 32)
			if err == nil {
				c := int32(cpu)
				cpuId = &c
			}
		}
	}

	result, err := client.Newthread(name, period, fp, cpuId)
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdDelThread(args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("delthread requires thread name")
	}
	result, err := client.Delthread(args[0])
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdAddF(args []string) error {
	if len(args) < 2 {
		return fmt.Errorf("addf requires: <function> <thread> [position]")
	}
	function := args[0]
	thread := args[1]
	var position *int32
	if len(args) > 2 {
		p, err := strconv.ParseInt(args[2], 10, 32)
		if err != nil {
			return fmt.Errorf("invalid position: %w", err)
		}
		pos := int32(p)
		position = &pos
	}
	result, err := client.Addf(thread, function, position)
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdDelF(args []string) error {
	if len(args) < 2 {
		return fmt.Errorf("delf requires: <function> <thread>")
	}
	result, err := client.Delf(args[1], args[0])
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdStart(args []string) error {
	result, err := client.Start()
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdStop(args []string) error {
	result, err := client.Stop()
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdAlias(args []string) error {
	if len(args) < 3 {
		return fmt.Errorf("alias requires: pin|param <name> <alias>")
	}
	what := strings.ToLower(args[0])
	name := args[1]
	alias := args[2]

	var result *halcmdclient.CmdResult
	var err error
	switch what {
	case "pin":
		result, err = client.AliasPin(name, alias)
	case "param", "parameter":
		result, err = client.AliasParam(name, alias)
	default:
		return fmt.Errorf("alias: unknown type '%s' (use pin or param)", what)
	}
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdUnalias(args []string) error {
	if len(args) < 2 {
		return fmt.Errorf("unalias requires: pin|param <name>")
	}
	what := strings.ToLower(args[0])
	name := args[1]

	var result *halcmdclient.CmdResult
	var err error
	switch what {
	case "pin":
		result, err = client.UnaliasPin(name)
	case "param", "parameter":
		result, err = client.UnaliasParam(name)
	default:
		return fmt.Errorf("unalias: unknown type '%s' (use pin or param)", what)
	}
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdLock(args []string) error {
	var level *string
	if len(args) > 0 {
		level = &args[0]
	}
	result, err := client.Lock(level)
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdUnlock(args []string) error {
	var level *string
	if len(args) > 0 {
		level = &args[0]
	}
	result, err := client.Unlock(level)
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdDebug(args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("debug requires level")
	}
	level, err := strconv.ParseInt(args[0], 10, 32)
	if err != nil {
		return fmt.Errorf("invalid debug level: %w", err)
	}
	result, err := client.SetDebug(int32(level))
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdSave(args []string) error {
	var saveType *string
	if len(args) > 0 {
		saveType = &args[0]
	}
	result, err := client.Save(saveType)
	if err != nil {
		return err
	}
	if result.Output != nil && *result.Output != "" {
		fmt.Print(*result.Output)
	}
	return nil
}

func cmdSource(args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("source requires filename")
	}
	return runFile(args[0])
}

func cmdRetain(args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("retain requires signal name")
	}
	result, err := client.Retain(args[0])
	if err != nil {
		return err
	}
	return checkResult(result)
}

func cmdUnretain(args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("unretain requires signal name")
	}
	result, err := client.Unretain(args[0])
	if err != nil {
		return err
	}
	return checkResult(result)
}

// checkResult returns an error if the result indicates failure
func checkResult(result *halcmdclient.CmdResult) error {
	if result == nil {
		return fmt.Errorf("no response from server")
	}
	if !result.Success {
		if result.Error != nil {
			return fmt.Errorf("%s", *result.Error)
		}
		return fmt.Errorf("command failed")
	}
	return nil
}
