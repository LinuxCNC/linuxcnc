package main

import (
	"fmt"
)

func init() {
	registerCommand(&Command{
		Name:  "domains",
		Brief: "Show configured domains.",
		Run:   cmdDomains,
	})
	registerCommand(&Command{
		Name:  "data",
		Brief: "Output binary domain process data.",
		Run:   cmdData,
	})
}

func cmdDomains(client *EthercatClient, opts *GlobalOpts, args []string) error {
	masterIndex := parseMasterIndex(opts.Masters)
	master, err := client.GetMaster(masterIndex)
	if err != nil {
		return err
	}

	for i := uint32(0); i < master.DomainCount; i++ {
		dom, err := client.GetDomain(masterIndex, i)
		if err != nil {
			return err
		}
		fmt.Printf("Domain%d: LogBaseAddr 0x%08x, Size %3d, WorkingCounter %d/%d\n",
			i, dom.LogicalBaseAddress, dom.DataSize,
			dom.WorkingCounter[0], dom.ExpectedWorkingCounter)

		if opts.Verbosity == Verbose {
			for j := uint32(0); j < dom.FmmuCount; j++ {
				fmmu, err := client.GetDomainFmmu(masterIndex, i, j)
				if err != nil {
					return err
				}
				dirStr := "Input"
				if fmmu.Dir == 1 {
					dirStr = "Output"
				}
				fmt.Printf("  SlaveConfig %d:%d, SM%d, LogAddr 0x%08x, Size %d, %s\n",
					fmmu.SlaveConfigAlias, fmmu.SlaveConfigPosition,
					fmmu.SyncIndex, fmmu.LogicalAddress, fmmu.DataSize, dirStr)
			}
		}
	}
	return nil
}

func cmdData(client *EthercatClient, opts *GlobalOpts, args []string) error {
	masterIndex := parseMasterIndex(opts.Masters)
	master, err := client.GetMaster(masterIndex)
	if err != nil {
		return err
	}

	for i := uint32(0); i < master.DomainCount; i++ {
		data, err := client.GetDomainData(masterIndex, i)
		if err != nil {
			return err
		}
		fmt.Print(data)
	}
	return nil
}
