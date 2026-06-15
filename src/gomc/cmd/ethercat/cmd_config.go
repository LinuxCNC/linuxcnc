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
	for i := uint32(0); i < count; i++ {
		cfg, err := client.GetConfig(masterIndex, i)
		if err != nil {
			return err
		}
		attachStr := "-"
		if cfg.SlavePosition >= 0 {
			attachStr = fmt.Sprintf("%d", cfg.SlavePosition)
		}
		fmt.Printf("%d:%d  0x%08x/0x%08x  %s\n",
			cfg.Alias, cfg.Position, cfg.VendorId, cfg.ProductCode, attachStr)
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

		attachStr := "none"
		if cfg.SlavePosition >= 0 {
			attachStr = fmt.Sprintf("Slave %d", cfg.SlavePosition)
		}
		fmt.Printf("Attached: %s\n", attachStr)

		// Sync managers.
		for smIdx := uint8(0); smIdx < 16; smIdx++ {
			sm := cfg.Syncs[smIdx]
			if sm.PdoCount == 0 && !sm.ConfigThis {
				continue
			}
			dirStr := "Invalid"
			switch sm.Dir {
			case 1:
				dirStr = "Output"
			case 2:
				dirStr = "Input"
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
					fmt.Printf("    PDO entry 0x%04x:%02x, %d bit\n",
						entry.Index, entry.Subindex, entry.BitLength)
				}
			}
		}

		// SDO configs.
		for sdoIdx := uint32(0); sdoIdx < cfg.SdoCount; sdoIdx++ {
			sdo, err := client.GetConfigSdo(masterIndex, i, sdoIdx)
			if err != nil {
				continue
			}
			fmt.Printf("  SDO 0x%04x:%02x, %d byte(s)\n",
				sdo.Index, sdo.Subindex, sdo.Size)
		}

		// IDN configs.
		for idnIdx := uint32(0); idnIdx < cfg.IdnCount; idnIdx++ {
			idn, err := client.GetConfigIdn(masterIndex, i, idnIdx)
			if err != nil {
				continue
			}
			fmt.Printf("  IDN: drive %d, idn %d, %d byte(s)\n",
				idn.DriveNo, idn.Idn, idn.Size)
		}

		fmt.Println()
	}
	return nil
}
