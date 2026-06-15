package main

import (
	"fmt"
	"os"
	"strconv"
)

func init() {
	registerCommand(&Command{
		Name:  "reg_read",
		Brief: "Output a slave's register contents.",
		Run:   cmdRegRead,
	})
	registerCommand(&Command{
		Name:  "reg_write",
		Brief: "Write data to a slave's registers.",
		Run:   cmdRegWrite,
	})
	registerCommand(&Command{
		Name:  "sii_read",
		Brief: "Output a slave's SII contents.",
		Run:   cmdSiiRead,
	})
	registerCommand(&Command{
		Name:  "sii_write",
		Brief: "Write slave's SII contents.",
		Run:   cmdSiiWrite,
	})
	registerCommand(&Command{
		Name:  "foe_read",
		Brief: "Read a file from a slave via FoE.",
		Run:   cmdFoeRead,
	})
	registerCommand(&Command{
		Name:  "foe_write",
		Brief: "Store a file on a slave via FoE.",
		Run:   cmdFoeWrite,
	})
	registerCommand(&Command{
		Name:  "soe_read",
		Brief: "Read an SoE IDN from a slave.",
		Run:   cmdSoeRead,
	})
	registerCommand(&Command{
		Name:  "soe_write",
		Brief: "Write an SoE IDN to a slave.",
		Run:   cmdSoeWrite,
	})
}

func cmdRegRead(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) < 2 {
		return fmt.Errorf("usage: reg_read <ADDRESS> <SIZE>")
	}
	addr, err := strconv.ParseUint(args[0], 0, 16)
	if err != nil {
		return fmt.Errorf("invalid address '%s': %v", args[0], err)
	}
	size, err := strconv.ParseUint(args[1], 0, 32)
	if err != nil {
		return fmt.Errorf("invalid size '%s': %v", args[1], err)
	}

	masterIndex := parseMasterIndex(opts.Masters)
	positions := parsePositionList(opts.Positions)
	if positions == nil {
		positions = []uint16{0}
	}

	for _, pos := range positions {
		result, err := client.RegRead(masterIndex, pos, uint16(addr), uint32(size))
		if err != nil {
			return err
		}
		fmt.Printf("%s\n", result.Data)
	}
	return nil
}

func cmdRegWrite(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) < 2 {
		return fmt.Errorf("usage: reg_write <ADDRESS> <DATA>")
	}
	addr, err := strconv.ParseUint(args[0], 0, 16)
	if err != nil {
		return fmt.Errorf("invalid address '%s': %v", args[0], err)
	}

	masterIndex := parseMasterIndex(opts.Masters)
	positions := parsePositionList(opts.Positions)
	if positions == nil {
		positions = []uint16{0}
	}

	for _, pos := range positions {
		req := RegWriteRequest{
			Address:   uint16(addr),
			Emergency: opts.Emergency,
			Data:      args[1],
		}
		_, err := client.RegWrite(masterIndex, pos, uint16(addr), req)
		if err != nil {
			return err
		}
	}
	return nil
}

func cmdSiiRead(client *EthercatClient, opts *GlobalOpts, args []string) error {
	masterIndex := parseMasterIndex(opts.Masters)
	positions := parsePositionList(opts.Positions)
	if positions == nil {
		positions = []uint16{0}
	}

	for _, pos := range positions {
		result, err := client.SiiRead(masterIndex, pos, 0, 0)
		if err != nil {
			return err
		}
		if opts.OutputFile != "" {
			return os.WriteFile(opts.OutputFile, []byte(result.Words), 0644)
		}
		fmt.Print(result.Words)
	}
	return nil
}

func cmdSiiWrite(client *EthercatClient, opts *GlobalOpts, args []string) error {
	masterIndex := parseMasterIndex(opts.Masters)
	positions := parsePositionList(opts.Positions)
	if positions == nil {
		positions = []uint16{0}
	}

	var inputData []byte
	var err error
	if len(args) > 0 {
		inputData, err = os.ReadFile(args[0])
	} else {
		inputData, err = os.ReadFile("/dev/stdin")
	}
	if err != nil {
		return fmt.Errorf("failed to read input: %v", err)
	}

	for _, pos := range positions {
		_, err := client.SiiWrite(masterIndex, pos, SiiData{Words: string(inputData)})
		if err != nil {
			return err
		}
	}
	return nil
}

func cmdFoeRead(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("usage: foe_read <FILENAME>")
	}

	masterIndex := parseMasterIndex(opts.Masters)
	positions := parsePositionList(opts.Positions)
	if positions == nil {
		positions = []uint16{0}
	}

	for _, pos := range positions {
		result, err := client.FoeRead(masterIndex, pos, args[0])
		if err != nil {
			return err
		}
		if opts.OutputFile != "" {
			return os.WriteFile(opts.OutputFile, []byte(result.Data), 0644)
		}
		fmt.Print(result.Data)
	}
	return nil
}

func cmdFoeWrite(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("usage: foe_write <FILENAME>")
	}

	masterIndex := parseMasterIndex(opts.Masters)
	positions := parsePositionList(opts.Positions)
	if positions == nil {
		positions = []uint16{0}
	}

	var inputData []byte
	var err error
	if len(args) > 1 {
		inputData, err = os.ReadFile(args[1])
	} else {
		inputData, err = os.ReadFile("/dev/stdin")
	}
	if err != nil {
		return fmt.Errorf("failed to read input: %v", err)
	}

	for _, pos := range positions {
		req := FoeWriteRequest{
			FileName: args[0],
			Data:     string(inputData),
		}
		_, err := client.FoeWrite(masterIndex, pos, args[0], req)
		if err != nil {
			return err
		}
	}
	return nil
}

func cmdSoeRead(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) < 2 {
		return fmt.Errorf("usage: soe_read <DRIVE_NO> <IDN>")
	}
	driveNo, err := strconv.ParseUint(args[0], 0, 8)
	if err != nil {
		return fmt.Errorf("invalid drive number '%s': %v", args[0], err)
	}
	idn, err := strconv.ParseUint(args[1], 0, 16)
	if err != nil {
		return fmt.Errorf("invalid IDN '%s': %v", args[1], err)
	}

	masterIndex := parseMasterIndex(opts.Masters)
	positions := parsePositionList(opts.Positions)
	if positions == nil {
		positions = []uint16{0}
	}

	for _, pos := range positions {
		result, err := client.SoeRead(masterIndex, pos, uint8(driveNo), uint16(idn), nil)
		if err != nil {
			return err
		}
		fmt.Printf("%s\n", result.Data)
	}
	return nil
}

func cmdSoeWrite(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) < 3 {
		return fmt.Errorf("usage: soe_write <DRIVE_NO> <IDN> <VALUE>")
	}
	driveNo, err := strconv.ParseUint(args[0], 0, 8)
	if err != nil {
		return fmt.Errorf("invalid drive number '%s': %v", args[0], err)
	}
	idn, err := strconv.ParseUint(args[1], 0, 16)
	if err != nil {
		return fmt.Errorf("invalid IDN '%s': %v", args[1], err)
	}

	masterIndex := parseMasterIndex(opts.Masters)
	positions := parsePositionList(opts.Positions)
	if positions == nil {
		positions = []uint16{0}
	}

	for _, pos := range positions {
		req := SoeWriteRequest{
			DriveNo: uint8(driveNo),
			Idn:     uint16(idn),
			Data:    args[2],
		}
		_, err := client.SoeWrite(masterIndex, pos, uint8(driveNo), uint16(idn), req)
		if err != nil {
			return err
		}
	}
	return nil
}
