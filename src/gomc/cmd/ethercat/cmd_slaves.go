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
	if len(args) > 0 {
		return fmt.Errorf("'slaves' takes no arguments!")
	}

	masterIndex := parseMasterIndex(opts.Masters)
	master, err := client.GetMaster(masterIndex)
	if err != nil {
		return err
	}

	if opts.Verbosity == Verbose {
		return slavesVerbose(client, masterIndex, master.SlaveCount, opts)
	}
	return slavesBrief(client, masterIndex, master.SlaveCount, opts)
}

func slavesBrief(client *EthercatClient, masterIndex *uint32, slaveCount uint32, opts *GlobalOpts) error {
	type slaveRow struct {
		pos      string
		alias    string
		relPos   string
		state    string
		flag     string
		name     string
		device   uint32
	}

	// Collect all slaves to compute alias tracking.
	var rows []slaveRow
	var lastAlias uint16
	var aliasIndex uint16

	positions := parsePositionList(opts.Positions)

	for i := uint16(0); i < uint16(slaveCount); i++ {
		slave, err := client.GetSlave(masterIndex, i)
		if err != nil {
			return err
		}

		if slave.Alias != 0 {
			lastAlias = slave.Alias
			aliasIndex = 0
		}

		// Check if this slave is in the selection.
		if positions != nil && !posInList(i, positions) {
			aliasIndex++
			continue
		}

		flag := "+"
		if slave.AlState == 0 {
			flag = " "
		}
		if slave.ErrorFlag {
			flag = "E"
		}

		name := slave.Name
		if name == "" {
			name = fmt.Sprintf("0x%08x:0x%08x", slave.VendorId, slave.ProductCode)
		}

		rows = append(rows, slaveRow{
			pos:    fmt.Sprintf("%d", i),
			alias:  fmt.Sprintf("%d", lastAlias),
			relPos: fmt.Sprintf("%d", aliasIndex),
			state:  alStateString(slave.AlState),
			flag:   flag,
			name:   name,
			device: slave.DeviceIndex,
		})

		aliasIndex++
	}

	// Calculate column widths.
	var maxPosWidth, maxAliasWidth, maxRelPosWidth, maxStateWidth int
	for _, r := range rows {
		if len(r.pos) > maxPosWidth {
			maxPosWidth = len(r.pos)
		}
		if len(r.alias) > maxAliasWidth {
			maxAliasWidth = len(r.alias)
		}
		if len(r.relPos) > maxRelPosWidth {
			maxRelPosWidth = len(r.relPos)
		}
		if len(r.state) > maxStateWidth {
			maxStateWidth = len(r.state)
		}
	}

	var lastDevice uint32
	for idx, r := range rows {
		if idx > 0 && r.device != lastDevice {
			fmt.Println("xxx LINK FAILURE xxx")
		}
		lastDevice = r.device
		fmt.Printf("%*s  %*s:%-*s  %-*s  %s  %s\n",
			maxPosWidth, r.pos,
			maxAliasWidth, r.alias,
			maxRelPosWidth, r.relPos,
			maxStateWidth, r.state,
			r.flag, r.name)
	}
	return nil
}

func slavesVerbose(client *EthercatClient, masterIndex *uint32, slaveCount uint32, opts *GlobalOpts) error {
	mi := masterIdx(masterIndex)
	positions := parsePositionList(opts.Positions)

	for i := uint16(0); i < uint16(slaveCount); i++ {
		if positions != nil && !posInList(i, positions) {
			continue
		}

		slave, err := client.GetSlave(masterIndex, i)
		if err != nil {
			return err
		}
		fmt.Printf("=== Master %d, Slave %d ===\n", mi, i)
		if slave.Alias != 0 {
			fmt.Printf("Alias: %d\n", slave.Alias)
		}
		fmt.Printf("Device: %s\n", func() string {
			if slave.DeviceIndex == 0 {
				return "Main"
			}
			return "Backup"
		}())
		fmt.Printf("State: %s\n", alStateString(slave.AlState))
		fmt.Printf("Flag: %s\n", func() string {
			if slave.ErrorFlag {
				return "E"
			}
			return "+"
		}())
		fmt.Printf("Identity:\n")
		fmt.Printf("  Vendor Id:       0x%08x\n", slave.VendorId)
		fmt.Printf("  Product code:    0x%08x\n", slave.ProductCode)
		fmt.Printf("  Revision number: 0x%08x\n", slave.RevisionNumber)
		fmt.Printf("  Serial number:   0x%08x\n", slave.SerialNumber)

		fmt.Printf("DL information:\n")
		fmt.Printf("  Distributed clocks: ")
		if slave.DcSupported {
			if slave.HasDcSystemTime {
				dcRange := "32 bit"
				if slave.DcRange == 1 {
					dcRange = "64 bit"
				}
				fmt.Printf("yes, %s\n", dcRange)
			} else {
				fmt.Printf("yes, delay measurement only\n")
			}
			fmt.Printf("  DC system time transmission delay: %d ns\n", slave.TransmissionDelay)
		} else {
			fmt.Printf("no\n")
		}

		fmt.Printf("Port  Type  Link  Loop    Signal  NextSlave")
		if slave.DcSupported {
			fmt.Printf("  RxTime [ns]  Diff [ns]   NextDc [ns]")
		}
		fmt.Println()

		for p := 0; p < 4; p++ {
			port := slave.Ports[p]
			fmt.Printf("   %d  %-4s", p, portDescStr(port.Desc))

			linkUp := port.Link&0x01 != 0
			loopClosed := port.Link&0x02 != 0
			signalDetected := port.Link&0x04 != 0

			fmt.Printf("  %-4s", boolUpDown(linkUp))
			fmt.Printf("  %-6s", func() string {
				if loopClosed {
					return "closed"
				}
				return "open"
			}())
			fmt.Printf("  %-6s", boolYesNo(signalDetected))
			fmt.Printf("  %9s", func() string {
				if port.NextSlave != 0xFFFF {
					return fmt.Sprintf("%d", port.NextSlave)
				}
				return "-"
			}())

			if slave.DcSupported {
				if !loopClosed {
					fmt.Printf("  %11d", port.ReceiveTime)
					diff := int64(port.ReceiveTime) - int64(slave.Ports[0].ReceiveTime)
					fmt.Printf("  %10d", diff)
					fmt.Printf("  %10d", port.DelayToNextDc)
				} else {
					fmt.Printf("  %11s  %10s  %10s", "-", "-", "-")
				}
			}
			fmt.Println()
		}

		if slave.MailboxProtocols != 0 {
			fmt.Printf("Mailboxes:\n")
			fmt.Printf("  Bootstrap RX: 0x%04x/%d, TX: 0x%04x/%d\n",
				slave.BootRxMailboxOffset, slave.BootRxMailboxSize,
				slave.BootTxMailboxOffset, slave.BootTxMailboxSize)
			fmt.Printf("  Standard  RX: 0x%04x/%d, TX: 0x%04x/%d\n",
				slave.StdRxMailboxOffset, slave.StdRxMailboxSize,
				slave.StdTxMailboxOffset, slave.StdTxMailboxSize)
			fmt.Printf("  Supported protocols: %s\n", mailboxProtocols(slave.MailboxProtocols))
		}

		if slave.HasGeneralCategory {
			fmt.Printf("General:\n")
			fmt.Printf("  Group: %s\n", slave.Group)
			fmt.Printf("  Image name: %s\n", slave.Image)
			fmt.Printf("  Order number: %s\n", slave.Order)
			fmt.Printf("  Device name: %s\n", slave.Name)
			if slave.CurrentOnEbus != 0 {
				fmt.Printf("  Current consumption: %d mA\n", slave.CurrentOnEbus)
			}
		}

		fmt.Println()
	}
	return nil
}

func posInList(pos uint16, positions []uint16) bool {
	for _, p := range positions {
		if p == pos {
			return true
		}
	}
	return false
}

func boolYesNo(b bool) string {
	if b {
		return "yes"
	}
	return "no"
}

func boolUpDown(b bool) string {
	if b {
		return "up"
	}
	return "down"
}

func portDescStr(desc uint8) string {
	switch desc {
	case 0x00:
		return "N/A"
	case 0x01:
		return "N/C"
	case 0x02:
		return "EBUS"
	case 0x03:
		return "MII"
	default:
		return "???"
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
