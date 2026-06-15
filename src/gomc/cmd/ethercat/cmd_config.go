package main

import (
	"fmt"
)

func init() {
	registerCommand(&Command{
		Name:  "config",
		Brief: "Show slave configurations.",
		Run:   cmdConfig,
	})
}

func cmdConfig(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) > 0 {
		return fmt.Errorf("'config' takes no arguments!")
	}

	masterIndex := parseMasterIndex(opts.Masters)
	master, err := client.GetMaster(masterIndex)
	if err != nil {
		return err
	}

	if opts.Verbosity == Verbose {
		return configVerbose(client, masterIndex, master.ConfigCount)
	}
	return configBrief(client, masterIndex, master.ConfigCount)
}

func configBrief(client *EthercatClient, masterIndex *uint32, count uint32) error {
	type configRow struct {
		alias    string
		pos      string
		ident    string
		slavePos string
		state    string
	}

	var rows []configRow
	var maxAliasWidth, maxPosWidth, maxSlavePosWidth, maxStateWidth int

	for i := uint32(0); i < count; i++ {
		cfg, err := client.GetConfig(masterIndex, i)
		if err != nil {
			return err
		}

		row := configRow{
			alias: fmt.Sprintf("%d", cfg.Alias),
			pos:   fmt.Sprintf("%d", cfg.Position),
			ident: fmt.Sprintf("0x%08x/0x%08x", cfg.VendorId, cfg.ProductCode),
		}

		if cfg.SlavePosition >= 0 {
			row.slavePos = fmt.Sprintf("%d", cfg.SlavePosition)
			// Need to get the slave's AL state.
			slave, err := client.GetSlave(masterIndex, uint16(cfg.SlavePosition))
			if err == nil {
				row.state = alStateString(slave.AlState)
			} else {
				row.state = "?"
			}
		} else {
			row.slavePos = "-"
			row.state = "-"
		}

		if len(row.alias) > maxAliasWidth {
			maxAliasWidth = len(row.alias)
		}
		if len(row.pos) > maxPosWidth {
			maxPosWidth = len(row.pos)
		}
		if len(row.slavePos) > maxSlavePosWidth {
			maxSlavePosWidth = len(row.slavePos)
		}
		if len(row.state) > maxStateWidth {
			maxStateWidth = len(row.state)
		}

		rows = append(rows, row)
	}

	for _, r := range rows {
		fmt.Printf("%*s:%-*s  %s  %-*s  %-*s  \n",
			maxAliasWidth, r.alias,
			maxPosWidth, r.pos,
			r.ident,
			maxSlavePosWidth, r.slavePos,
			maxStateWidth, r.state)
	}
	return nil
}

func configVerbose(client *EthercatClient, masterIndex *uint32, count uint32) error {
	for i := uint32(0); i < count; i++ {
		cfg, err := client.GetConfig(masterIndex, i)
		if err != nil {
			return err
		}
		fmt.Printf("Alias: %d\n", cfg.Alias)
		fmt.Printf("Position: %d\n", cfg.Position)
		fmt.Printf("Vendor Id: 0x%08x\n", cfg.VendorId)
		fmt.Printf("Product code: 0x%08x\n", cfg.ProductCode)

		fmt.Printf("Attached slave: ")
		if cfg.SlavePosition >= 0 {
			slave, err := client.GetSlave(masterIndex, uint16(cfg.SlavePosition))
			if err == nil {
				fmt.Printf("%d (%s)\n", cfg.SlavePosition, alStateString(slave.AlState))
			} else {
				fmt.Printf("%d\n", cfg.SlavePosition)
			}
		} else {
			fmt.Printf("none\n")
		}

		fmt.Printf("Watchdog divider: ")
		if cfg.WatchdogDivider != 0 {
			fmt.Printf("%d", cfg.WatchdogDivider)
		} else {
			fmt.Printf("(Default)")
		}
		fmt.Println()
		fmt.Printf("Watchdog intervals: ")
		if cfg.WatchdogIntervals != 0 {
			fmt.Printf("%d", cfg.WatchdogIntervals)
		} else {
			fmt.Printf("(Default)")
		}
		fmt.Println()

		// Sync managers with PDOs.
		for smIdx := uint8(0); smIdx < 16; smIdx++ {
			sm := cfg.Syncs[smIdx]
			if sm.PdoCount == 0 {
				continue
			}
			dirStr := "Invalid"
			switch sm.Dir {
			case 1:
				dirStr = "Input"
			case 2:
				dirStr = "Output"
			}
			wdStr := "Default"
			switch sm.WatchdogMode {
			case 1:
				wdStr = "Enable"
			case 2:
				wdStr = "Disable"
			}
			fmt.Printf("SM%d, Dir: %s, Watchdog: %s\n", smIdx, dirStr, wdStr)

			for pdoIdx := uint16(0); pdoIdx < uint16(sm.PdoCount); pdoIdx++ {
				pdo, err := client.GetConfigPdo(masterIndex, i, smIdx, pdoIdx)
				if err != nil {
					continue
				}
				fmt.Printf("  PDO 0x%04x\n", pdo.Index)
				for entryIdx := uint8(0); entryIdx < pdo.EntryCount; entryIdx++ {
					entry, err := client.GetConfigPdoEntry(masterIndex, i, smIdx, pdoIdx, entryIdx)
					if err != nil {
						continue
					}
					fmt.Printf("    PDO entry 0x%04x:%02x, %2d bit\n",
						entry.Index, entry.Subindex, entry.BitLength)
				}
			}
		}

		// SDO configuration.
		fmt.Printf("SDO configuration:\n")
		if cfg.SdoCount > 0 {
			for sdoIdx := uint32(0); sdoIdx < cfg.SdoCount; sdoIdx++ {
				sdo, err := client.GetConfigSdo(masterIndex, i, sdoIdx)
				if err != nil {
					continue
				}
				if sdo.CompleteAccess {
					fmt.Printf("  0x%04x C, %d byte\n", sdo.Index, sdo.Size)
				} else {
					fmt.Printf("  0x%04x:%02x, %d byte\n", sdo.Index, sdo.Subindex, sdo.Size)
				}
				// Print data hex dump if available.
				if len(sdo.Data) > 0 {
					printConfigDataHex("    ", sdo.Data)
				}
			}
		} else {
			fmt.Printf("  None.\n")
		}

		// IDN configuration.
		fmt.Printf("IDN configuration:\n")
		if cfg.IdnCount > 0 {
			for idnIdx := uint32(0); idnIdx < cfg.IdnCount; idnIdx++ {
				idn, err := client.GetConfigIdn(masterIndex, i, idnIdx)
				if err != nil {
					continue
				}
				fmt.Printf("  Drive %d, %s, %d byte\n",
					idn.DriveNo, outputIdn(idn.Idn), idn.Size)
				if len(idn.Data) > 0 {
					printConfigDataHex("    ", idn.Data)
				}
			}
		} else {
			fmt.Printf("  None.\n")
		}

		// Feature flags.
		fmt.Printf("Feature flags:\n")
		if cfg.FlagCount > 0 {
			for flagIdx := uint32(0); flagIdx < cfg.FlagCount; flagIdx++ {
				flag, err := client.GetConfigFlag(masterIndex, i, flagIdx)
				if err != nil {
					continue
				}
				fmt.Printf("  %s: %d\n", flag.Key, flag.Value)
			}
		} else {
			fmt.Printf("  None.\n")
		}

		// DC configuration.
		if cfg.DcAssignActivate != 0 {
			fmt.Printf("DC configuration:\n")
			fmt.Printf("  AssignActivate: 0x%04x\n", cfg.DcAssignActivate)
			fmt.Printf("         Cycle [ns]   Shift [ns]\n")
			for s := 0; s < 2; s++ {
				fmt.Printf("  SYNC%d  %11d  %11d\n",
					s, cfg.DcSync[s].CycleTime, cfg.DcSync[s].ShiftTime)
			}
		}

		fmt.Println()
	}
	return nil
}

// outputIdn formats an IDN like the IgH tool: S-x-yyyy or P-x-yyyy.
func outputIdn(idn uint16) string {
	if idn&0x8000 != 0 {
		return fmt.Sprintf("P-%d-%04d", (idn>>12)&0x07, idn&0x0FFF)
	}
	return fmt.Sprintf("S-%d-%04d", (idn>>12)&0x07, idn&0x0FFF)
}

// printConfigDataHex prints binary data as hex, 16 bytes per line.
func printConfigDataHex(indent string, data []byte) {
	fmt.Printf("%s", indent)
	for i, b := range data {
		fmt.Printf("%02x ", b)
		if (i+1)%16 == 0 && i < len(data)-1 {
			fmt.Printf("\n%s", indent)
		}
	}
	fmt.Println()
}

// alStateString converts AL state byte to string (matches IgH tool).
func alStateString(state uint8) string {
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
