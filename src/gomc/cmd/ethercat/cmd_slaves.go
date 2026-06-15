package main

import (
	"fmt"
	"strings"
)

func init() {
	registerCommand(&Command{
		Name:  "slaves",
		Brief: "Display slaves on the bus.",
		Run:   cmdSlaves,
	})
}

func cmdSlaves(client *EthercatClient, opts *GlobalOpts, args []string) error {
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

	if opts.Verbosity == Verbose {
		return slavesVerbose(client, masterIndex, positions)
	}
	return slavesBrief(client, masterIndex, positions)
}

func slavesBrief(client *EthercatClient, masterIndex *uint32, positions []uint16) error {
	type slaveRow struct {
		pos   uint16
		alias uint16
		state string
		flag  string
		name  string
	}

	var rows []slaveRow
	for _, pos := range positions {
		slave, err := client.GetSlave(masterIndex, pos)
		if err != nil {
			return err
		}
		flag := "+"
		if slave.AlState == 0 {
			flag = " "
		}
		if slave.ErrorFlag {
			flag = "E"
		}
		rows = append(rows, slaveRow{
			pos:   pos,
			alias: slave.Alias,
			state: stateString(slave.AlState),
			flag:  flag,
			name:  slave.Name,
		})
	}

	maxPos := 0
	maxState := 0
	for _, r := range rows {
		w := len(fmt.Sprintf("%d", r.pos))
		if w > maxPos {
			maxPos = w
		}
		w = len(r.state)
		if w > maxState {
			maxState = w
		}
	}

	for _, r := range rows {
		fmt.Printf("%*d  %5d:%-3d  %*s  %s  %s\n",
			maxPos, r.pos,
			r.alias, r.pos,
			maxState, r.state,
			r.flag, r.name)
	}
	return nil
}

func slavesVerbose(client *EthercatClient, masterIndex *uint32, positions []uint16) error {
	mi := masterIdx(masterIndex)

	for _, pos := range positions {
		slave, err := client.GetSlave(masterIndex, pos)
		if err != nil {
			return err
		}
		fmt.Printf("=== Master %d, Slave %d ===\n", mi, pos)
		if slave.Alias != 0 {
			fmt.Printf("Alias: %d\n", slave.Alias)
		}
		fmt.Printf("State: %s\n", stateString(slave.AlState))
		fmt.Printf("Flag: %s\n", errorFlagString(slave.ErrorFlag))
		fmt.Printf("Identity:\n")
		fmt.Printf("  Vendor Id:       0x%08x\n", slave.VendorId)
		fmt.Printf("  Product code:    0x%08x\n", slave.ProductCode)
		fmt.Printf("  Revision number: 0x%08x\n", slave.RevisionNumber)
		fmt.Printf("  Serial number:   0x%08x\n", slave.SerialNumber)

		fmt.Printf("DL information:\n")
		for i, p := range slave.Ports {
			linkStr := "down"
			if p.Link != 0 {
				linkStr = "up"
			}
			portType := portDescString(p.Desc)
			fmt.Printf("  Port  %d: %s, %s\n", i, portType, linkStr)
		}

		if slave.HasGeneralCategory {
			fmt.Printf("General:\n")
			fmt.Printf("  Group: %s\n", slave.Group)
			fmt.Printf("  Image name: %s\n", slave.Image)
			fmt.Printf("  Order number: %s\n", slave.Order)
			fmt.Printf("  Device name: %s\n", slave.Name)
		}

		if slave.MailboxProtocols != 0 {
			fmt.Printf("Mailbox:\n")
			fmt.Printf("  Supported protocols: %s\n", mailboxProtocols(slave.MailboxProtocols))
			if slave.StdRxMailboxSize > 0 {
				fmt.Printf("  Default RX mailbox: offset 0x%04x, size %d\n",
					slave.StdRxMailboxOffset, slave.StdRxMailboxSize)
			}
			if slave.StdTxMailboxSize > 0 {
				fmt.Printf("  Default TX mailbox: offset 0x%04x, size %d\n",
					slave.StdTxMailboxOffset, slave.StdTxMailboxSize)
			}
		}

		fmt.Println()
	}
	return nil
}

func stateString(state uint8) string {
	if state == 0 {
		return "UNKNOWN"
	}
	var s string
	switch state & 0x0F {
	case 0x01:
		s = "INIT"
	case 0x02:
		s = "PREOP"
	case 0x03:
		s = "BOOT"
	case 0x04:
		s = "SAFEOP"
	case 0x08:
		s = "OP"
	default:
		s = fmt.Sprintf("0x%02X", state&0x0F)
	}
	if state&0x10 != 0 {
		s += "+ERROR"
	}
	return s
}

func errorFlagString(flag bool) string {
	if flag {
		return "E"
	}
	return "-"
}

func portDescString(desc uint8) string {
	switch desc {
	case 0x00:
		return "Not implemented"
	case 0x01:
		return "Not configured"
	case 0x02:
		return "EBUS"
	case 0x03:
		return "MII"
	default:
		return fmt.Sprintf("0x%02x", desc)
	}
}

func mailboxProtocols(proto uint16) string {
	var parts []string
	if proto&0x0001 != 0 {
		parts = append(parts, "AoE")
	}
	if proto&0x0002 != 0 {
		parts = append(parts, "EoE")
	}
	if proto&0x0004 != 0 {
		parts = append(parts, "CoE")
	}
	if proto&0x0008 != 0 {
		parts = append(parts, "FoE")
	}
	if proto&0x0010 != 0 {
		parts = append(parts, "SoE")
	}
	if proto&0x0020 != 0 {
		parts = append(parts, "VoE")
	}
	return strings.Join(parts, ", ")
}
