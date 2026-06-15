package main

import (
	"fmt"
	"strconv"
	"strings"
)

func init() {
	registerCommand(&Command{
		Name:  "sdos",
		Brief: "List SDO dictionaries.",
		Run:   cmdSdos,
	})
	registerCommand(&Command{
		Name:  "upload",
		Brief: "Read an SDO entry.",
		Run:   cmdUpload,
	})
	registerCommand(&Command{
		Name:  "download",
		Brief: "Write an SDO entry.",
		Run:   cmdDownload,
	})
}

func cmdSdos(client *EthercatClient, opts *GlobalOpts, args []string) error {
	masterIndex := parseMasterIndex(opts.Masters)
	master, err := client.GetMaster(masterIndex)
	if err != nil {
		return err
	}

	positions := parsePositionList(opts.Positions)
	if positions == nil {
		positions = make([]uint16, master.SlaveCount)
		for i := range positions {
			positions[i] = uint16(i)
		}
	}

	for _, pos := range positions {
		slave, err := client.GetSlave(masterIndex, pos)
		if err != nil {
			return err
		}
		for sdoIdx := uint16(0); sdoIdx < slave.SdoCount; sdoIdx++ {
			sdo, err := client.GetSlaveSdo(masterIndex, pos, sdoIdx)
			if err != nil {
				return err
			}
			fmt.Printf("SDO 0x%04x, \"%s\"\n", sdo.SdoIndex, sdo.Name)
			for subIdx := uint8(0); subIdx <= sdo.MaxSubindex; subIdx++ {
				entry, err := client.GetSlaveSdoEntry(masterIndex, pos, int32(sdo.SdoIndex), subIdx)
				if err != nil {
					continue
				}
				accessStr := sdoAccessString(entry.ReadAccess, entry.WriteAccess)
				typeStr := sdoDataTypeString(entry.DataType)
				fmt.Printf("  0x%04x:%02x, %s, %s, %d bit, \"%s\"\n",
					sdo.SdoIndex, subIdx, accessStr, typeStr, entry.BitLength, entry.Description)
			}
		}
	}
	return nil
}

func sdoAccessString(read, write [3]bool) string {
	// 3 access pairs: PreOP, SafeOP, OP
	var s strings.Builder
	for i := 0; i < 3; i++ {
		if read[i] {
			s.WriteByte('r')
		} else {
			s.WriteByte(' ')
		}
		if write[i] {
			s.WriteByte('w')
		} else {
			s.WriteByte(' ')
		}
	}
	return "[" + s.String() + "]"
}

func sdoDataTypeString(dt uint16) string {
	switch dt {
	case 0x0001:
		return "BOOLEAN"
	case 0x0002:
		return "INTEGER8"
	case 0x0003:
		return "INTEGER16"
	case 0x0004:
		return "INTEGER32"
	case 0x0005:
		return "UNSIGNED8"
	case 0x0006:
		return "UNSIGNED16"
	case 0x0007:
		return "UNSIGNED32"
	case 0x0008:
		return "REAL32"
	case 0x0009:
		return "VISIBLE_STRING"
	case 0x000A:
		return "OCTET_STRING"
	case 0x000B:
		return "UNICODE_STRING"
	case 0x0011:
		return "REAL64"
	case 0x0015:
		return "INTEGER64"
	case 0x001B:
		return "UNSIGNED64"
	default:
		if dt == 0 {
			return "---"
		}
		return fmt.Sprintf("type 0x%04x", dt)
	}
}

func cmdUpload(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) < 2 {
		return fmt.Errorf("usage: upload <INDEX> <SUBINDEX>")
	}

	sdoIndex, err := strconv.ParseUint(args[0], 0, 16)
	if err != nil {
		return fmt.Errorf("invalid SDO index '%s': %v", args[0], err)
	}
	subIndex, err := strconv.ParseUint(args[1], 0, 8)
	if err != nil {
		return fmt.Errorf("invalid subindex '%s': %v", args[1], err)
	}

	masterIndex := parseMasterIndex(opts.Masters)
	positions := parsePositionList(opts.Positions)
	if positions == nil {
		positions = []uint16{0}
	}

	for _, pos := range positions {
		result, err := client.SdoUpload(masterIndex, pos, uint16(sdoIndex), uint8(subIndex), nil)
		if err != nil {
			return err
		}
		// Data comes as string (base64 or raw from server).
		fmt.Printf("%s\n", result.Data)
	}
	return nil
}

func cmdDownload(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) < 3 {
		return fmt.Errorf("usage: download <INDEX> <SUBINDEX> <VALUE>")
	}

	sdoIndex, err := strconv.ParseUint(args[0], 0, 16)
	if err != nil {
		return fmt.Errorf("invalid SDO index '%s': %v", args[0], err)
	}
	subIndex, err := strconv.ParseUint(args[1], 0, 8)
	if err != nil {
		return fmt.Errorf("invalid subindex '%s': %v", args[1], err)
	}

	masterIndex := parseMasterIndex(opts.Masters)
	positions := parsePositionList(opts.Positions)
	if positions == nil {
		positions = []uint16{0}
	}

	for _, pos := range positions {
		req := SdoDownloadRequest{
			SdoIndex:         uint16(sdoIndex),
			SdoEntrySubindex: uint8(subIndex),
			Data:             args[2],
		}
		_, err := client.SdoDownload(masterIndex, pos, uint16(sdoIndex), uint8(subIndex), req)
		if err != nil {
			return err
		}
	}
	return nil
}
