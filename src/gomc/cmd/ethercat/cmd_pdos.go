package main

import (
	"fmt"
)

func init() {
	registerCommand(&Command{
		Name:  "pdos",
		Brief: "List Sync Manager PDO assignment.",
		Run:   cmdPdos,
	})
}

func cmdPdos(client *EthercatClient, opts *GlobalOpts, args []string) error {
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
		for smIdx := uint32(0); smIdx < uint32(slave.SyncCount); smIdx++ {
			sm, err := client.GetSlaveSync(masterIndex, pos, smIdx)
			if err != nil {
				continue
			}
			dirStr := ""
			switch sm.ControlRegister & 0x0C {
			case 0x04:
				dirStr = " (Inputs)"
			case 0x08:
				dirStr = " (Outputs)"
			}
			enableStr := uint8(0)
			if sm.Enable {
				enableStr = 1
			}
			fmt.Printf("SM%d: PhysAddr 0x%04x, DefaultSize %d, ControlRegister 0x%02x%s, Enable %d\n",
				smIdx, sm.PhysicalStartAddress, sm.DefaultSize, sm.ControlRegister, dirStr, enableStr)

			for pdoIdx := uint32(0); pdoIdx < uint32(sm.PdoCount); pdoIdx++ {
				pdo, err := client.GetSlaveSyncPdo(masterIndex, pos, smIdx, pdoIdx)
				if err != nil {
					continue
				}
				pdoType := "TxPDO"
				if sm.ControlRegister&0x0C == 0x08 {
					pdoType = "RxPDO"
				}
				fmt.Printf("  %s 0x%04x \"%s\"\n", pdoType, pdo.Index, pdo.Name)

				for entryIdx := uint32(0); entryIdx < uint32(pdo.EntryCount); entryIdx++ {
					entry, err := client.GetSlaveSyncPdoEntry(masterIndex, pos, smIdx, pdoIdx, entryIdx)
					if err != nil {
						continue
					}
					fmt.Printf("    PDO entry 0x%04x:%02x, %2d bit, \"%s\"\n",
						entry.Index, entry.Subindex, entry.BitLength, entry.Name)
				}
			}
		}
	}
	return nil
}
