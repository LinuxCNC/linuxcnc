// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package main

import (
	"fmt"
	"strconv"
	"strings"
)

func init() {
	registerCommand(&Command{
		Name:  "version",
		Brief: "Show version information.",
		Run:   cmdVersion,
	})
	registerCommand(&Command{
		Name:  "debug",
		Brief: "Set the master's debug level.",
		Run:   cmdDebug,
	})
	registerCommand(&Command{
		Name:  "rescan",
		Brief: "Rescan the bus.",
		Run:   cmdRescan,
	})
	registerCommand(&Command{
		Name:  "states",
		Brief: "Request application-layer states.",
		Run:   cmdStates,
	})
}

func cmdVersion(client *EthercatClient, opts *GlobalOpts, args []string) error {
	mod, err := client.GetModule()
	if err != nil {
		return err
	}
	magic := mod.IoctlVersionMagic
	major := (magic >> 16) & 0xFF
	minor := (magic >> 8) & 0xFF
	patch := magic & 0xFF
	fmt.Printf("IgH EtherCAT master %d.%d.%d\n", major, minor, patch)
	return nil
}

func cmdDebug(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("debug level required")
	}
	level, err := strconv.ParseUint(args[0], 0, 32)
	if err != nil {
		return fmt.Errorf("invalid debug level '%s': %v", args[0], err)
	}
	masterIndex := parseMasterIndex(opts.Masters)
	_, err = client.SetDebug(masterIndex, uint32(level))
	return err
}

func cmdRescan(client *EthercatClient, opts *GlobalOpts, args []string) error {
	masterIndex := parseMasterIndex(opts.Masters)
	_, err := client.Rescan(masterIndex)
	return err
}

func cmdStates(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) != 1 {
		return fmt.Errorf("'states' takes exactly one argument!")
	}
	stateStr := strings.ToUpper(args[0])
	var stateVal uint8
	switch stateStr {
	case "INIT":
		stateVal = 0x01
	case "PREOP":
		stateVal = 0x02
	case "BOOT":
		stateVal = 0x03
	case "SAFEOP":
		stateVal = 0x04
	case "OP":
		stateVal = 0x08
	default:
		return fmt.Errorf("invalid state '%s'", args[0])
	}

	masterIndex := parseMasterIndex(opts.Masters)
	positions := parsePositionList(opts.Positions)

	if positions == nil {
		_, err := client.SetSlaveState(masterIndex, 0xFFFF, SlaveStateRequest{AlState: stateVal})
		return err
	}
	for _, pos := range positions {
		_, err := client.SetSlaveState(masterIndex, pos, SlaveStateRequest{AlState: stateVal})
		if err != nil {
			return err
		}
	}
	return nil
}

// parseMasterIndex returns a *uint32 from the masters spec.
func parseMasterIndex(spec string) *uint32 {
	if spec == "-" || spec == "" {
		return nil
	}
	parts := strings.Split(spec, ",")
	v, err := strconv.ParseUint(strings.TrimSpace(parts[0]), 0, 32)
	if err != nil {
		return nil
	}
	u := uint32(v)
	return &u
}

// masterIdx returns the master index value or 0 if nil.
func masterIdx(m *uint32) uint32 {
	if m != nil {
		return *m
	}
	return 0
}

// parsePositionList parses a position spec. "-" or empty means nil (all).
func parsePositionList(spec string) []uint16 {
	if spec == "-" || spec == "" {
		return nil
	}
	var result []uint16
	for _, part := range strings.Split(spec, ",") {
		part = strings.TrimSpace(part)
		if strings.Contains(part, "-") {
			bounds := strings.SplitN(part, "-", 2)
			lo, _ := strconv.ParseUint(bounds[0], 0, 16)
			hi, _ := strconv.ParseUint(bounds[1], 0, 16)
			for i := lo; i <= hi; i++ {
				result = append(result, uint16(i))
			}
		} else {
			v, _ := strconv.ParseUint(part, 0, 16)
			result = append(result, uint16(v))
		}
	}
	return result
}
