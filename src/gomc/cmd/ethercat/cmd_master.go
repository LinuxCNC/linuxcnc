// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package main

import (
	"fmt"
	"time"
)

func init() {
	registerCommand(&Command{
		Name:  "master",
		Brief: "Show master and Ethernet device information.",
		Run:   cmdMaster,
	})
}

func cmdMaster(client *EthercatClient, opts *GlobalOpts, args []string) error {
	if len(args) > 0 {
		return fmt.Errorf("'master' takes no arguments")
	}

	masterIndex := parseMasterIndex(opts.Masters)
	info, err := client.GetMaster(masterIndex)
	if err != nil {
		return err
	}

	mi := masterIdx(masterIndex)
	fmt.Printf("Master%d\n", mi)

	fmt.Printf("  Phase: ")
	switch info.Phase {
	case 0:
		fmt.Printf("Waiting for device(s)...")
	case 1:
		fmt.Printf("Idle")
	case 2:
		fmt.Printf("Operation")
	default:
		fmt.Printf("???")
	}
	fmt.Println()

	activeStr := "no"
	if info.Active {
		activeStr = "yes"
	}
	fmt.Printf("  Active: %s\n", activeStr)
	fmt.Printf("  Slaves: %d\n", info.SlaveCount)
	fmt.Printf("  Ethernet devices:\n")

	for i, dev := range info.Devices {
		label := "Main"
		if i > 0 {
			label = "Backup"
		}
		attachedStr := "waiting..."
		if dev.Attached {
			attachedStr = "attached"
		}
		linkStr := "DOWN"
		if dev.LinkState {
			linkStr = "UP"
		}
		fmt.Printf("    %s: %s (%s)\n", label, dev.Address, attachedStr)
		fmt.Printf("      Link: %s\n", linkStr)
		fmt.Printf("      Tx frames:   %d\n", dev.TxCount)
		fmt.Printf("      Tx bytes:    %d\n", dev.TxBytes)
		fmt.Printf("      Rx frames:   %d\n", dev.RxCount)
		fmt.Printf("      Rx bytes:    %d\n", dev.RxBytes)
		fmt.Printf("      Tx errors:   %d\n", dev.TxErrors)
		fmt.Printf("      Tx frame rate [1/s]: ")
		for j := 0; j < 3; j++ {
			if j > 0 {
				fmt.Printf(" ")
			}
			fmt.Printf("%6.0f", float64(dev.TxFrameRates[j])/1000.0)
		}
		fmt.Println()
		fmt.Printf("      Tx rate [KByte/s]:   ")
		for j := 0; j < 3; j++ {
			if j > 0 {
				fmt.Printf(" ")
			}
			fmt.Printf("%6.1f", float64(dev.TxByteRates[j])/1024.0)
		}
		fmt.Println()
		fmt.Printf("      Rx frame rate [1/s]: ")
		for j := 0; j < 3; j++ {
			if j > 0 {
				fmt.Printf(" ")
			}
			fmt.Printf("%6.0f", float64(dev.RxFrameRates[j])/1000.0)
		}
		fmt.Println()
		fmt.Printf("      Rx rate [KByte/s]:   ")
		for j := 0; j < 3; j++ {
			if j > 0 {
				fmt.Printf(" ")
			}
			fmt.Printf("%6.1f", float64(dev.RxByteRates[j])/1024.0)
		}
		fmt.Println()
	}

	// Common section.
	lost := info.TxCount - info.RxCount
	if lost == 1 {
		// allow one frame travelling
		lost = 0
	}
	fmt.Printf("    Common:\n")
	fmt.Printf("      Tx frames:   %d\n", info.TxCount)
	fmt.Printf("      Tx bytes:    %d\n", info.TxBytes)
	fmt.Printf("      Rx frames:   %d\n", info.RxCount)
	fmt.Printf("      Rx bytes:    %d\n", info.RxBytes)
	fmt.Printf("      Lost frames: %d\n", lost)
	fmt.Printf("      Tx frame rate [1/s]: ")
	for j := 0; j < 3; j++ {
		if j > 0 {
			fmt.Printf(" ")
		}
		fmt.Printf("%6.0f", float64(info.TxFrameRates[j])/1000.0)
	}
	fmt.Println()
	fmt.Printf("      Tx rate [KByte/s]:   ")
	for j := 0; j < 3; j++ {
		if j > 0 {
			fmt.Printf(" ")
		}
		fmt.Printf("%6.1f", float64(info.TxByteRates[j])/1024.0)
	}
	fmt.Println()
	fmt.Printf("      Rx frame rate [1/s]: ")
	for j := 0; j < 3; j++ {
		if j > 0 {
			fmt.Printf(" ")
		}
		fmt.Printf("%6.0f", float64(info.RxFrameRates[j])/1000.0)
	}
	fmt.Println()
	fmt.Printf("      Rx rate [KByte/s]:   ")
	for j := 0; j < 3; j++ {
		if j > 0 {
			fmt.Printf(" ")
		}
		fmt.Printf("%6.1f", float64(info.RxByteRates[j])/1024.0)
	}
	fmt.Println()
	fmt.Printf("      Loss rate [1/s]:     ")
	for j := 0; j < 3; j++ {
		if j > 0 {
			fmt.Printf(" ")
		}
		fmt.Printf("%6.0f", float64(info.LossRates[j])/1000.0)
	}
	fmt.Println()
	fmt.Printf("      Frame loss [%%]:      ")
	for j := 0; j < 3; j++ {
		if j > 0 {
			fmt.Printf(" ")
		}
		var perc float64
		if info.TxFrameRates[j] != 0 {
			perc = 100.0 * float64(info.LossRates[j]) / float64(info.TxFrameRates[j])
		}
		fmt.Printf("%6.1f", perc)
	}
	fmt.Println()

	// Distributed clocks.
	fmt.Printf("  Distributed clocks:\n")
	fmt.Printf("    Reference clock:   ")
	if info.RefClock != 0xFFFF {
		fmt.Printf("Slave %d", info.RefClock)
	} else {
		fmt.Printf("None")
	}
	fmt.Println()
	fmt.Printf("    DC reference time: %d\n", info.DcRefTime)
	fmt.Printf("    Application time:  %d\n", info.AppTime)

	if info.AppTime > 0 {
		// EtherCAT epoch is 2000-01-01, app_time is in nanoseconds since then.
		epoch := int64(info.AppTime)/1000000000 + 946684800
		t := time.Unix(epoch, int64(info.AppTime)%1000000000)
		fmt.Printf("                       %s.%09d\n",
			t.UTC().Format("2006-01-02 15:04:05"),
			info.AppTime%1000000000)
	}

	return nil
}
