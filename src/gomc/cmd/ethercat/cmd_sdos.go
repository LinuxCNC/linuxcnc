package main

import (
	"encoding/base64"
	"encoding/binary"
	"fmt"
	"math"
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

		data, err := base64.StdEncoding.DecodeString(result.Data)
		if err != nil {
			// Try raw string fallback
			data = []byte(result.Data)
		}

		if result.AbortCode != 0 {
			return fmt.Errorf("SDO abort code 0x%08x", result.AbortCode)
		}

		formatted := formatSdoData(data, opts.DataType)
		fmt.Println(formatted)
	}
	return nil
}

func formatSdoData(data []byte, dataType string) string {
	switch strings.ToLower(dataType) {
	case "bool":
		if len(data) >= 1 {
			if data[0] != 0 {
				return "0x01 1"
			}
			return "0x00 0"
		}
	case "int8", "sint8":
		if len(data) >= 1 {
			v := int8(data[0])
			return fmt.Sprintf("0x%02x %d", uint8(v), v)
		}
	case "uint8":
		if len(data) >= 1 {
			return fmt.Sprintf("0x%02x %d", data[0], data[0])
		}
	case "int16", "sint16":
		if len(data) >= 2 {
			v := int16(binary.LittleEndian.Uint16(data))
			return fmt.Sprintf("0x%04x %d", uint16(v), v)
		}
	case "uint16":
		if len(data) >= 2 {
			v := binary.LittleEndian.Uint16(data)
			return fmt.Sprintf("0x%04x %d", v, v)
		}
	case "int32", "sint32":
		if len(data) >= 4 {
			v := int32(binary.LittleEndian.Uint32(data))
			return fmt.Sprintf("0x%08x %d", uint32(v), v)
		}
	case "uint32":
		if len(data) >= 4 {
			v := binary.LittleEndian.Uint32(data)
			return fmt.Sprintf("0x%08x %d", v, v)
		}
	case "int64", "sint64":
		if len(data) >= 8 {
			v := int64(binary.LittleEndian.Uint64(data))
			return fmt.Sprintf("0x%016x %d", uint64(v), v)
		}
	case "uint64":
		if len(data) >= 8 {
			v := binary.LittleEndian.Uint64(data)
			return fmt.Sprintf("0x%016x %d", v, v)
		}
	case "float", "real32", "float32":
		if len(data) >= 4 {
			v := math.Float32frombits(binary.LittleEndian.Uint32(data))
			return fmt.Sprintf("%g", v)
		}
	case "double", "real64", "float64":
		if len(data) >= 8 {
			v := math.Float64frombits(binary.LittleEndian.Uint64(data))
			return fmt.Sprintf("%g", v)
		}
	case "string", "visible_string":
		return string(data)
	case "octet_string":
		return string(data)
	case "":
		// No type specified — print raw data like IgH tool
		return printRawData(data)
	}
	// Fallback: raw hex
	return printRawData(data)
}

// printRawData formats bytes as "0xHH 0xHH 0xHH" — matching IgH tool's printRawData().
func printRawData(data []byte) string {
	var sb strings.Builder
	for i, b := range data {
		if i > 0 {
			sb.WriteByte(' ')
		}
		fmt.Fprintf(&sb, "0x%02x", b)
	}
	return sb.String()
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

	data, err := encodeSdoValue(args[2], opts.DataType)
	if err != nil {
		return err
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
			Data:             base64.StdEncoding.EncodeToString(data),
		}
		result, err := client.SdoDownload(masterIndex, pos, uint16(sdoIndex), uint8(subIndex), req)
		if err != nil {
			return err
		}
		if result.AbortCode != 0 {
			return fmt.Errorf("SDO abort code 0x%08x", result.AbortCode)
		}
	}
	return nil
}

func encodeSdoValue(valueStr, dataType string) ([]byte, error) {
	switch strings.ToLower(dataType) {
	case "bool":
		v, err := strconv.ParseUint(valueStr, 0, 8)
		if err != nil {
			return nil, fmt.Errorf("invalid bool value: %v", err)
		}
		return []byte{byte(v)}, nil
	case "int8", "sint8":
		v, err := strconv.ParseInt(valueStr, 0, 8)
		if err != nil {
			return nil, fmt.Errorf("invalid int8 value: %v", err)
		}
		return []byte{byte(v)}, nil
	case "uint8":
		v, err := strconv.ParseUint(valueStr, 0, 8)
		if err != nil {
			return nil, fmt.Errorf("invalid uint8 value: %v", err)
		}
		return []byte{byte(v)}, nil
	case "int16", "sint16":
		v, err := strconv.ParseInt(valueStr, 0, 16)
		if err != nil {
			return nil, fmt.Errorf("invalid int16 value: %v", err)
		}
		b := make([]byte, 2)
		binary.LittleEndian.PutUint16(b, uint16(v))
		return b, nil
	case "uint16":
		v, err := strconv.ParseUint(valueStr, 0, 16)
		if err != nil {
			return nil, fmt.Errorf("invalid uint16 value: %v", err)
		}
		b := make([]byte, 2)
		binary.LittleEndian.PutUint16(b, uint16(v))
		return b, nil
	case "int32", "sint32":
		v, err := strconv.ParseInt(valueStr, 0, 32)
		if err != nil {
			return nil, fmt.Errorf("invalid int32 value: %v", err)
		}
		b := make([]byte, 4)
		binary.LittleEndian.PutUint32(b, uint32(v))
		return b, nil
	case "uint32":
		v, err := strconv.ParseUint(valueStr, 0, 32)
		if err != nil {
			return nil, fmt.Errorf("invalid uint32 value: %v", err)
		}
		b := make([]byte, 4)
		binary.LittleEndian.PutUint32(b, uint32(v))
		return b, nil
	case "int64", "sint64":
		v, err := strconv.ParseInt(valueStr, 0, 64)
		if err != nil {
			return nil, fmt.Errorf("invalid int64 value: %v", err)
		}
		b := make([]byte, 8)
		binary.LittleEndian.PutUint64(b, uint64(v))
		return b, nil
	case "uint64":
		v, err := strconv.ParseUint(valueStr, 0, 64)
		if err != nil {
			return nil, fmt.Errorf("invalid uint64 value: %v", err)
		}
		b := make([]byte, 8)
		binary.LittleEndian.PutUint64(b, uint64(v))
		return b, nil
	case "float", "real32", "float32":
		v, err := strconv.ParseFloat(valueStr, 32)
		if err != nil {
			return nil, fmt.Errorf("invalid float value: %v", err)
		}
		b := make([]byte, 4)
		binary.LittleEndian.PutUint32(b, math.Float32bits(float32(v)))
		return b, nil
	case "double", "real64", "float64":
		v, err := strconv.ParseFloat(valueStr, 64)
		if err != nil {
			return nil, fmt.Errorf("invalid double value: %v", err)
		}
		b := make([]byte, 8)
		binary.LittleEndian.PutUint64(b, math.Float64bits(v))
		return b, nil
	case "string", "octet_string", "visible_string":
		return []byte(valueStr), nil
	case "":
		// No type specified — try to parse as integer
		v, err := strconv.ParseInt(valueStr, 0, 64)
		if err != nil {
			// Treat as raw string
			return []byte(valueStr), nil
		}
		// Auto-size based on value magnitude
		switch {
		case v >= 0 && v <= 0xFF:
			return []byte{byte(v)}, nil
		case v >= -128 && v <= 127:
			return []byte{byte(v)}, nil
		case v >= 0 && v <= 0xFFFF:
			b := make([]byte, 2)
			binary.LittleEndian.PutUint16(b, uint16(v))
			return b, nil
		case v >= -32768 && v <= 32767:
			b := make([]byte, 2)
			binary.LittleEndian.PutUint16(b, uint16(v))
			return b, nil
		case v >= 0 && v <= 0xFFFFFFFF:
			b := make([]byte, 4)
			binary.LittleEndian.PutUint32(b, uint32(v))
			return b, nil
		default:
			b := make([]byte, 8)
			binary.LittleEndian.PutUint64(b, uint64(v))
			return b, nil
		}
	}
	return nil, fmt.Errorf("unsupported data type: %s", dataType)
}
