// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// ethercat — CLI tool for EtherCAT master diagnostics via REST API.
//
// Drop-in replacement for the IgH EtherCAT master "ethercat" command-line tool.
// Communicates via REST API instead of Unix socket / ioctl.
//
// Environment:
//
//	EC_INST       Instance name (default: "ethercat")
//	GMC_REST_URL  REST endpoint (default: "http://localhost:5080/")
package main

import (
	"fmt"
	"os"
	"sort"
	"strings"
)

// Verbosity levels matching the C++ tool.
type Verbosity int

const (
	Quiet   Verbosity = -1
	Normal  Verbosity = 0
	Verbose Verbosity = 1
)

// Global options parsed from command line.
type GlobalOpts struct {
	Masters    string // --master -m (default "-" = all)
	Positions  string // --position -p (default "-" = all)
	Aliases    string // --alias -a (default "-" = all)
	Domains    string // --domain -d (default "-" = all)
	DataType   string // --type -t
	Force      bool   // --force -f
	Emergency  bool   // --emergency -e
	Verbosity  Verbosity
	OutputFile string // --output-file -o
	Skin       string // --skin -s
}

// Command is a subcommand implementation.
type Command struct {
	Name  string
	Brief string
	Run   func(client *EthercatClient, opts *GlobalOpts, args []string) error
}

var commands []*Command

func registerCommand(c *Command) {
	commands = append(commands, c)
}

func findCommand(name string) *Command {
	// Support abbreviation (like the C++ tool)
	var matches []*Command
	for _, c := range commands {
		if c.Name == name {
			return c
		}
		if strings.HasPrefix(c.Name, name) {
			matches = append(matches, c)
		}
	}
	if len(matches) == 1 {
		return matches[0]
	}
	return nil
}

func usage(progName string) {
	sort.Slice(commands, func(i, j int) bool {
		return commands[i].Name < commands[j].Name
	})

	maxWidth := 0
	for _, c := range commands {
		if len(c.Name) > maxWidth {
			maxWidth = len(c.Name)
		}
	}

	fmt.Fprintf(os.Stderr, "Usage: %s <COMMAND> [OPTIONS] [ARGUMENTS]\n\n", progName)
	fmt.Fprintf(os.Stderr, "Commands (can be abbreviated):\n")
	for _, c := range commands {
		fmt.Fprintf(os.Stderr, "  %-*s  %s\n", maxWidth, c.Name, c.Brief)
	}
	fmt.Fprintf(os.Stderr, `
Global options:
  --master  -m <master>  Comma separated list of masters
                         to select, ranges are allowed.
                         Examples: '1,3', '5-7,9', '-3'.
                         Default: '-' (all).
  --force   -f           Force a command.
  --quiet   -q           Output less information.
  --verbose -v           Output more information.
  --help    -h           Show this help.

Environment:
  EC_INST       EtherCAT instance name (default: "ethercat")
  GMC_REST_URL  REST server URL (default: "http://localhost:5080/")

Numeric values can be specified as:
  - Decimal:     12345
  - Octal:       012345
  - Hexadecimal: 0x12345

Call '%s <COMMAND> --help' for command-specific help.
`, progName)
}

func main() {
	progName := "ethercat"
	if len(os.Args) > 0 {
		parts := strings.Split(os.Args[0], "/")
		progName = parts[len(parts)-1]
	}

	// Parse global options before and after command name.
	opts := &GlobalOpts{
		Masters:   "-",
		Positions: "-",
		Aliases:   "-",
		Domains:   "-",
		Verbosity: Normal,
	}

	args := os.Args[1:]
	var cmdName string
	var cmdArgs []string

	for i := 0; i < len(args); i++ {
		a := args[i]
		switch {
		case a == "--help" || a == "-h":
			usage(progName)
			os.Exit(0)
		case a == "--master" || a == "-m":
			i++
			if i < len(args) {
				opts.Masters = args[i]
			}
		case a == "--position" || a == "-p":
			i++
			if i < len(args) {
				opts.Positions = args[i]
			}
		case a == "--alias" || a == "-a":
			i++
			if i < len(args) {
				opts.Aliases = args[i]
			}
		case a == "--domain" || a == "-d":
			i++
			if i < len(args) {
				opts.Domains = args[i]
			}
		case a == "--type" || a == "-t":
			i++
			if i < len(args) {
				opts.DataType = args[i]
			}
		case a == "--output-file" || a == "-o":
			i++
			if i < len(args) {
				opts.OutputFile = args[i]
			}
		case a == "--skin" || a == "-s":
			i++
			if i < len(args) {
				opts.Skin = args[i]
			}
		case a == "--force" || a == "-f":
			opts.Force = true
		case a == "--emergency" || a == "-e":
			opts.Emergency = true
		case a == "--quiet" || a == "-q":
			opts.Verbosity = Quiet
		case a == "--verbose" || a == "-v":
			opts.Verbosity = Verbose
		case strings.HasPrefix(a, "-"):
			fmt.Fprintf(os.Stderr, "Error: Unknown option '%s'.\n", a)
			os.Exit(1)
		default:
			if cmdName == "" {
				cmdName = a
			} else {
				cmdArgs = append(cmdArgs, a)
			}
		}
	}

	if cmdName == "" {
		usage(progName)
		os.Exit(1)
	}

	cmd := findCommand(cmdName)
	if cmd == nil {
		fmt.Fprintf(os.Stderr, "Error: Unknown command '%s'.\n", cmdName)
		os.Exit(1)
	}

	// Create client from environment.
	restURL := os.Getenv("GMC_REST_URL")
	if restURL == "" {
		restURL = "http://localhost:5080/"
	}
	instance := os.Getenv("EC_INST")
	if instance == "" {
		instance = "ethercat"
	}

	client := NewEthercatClient(restURL, instance)

	if err := cmd.Run(client, opts, cmdArgs); err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
}
