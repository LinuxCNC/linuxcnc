package main

import (
	"encoding/base64"
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
		data, err := base64.StdEncoding.DecodeString(result.Data)
		if err != nil {
			data = []byte(result.Data)
		}
		if opts.DataType != "" {
			fmt.Println(formatSdoData(data, opts.DataType))
		} else {
			// Default: hex dump like IgH tool
			for i := 0; i < len(data); i += 16 {
				fmt.Printf("%04x  ", uint16(addr)+uint16(i))
				end := i + 16
				if end > len(data) {
					end = len(data)
				}
				for j := i; j < end; j++ {
					fmt.Printf("%02x ", data[j])
				}
				fmt.Println()
			}
		}
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
		data, err := parseHexBytes(args[1])
		if err != nil {
			return fmt.Errorf("invalid data '%s': %v", args[1], err)
		}
		req := RegWriteRequest{
			Address:   uint16(addr),
			Emergency: opts.Emergency,
			Data:      base64.StdEncoding.EncodeToString(data),
		}
		_, err = client.RegWrite(masterIndex, pos, uint16(addr), req)
		if err != nil {
			return err
		}
	}
	return nil
}

// parseHexBytes parses a hex string like "0x1234" or "ab cd ef" into bytes.
func parseHexBytes(s string) ([]byte, error) {
	// Try as a single integer value first
	if v, err := strconv.ParseUint(s, 0, 64); err == nil {
		// Auto-size
		switch {
		case v <= 0xFF:
			return []byte{byte(v)}, nil
		case v <= 0xFFFF:
			return []byte{byte(v), byte(v >> 8)}, nil
		case v <= 0xFFFFFFFF:
			return []byte{byte(v), byte(v >> 8), byte(v >> 16), byte(v >> 24)}, nil
		default:
			b := make([]byte, 8)
			for i := range b {
				b[i] = byte(v >> (i * 8))
			}
			return b, nil
		}
	}
	// Try as space-separated hex bytes
	var result []byte
	for _, part := range splitFields(s) {
		v, err := strconv.ParseUint(part, 16, 8)
		if err != nil {
			return nil, fmt.Errorf("invalid hex byte '%s'", part)
		}
		result = append(result, byte(v))
	}
	if len(result) == 0 {
		return nil, fmt.Errorf("empty data")
	}
	return result, nil
}

func splitFields(s string) []string {
	var fields []string
	field := ""
	for _, c := range s {
		if c == ' ' || c == '\t' || c == ',' {
			if field != "" {
				fields = append(fields, field)
				field = ""
			}
		} else {
			field += string(c)
		}
	}
	if field != "" {
		fields = append(fields, field)
	}
	return fields
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
		data, err := base64.StdEncoding.DecodeString(result.Words)
		if err != nil {
			data = []byte(result.Words)
		}
		if opts.OutputFile != "" {
			return os.WriteFile(opts.OutputFile, data, 0644)
		}
		os.Stdout.Write(data)
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
		_, err := client.SiiWrite(masterIndex, pos, SiiData{Words: base64.StdEncoding.EncodeToString(inputData)})
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
		if result.Result != 0 {
			return fmt.Errorf("FoE read failed: result=%d error_code=%d", result.Result, result.ErrorCode)
		}
		data, err := base64.StdEncoding.DecodeString(result.Data)
		if err != nil {
			data = []byte(result.Data)
		}
		if opts.OutputFile != "" {
			return os.WriteFile(opts.OutputFile, data, 0644)
		}
		os.Stdout.Write(data)
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
			Data:     base64.StdEncoding.EncodeToString(inputData),
		}
		result, err := client.FoeWrite(masterIndex, pos, args[0], req)
		if err != nil {
			return err
		}
		if result.Result != 0 {
			return fmt.Errorf("FoE write failed: result=%d error_code=%d", result.Result, result.ErrorCode)
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
		if result.ErrorCode != 0 {
			return fmt.Errorf("SoE read error: 0x%04x", result.ErrorCode)
		}
		data, err := base64.StdEncoding.DecodeString(result.Data)
		if err != nil {
			data = []byte(result.Data)
		}
		if opts.DataType != "" {
			fmt.Println(formatSdoData(data, opts.DataType))
		} else {
			fmt.Println(formatSdoData(data, ""))
		}
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
		data, err := encodeSdoValue(args[2], opts.DataType)
		if err != nil {
			return err
		}
		req := SoeWriteRequest{
			DriveNo: uint8(driveNo),
			Idn:     uint16(idn),
			Data:    base64.StdEncoding.EncodeToString(data),
		}
		result, err := client.SoeWrite(masterIndex, pos, uint8(driveNo), uint16(idn), req)
		if err != nil {
			return err
		}
		if result.ErrorCode != 0 {
			return fmt.Errorf("SoE write error: 0x%04x", result.ErrorCode)
		}
	}
	return nil
}
