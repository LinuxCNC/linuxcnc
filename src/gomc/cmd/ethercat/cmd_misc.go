package main

import (
	"encoding/base64"
	"encoding/binary"
	"fmt"
	"strconv"
)

func init() {
	registerCommand(&Command{
		Name:  "alias",
		Brief: "Write alias addresses.",
		Run:   cmdAlias,
	})
	registerCommand(&Command{
		Name:  "crc",
		Brief: "CRC error register diagnosis.",
		Run:   cmdCrc,
	})
	registerCommand(&Command{
		Name:  "cstruct",
		Brief: "Generate slave PDO info as C code.",
		Run:   cmdCstruct,
	})
	registerCommand(&Command{
		Name:  "eoe",
		Brief: "Display Ethernet over EtherCAT statistics.",
		Run:   cmdEoe,
	})
	registerCommand(&Command{
		Name:  "graph",
		Brief: "Output the bus topology as a graph.",
		Run:   cmdGraph,
	})
	registerCommand(&Command{
		Name:  "ip",
		Brief: "Set EoE IP parameters.",
		Run:   cmdIp,
	})
	registerCommand(&Command{
		Name:  "xml",
		Brief: "Generate slave information XML.",
		Run:   cmdXml,
	})
}

func cmdAlias(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) < 1 {
		return fmt.Errorf("usage: alias <ALIAS>")
	}
	alias, err := strconv.ParseUint(args[0], 0, 16)
	if err != nil {
		return fmt.Errorf("invalid alias '%s': %v", args[0], err)
	}

	masterIndex := parseMasterIndex(opts.Masters)
	positions := parsePositionList(opts.Positions)
	if positions == nil {
		return fmt.Errorf("'alias' requires slave selection (use -p)")
	}

	for _, pos := range positions {
		err := writeAlias(client, masterIndex, pos, uint16(alias), opts.Force)
		if err != nil {
			return err
		}
	}
	return nil
}

func writeAlias(client *EthercatClient, masterIndex *uint32, pos uint16, alias uint16, force bool) error {
	// Read first 8 SII words.
	result, err := client.SiiRead(masterIndex, pos, 0, 8)
	if err != nil {
		return fmt.Errorf("failed to read SII: %v", err)
	}

	// SII words are returned as base64-encoded binary.
	data, err := base64.StdEncoding.DecodeString(result.Words)
	if err != nil {
		data = []byte(result.Words)
	}
	if len(data) < 16 {
		return fmt.Errorf("SII data too short (%d bytes)", len(data))
	}

	// Word 4 = alias (little-endian).
	binary.LittleEndian.PutUint16(data[8:10], alias)

	// Recalculate CRC over words 0-6 (bytes 0-13) and store in word 7 (bytes 14-15).
	crc := siiCrc(data[:14])
	data[14] = crc
	data[15] = 0

	// Write back.
	sii := SiiData{
		Offset: 0,
		Nwords: 8,
		Words:  base64.StdEncoding.EncodeToString(data[:16]),
	}
	_, err = client.SiiWrite(masterIndex, pos, sii)
	if err != nil {
		return fmt.Errorf("failed to write SII: %v", err)
	}
	return nil
}

// siiCrc computes the SII CRC-8 over the given bytes (ETG.1000.6 algorithm).
func siiCrc(data []byte) byte {
	var crc uint8 = 0xFF
	for _, b := range data {
		for bit := 0; bit < 8; bit++ {
			if (crc^b)&0x01 != 0 {
				crc = (crc >> 1) ^ 0xA6
			} else {
				crc >>= 1
			}
			b >>= 1
		}
	}
	return crc
}

func cmdCrc(client *EthercatClient, opts *GlobalOpts, args []string) error {
	// Handle optional "reset" argument.
	if len(args) > 1 {
		return fmt.Errorf("'crc' takes either no or 'reset' argument!")
	}

	reset := false
	if len(args) == 1 {
		if args[0] != "reset" {
			return fmt.Errorf("'crc' takes either no or 'reset' argument!")
		}
		reset = true
	}

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

	if reset {
		// Write 20 zero bytes to register 0x0300 on each slave.
		zeroData := base64.StdEncoding.EncodeToString(make([]byte, 20))
		for _, pos := range positions {
			_, err := client.RegWrite(masterIndex, pos, 0x0300, RegWriteRequest{
				Address:   0x0300,
				Emergency: false,
				Data:      zeroData,
			})
			if err != nil {
				return fmt.Errorf("failed to reset CRC on slave %d: %v", pos, err)
			}
		}
		return nil
	}

	// Print header.
	fmt.Printf("   |")
	for port := 0; port < 4; port++ {
		fmt.Printf("Port %d         |", port)
	}
	fmt.Println()

	fmt.Printf("   |")
	for port := 0; port < 4; port++ {
		_ = port
		fmt.Printf("CRC PHY FWD NXT|")
	}
	fmt.Println()

	// Read CRC registers (0x0300, 20 bytes) for each slave.
	for _, pos := range positions {
		slave, err := client.GetSlave(masterIndex, pos)
		if err != nil {
			fmt.Printf("%3d|(read failed)\n", pos)
			continue
		}

		result, err := client.RegRead(masterIndex, pos, 0x0300, 20)
		if err != nil {
			fmt.Printf("%3d|(read failed)\n", pos)
			continue
		}
		data, err := base64.StdEncoding.DecodeString(result.Data)
		if err != nil || len(data) < 12 {
			fmt.Printf("%3d|(decode failed)\n", pos)
			continue
		}

		fmt.Printf("%3d|", pos)
		for port := 0; port < 4; port++ {
			// Check if port loop is closed (bit 1 of link bitmask).
			loopClosed := slave.Ports[port].Link&0x02 != 0
			if loopClosed {
				fmt.Printf("               |")
				continue
			}

			// CRC error counter: byte at offset 0 + port*2
			crc := uint8(0)
			if 0+port*2 < len(data) {
				crc = data[0+port*2]
			}
			// PHY error counter: byte at offset 1 + port*2
			phy := uint8(0)
			if 1+port*2 < len(data) {
				phy = data[1+port*2]
			}
			// Forwarded RX error: byte at offset 8 + port
			fwd := uint8(0)
			if 8+port < len(data) {
				fwd = data[8+port]
			}

			fmt.Printf("%3d %3d %3d", crc, phy, fwd)

			// NXT column: topology arrow based on next_slave.
			nextSlave := slave.Ports[port].NextSlave
			if nextSlave == pos-1 {
				fmt.Printf("   \u2191")
			} else if nextSlave == pos+1 {
				fmt.Printf("   \u2193")
			} else if nextSlave != 0xFFFF {
				fmt.Printf("%4d", nextSlave)
			} else {
				fmt.Printf("    ")
			}
			fmt.Printf("|")
		}

		// Slave name (truncated to 11 chars).
		name := slave.Name
		if len(name) > 11 {
			name = name[:11]
		}
		fmt.Printf("%s\n", name)
	}
	return nil
}

func cmdCstruct(client *EthercatClient, opts *GlobalOpts, args []string) error {
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

	mi := masterIdx(masterIndex)

	for _, pos := range positions {
		slave, err := client.GetSlave(masterIndex, pos)
		if err != nil {
			return err
		}

		fmt.Printf("/* Master %d, Slave %d", mi, pos)
		if slave.Name != "" {
			fmt.Printf(", \"%s\"", slave.Name)
		}
		fmt.Printf("\n * Vendor ID:       0x%08x\n", slave.VendorId)
		fmt.Printf(" * Product code:    0x%08x\n", slave.ProductCode)
		fmt.Printf(" * Revision number: 0x%08x\n", slave.RevisionNumber)
		fmt.Printf(" */\n\n")

		// Collect all PDO entries.
		hasEntries := false
		entryPos := 0
		for smIdx := uint32(0); smIdx < uint32(slave.SyncCount); smIdx++ {
			sm, err := client.GetSlaveSync(masterIndex, pos, smIdx)
			if err != nil || sm.PdoCount == 0 {
				continue
			}
			for pdoIdx := uint32(0); pdoIdx < uint32(sm.PdoCount); pdoIdx++ {
				pdo, err := client.GetSlaveSyncPdo(masterIndex, pos, smIdx, pdoIdx)
				if err != nil || pdo.EntryCount == 0 {
					continue
				}
				if !hasEntries {
					fmt.Printf("ec_pdo_entry_info_t slave_%d_pdo_entries[] = {\n", pos)
					hasEntries = true
				}
				for entryIdx := uint32(0); entryIdx < uint32(pdo.EntryCount); entryIdx++ {
					entry, err := client.GetSlaveSyncPdoEntry(masterIndex, pos, smIdx, pdoIdx, entryIdx)
					if err != nil {
						continue
					}
					fmt.Printf("    {0x%04x, 0x%02x, %d},\n", entry.Index, entry.Subindex, entry.BitLength)
					entryPos++
				}
			}
		}
		if hasEntries {
			fmt.Printf("};\n\n")
		}

		// PDO info array.
		hasPdos := false
		pdoPos := 0
		entryPos = 0
		for smIdx := uint32(0); smIdx < uint32(slave.SyncCount); smIdx++ {
			sm, err := client.GetSlaveSync(masterIndex, pos, smIdx)
			if err != nil || sm.PdoCount == 0 {
				continue
			}
			if !hasPdos {
				fmt.Printf("ec_pdo_info_t slave_%d_pdos[] = {\n", pos)
				hasPdos = true
			}
			for pdoIdx := uint32(0); pdoIdx < uint32(sm.PdoCount); pdoIdx++ {
				pdo, err := client.GetSlaveSyncPdo(masterIndex, pos, smIdx, pdoIdx)
				if err != nil {
					continue
				}
				entRef := "NULL"
				if pdo.EntryCount > 0 {
					entRef = fmt.Sprintf("slave_%d_pdo_entries + %d", pos, entryPos)
				}
				fmt.Printf("    {0x%04x, %d, %s},\n",
					pdo.Index, pdo.EntryCount, entRef)
				entryPos += int(pdo.EntryCount)
				pdoPos++
			}
		}
		if hasPdos {
			fmt.Printf("};\n\n")
		}

		// Sync manager info.
		fmt.Printf("ec_sync_info_t slave_%d_syncs[] = {\n", pos)
		pdoPos = 0
		for smIdx := uint32(0); smIdx < uint32(slave.SyncCount); smIdx++ {
			sm, err := client.GetSlaveSync(masterIndex, pos, smIdx)
			if err != nil {
				continue
			}
			dirStr := "EC_DIR_INVALID"
			switch sm.ControlRegister & 0x0C {
			case 0x04:
				dirStr = "EC_DIR_INPUT"
			case 0x08:
				dirStr = "EC_DIR_OUTPUT"
			}
			wdStr := "EC_WD_DISABLE"
			if sm.ControlRegister&0x40 != 0 {
				wdStr = "EC_WD_ENABLE"
			}
			pdoRef := "NULL"
			if sm.PdoCount > 0 {
				pdoRef = fmt.Sprintf("slave_%d_pdos + %d", pos, pdoPos)
			}
			fmt.Printf("    {%d, %s, %d, %s, %s},\n",
				smIdx, dirStr, sm.PdoCount, pdoRef, wdStr)
			pdoPos += int(sm.PdoCount)
		}
		fmt.Printf("    {0xff}\n")
		fmt.Printf("};\n\n")
	}
	return nil
}

func cmdEoe(client *EthercatClient, opts *GlobalOpts, args []string) error {
	masterIndex := parseMasterIndex(opts.Masters)
	master, err := client.GetMaster(masterIndex)
	if err != nil {
		return err
	}

	if master.EoeHandlerCount == 0 {
		return nil
	}

	fmt.Printf("%-12s %-6s %-8s %10s %10s %10s %10s %7s\n",
		"Interface", "Slave", "State", "RxBytes", "RxRate", "TxBytes", "TxRate", "TxQueue")

	for i := uint16(0); i < uint16(master.EoeHandlerCount); i++ {
		eoe, err := client.GetEoeHandler(masterIndex, i)
		if err != nil {
			continue
		}
		stateStr := "DOWN"
		if eoe.Open {
			stateStr = "UP"
		}
		fmt.Printf("%-12s %5d  %-8s %10d %10d %10d %10d %7d\n",
			eoe.Name, eoe.SlavePosition, stateStr,
			eoe.RxBytes, eoe.RxRate, eoe.TxBytes, eoe.TxRate, eoe.TxQueueSize)
	}
	return nil
}

func cmdGraph(client *EthercatClient, opts *GlobalOpts, args []string) error {
	masterIndex := parseMasterIndex(opts.Masters)
	master, err := client.GetMaster(masterIndex)
	if err != nil {
		return err
	}

	mi := masterIdx(masterIndex)
	fmt.Println("digraph EtherCAT {")
	fmt.Printf("  rankdir=LR;\n")
	fmt.Printf("  Master%d [shape=record,label=\"Master %d\"];\n", mi, mi)

	for pos := uint16(0); pos < uint16(master.SlaveCount); pos++ {
		slave, err := client.GetSlave(masterIndex, pos)
		if err != nil {
			continue
		}
		label := fmt.Sprintf("Slave %d\\n%s\\n0x%08x/0x%08x",
			pos, slave.Name, slave.VendorId, slave.ProductCode)
		fmt.Printf("  Slave%d_%d [shape=record,label=\"%s\"];\n", mi, pos, label)

		if pos == 0 {
			fmt.Printf("  Master%d -> Slave%d_%d;\n", mi, mi, pos)
		} else {
			fmt.Printf("  Slave%d_%d -> Slave%d_%d;\n", mi, pos-1, mi, pos)
		}
	}
	fmt.Println("}")
	return nil
}

func cmdIp(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) == 0 {
		return nil
	}
	if len(args)%2 != 0 {
		return fmt.Errorf("'ip' needs an even number of arguments (key value pairs)")
	}

	masterIndex := parseMasterIndex(opts.Masters)
	positions := parsePositionList(opts.Positions)
	if positions == nil {
		return fmt.Errorf("'ip' requires a single slave (use -p)")
	}
	if len(positions) != 1 {
		return fmt.Errorf("'ip' requires exactly one slave")
	}

	var req EoeIpRequest

	for i := 0; i < len(args); i += 2 {
		key := args[i]
		val := args[i+1]
		switch key {
		case "link", "mac_address":
			req.MacAddress = &val
		case "addr", "ip_address":
			// May contain /prefix — split and handle subnet
			if idx := indexOf(val, '/'); idx >= 0 {
				ipPart := val[:idx]
				prefix := val[idx+1:]
				req.IpAddress = &ipPart
				mask := prefixToMask(prefix)
				if mask != "" {
					req.SubnetMask = &mask
				}
			} else {
				req.IpAddress = &val
			}
		case "default", "default_gateway":
			req.Gateway = &val
		case "dns", "dns_address":
			req.Dns = &val
		case "name", "hostname":
			req.Hostname = &val
		default:
			return fmt.Errorf("unknown argument '%s'", key)
		}
	}

	_, err := client.SetEoeIp(masterIndex, positions[0], req)
	return err
}

func indexOf(s string, c byte) int {
	for i := 0; i < len(s); i++ {
		if s[i] == c {
			return i
		}
	}
	return -1
}

func prefixToMask(prefix string) string {
	bits, err := strconv.ParseUint(prefix, 10, 8)
	if err != nil || bits > 32 {
		return ""
	}
	mask := uint32(0xFFFFFFFF) << (32 - bits)
	return fmt.Sprintf("%d.%d.%d.%d",
		(mask>>24)&0xFF, (mask>>16)&0xFF, (mask>>8)&0xFF, mask&0xFF)
}

func cmdXml(client *EthercatClient, opts *GlobalOpts, args []string) error {
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

	fmt.Println("<?xml version=\"1.0\" ?>")
	fmt.Println("<EtherCATInfoList>")
	for _, pos := range positions {
		slave, err := client.GetSlave(masterIndex, pos)
		if err != nil {
			continue
		}
		fmt.Println("  <EtherCATInfo>")
		fmt.Println("    <Vendor>")
		fmt.Printf("      <Id>0x%08x</Id>\n", slave.VendorId)
		fmt.Println("    </Vendor>")
		fmt.Println("    <Descriptions>")
		fmt.Println("      <Devices>")
		fmt.Println("        <Device>")
		fmt.Printf("          <Type ProductCode=\"#x%08x\" RevisionNo=\"#x%08x\">%s</Type>\n",
			slave.ProductCode, slave.RevisionNumber, slave.Name)
		fmt.Printf("          <Name>%s</Name>\n", slave.Name)

		for smIdx := uint32(0); smIdx < uint32(slave.SyncCount); smIdx++ {
			sm, err := client.GetSlaveSync(masterIndex, pos, smIdx)
			if err != nil {
				continue
			}
			enableStr := "0"
			if sm.Enable {
				enableStr = "1"
			}
			fmt.Printf("          <Sm MinSize=\"%d\" DefaultSize=\"%d\" ",
				sm.DefaultSize, sm.DefaultSize)
			fmt.Printf("ControlByte=\"0x%02x\" Enable=\"%s\">SM%d</Sm>\n",
				sm.ControlRegister, enableStr, smIdx)
		}

		for smIdx := uint32(0); smIdx < uint32(slave.SyncCount); smIdx++ {
			sm, err := client.GetSlaveSync(masterIndex, pos, smIdx)
			if err != nil || sm.PdoCount == 0 {
				continue
			}
			pdoTag := "TxPdo"
			if sm.ControlRegister&0x0C == 0x08 {
				pdoTag = "RxPdo"
			}
			for pdoIdx := uint32(0); pdoIdx < uint32(sm.PdoCount); pdoIdx++ {
				pdo, err := client.GetSlaveSyncPdo(masterIndex, pos, smIdx, pdoIdx)
				if err != nil {
					continue
				}
				fmt.Printf("          <%s Sm=\"%d\">\n", pdoTag, smIdx)
				fmt.Printf("            <Index>#x%04x</Index>\n", pdo.Index)
				fmt.Printf("            <Name>%s</Name>\n", pdo.Name)
				for entryIdx := uint32(0); entryIdx < uint32(pdo.EntryCount); entryIdx++ {
					entry, err := client.GetSlaveSyncPdoEntry(masterIndex, pos, smIdx, pdoIdx, entryIdx)
					if err != nil {
						continue
					}
					fmt.Printf("            <Entry>\n")
					fmt.Printf("              <Index>#x%04x</Index>\n", entry.Index)
					fmt.Printf("              <SubIndex>%d</SubIndex>\n", entry.Subindex)
					fmt.Printf("              <BitLen>%d</BitLen>\n", entry.BitLength)
					fmt.Printf("              <Name>%s</Name>\n", entry.Name)
					fmt.Printf("            </Entry>\n")
				}
				fmt.Printf("          </%s>\n", pdoTag)
			}
		}

		fmt.Println("        </Device>")
		fmt.Println("      </Devices>")
		fmt.Println("    </Descriptions>")
		fmt.Println("  </EtherCATInfo>")
	}
	fmt.Println("</EtherCATInfoList>")
	return nil
}
