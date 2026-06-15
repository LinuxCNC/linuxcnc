package main

import (
	"fmt"
)

func init() {
	registerCommand(&Command{
		Name:  "master",
		Brief: "Show master and Ethernet device information.",
		Run:   cmdMaster,
	})
}

func phaseString(phase uint8) string {
	switch phase {
	case 0:
		return "Idle"
	case 1:
		return "Operation"
	default:
		return fmt.Sprintf("Unknown (0x%02x)", phase)
	}
}

func cmdMaster(client *EthercatClient, opts *GlobalOpts, args []string) error {
	masterIndex := parseMasterIndex(opts.Masters)
	info, err := client.GetMaster(masterIndex)
	if err != nil {
		return err
	}

	mi := masterIdx(masterIndex)
	fmt.Printf("Master%d\n", mi)
	fmt.Printf("  Phase: %s\n", phaseString(info.Phase))
	fmt.Printf("  Active: %v\n", info.Active)
	fmt.Printf("  Slaves: %d\n", info.SlaveCount)
	fmt.Printf("  Ethernet devices:\n")

	for i, dev := range info.Devices {
		linkStr := "DOWN"
		if dev.LinkState {
			linkStr = "UP"
		}
		fmt.Printf("    %s (device %d):\n", dev.Address, i)
		fmt.Printf("      Link: %s\n", linkStr)
		fmt.Printf("      Tx frames:   %10d\n", dev.TxCount)
		fmt.Printf("      Tx bytes:    %10d\n", dev.TxBytes)
		fmt.Printf("      Rx frames:   %10d\n", dev.RxCount)
		fmt.Printf("      Rx bytes:    %10d\n", dev.RxBytes)
		fmt.Printf("      Tx errors:   %10d\n", dev.TxErrors)
		fmt.Printf("      Tx frame rate [1/s]: %6d %6d %6d\n",
			dev.TxFrameRates[0], dev.TxFrameRates[1], dev.TxFrameRates[2])
		fmt.Printf("      Tx rate [KByte/s]:   %6d %6d %6d\n",
			dev.TxByteRates[0]/1024, dev.TxByteRates[1]/1024, dev.TxByteRates[2]/1024)
		fmt.Printf("      Rx frame rate [1/s]: %6d %6d %6d\n",
			dev.RxFrameRates[0], dev.RxFrameRates[1], dev.RxFrameRates[2])
		fmt.Printf("      Rx rate [KByte/s]:   %6d %6d %6d\n",
			dev.RxByteRates[0]/1024, dev.RxByteRates[1]/1024, dev.RxByteRates[2]/1024)
	}

	fmt.Printf("  Distributed clocks:\n")
	if info.RefClock != 0xFFFF {
		fmt.Printf("    Reference clock: Slave %d\n", info.RefClock)
	} else {
		fmt.Printf("    Reference clock: (not available)\n")
	}
	if info.AppTime > 0 {
		fmt.Printf("    Application time: %d\n", info.AppTime)
	}

	return nil
}
