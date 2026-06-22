// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package main

import (
	"fmt"
	"os"
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

	domains := parseDomainList(opts.Domains)

	for i := uint32(0); i < master.DomainCount; i++ {
		if domains != nil && !containsU32(domains, i) {
			continue
		}
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

	domains := parseDomainList(opts.Domains)

	for i := uint32(0); i < master.DomainCount; i++ {
		if domains != nil && !containsU32(domains, i) {
			continue
		}
		dataStr, err := client.GetDomainData(masterIndex, i)
		if err != nil {
			return err
		}
		os.Stdout.Write(dataStr)
	}
	return nil
}

func parseDomainList(s string) []uint32 {
	if s == "" {
		return nil
	}
	var result []uint32
	for _, part := range splitFields(s) {
		v, err := parseUint32(part)
		if err == nil {
			result = append(result, v)
		}
	}
	return result
}

func containsU32(list []uint32, v uint32) bool {
	for _, x := range list {
		if x == v {
			return true
		}
	}
	return false
}

func parseUint32(s string) (uint32, error) {
	v, err := fmt.Sscanf(s, "%d", new(uint32))
	if err != nil || v == 0 {
		return 0, fmt.Errorf("invalid")
	}
	var val uint32
	fmt.Sscanf(s, "%d", &val)
	return val, nil
}
