// Command ads-xml-gen generates a TwinCAT-compatible XML symbol table from
// an ADS .conf file. The XML is written to stdout.
//
// Usage:
//
//	ads-xml-gen <config.conf>
package main

import (
	"fmt"
	"os"

	"github.com/sittner/linuxcnc/src/launcher/pkg/adsconfig"
)

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintf(os.Stderr, "usage: %s <config.conf>\n", os.Args[0])
		os.Exit(1)
	}

	_, aliases, tree, err := adsconfig.ParseConfFile(os.Args[1])
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		os.Exit(1)
	}

	if err := adsconfig.GenerateXML(os.Stdout, tree, aliases); err != nil {
		fmt.Fprintf(os.Stderr, "error generating XML: %v\n", err)
		os.Exit(1)
	}
}
