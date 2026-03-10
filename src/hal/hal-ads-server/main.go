// hal-ads-server - LinuxCNC HAL component implementing a Beckhoff ADS/TwinCAT protocol server.
//
// It bridges ADS symbol read/write requests from TwinCAT HMI clients to
// LinuxCNC HAL pins.  A config file describes the symbol tree and HAL pin
// mapping; at startup the server creates all HAL pins, registers them as ADS
// symbols, and starts listening for TCP connections.
//
// Usage:
//
//	hal-ads-server [options] <config-file>
//
//	-bind string         IP address to bind the ADS server to (default "0.0.0.0")
//	-name string        HAL component name (default "hal-ads-server")
//	-port int           TCP port for ADS server (default 48898)
//	-ams-net-id string  Local AMS Net ID (default "192.168.0.99.1.1")
//	-verbose            Enable verbose debug logging
//	-xml                Generate PLCopen TC6 XML from config file and exit
package main

import (
	"flag"
	"log"
	"os"
	"os/signal"
	"strconv"
	"syscall"
	"time"

	"linuxcnc.org/hal"

	"linuxcnc.org/hal-ads-server/ads"
)

// DefaultAMSPort is the standard AMS port for TwinCAT3 PLC runtime 1.
// This is the logical ADS port used in AMS headers, distinct from the TCP
// listen port (-port flag, default 48898).
const DefaultAMSPort uint16 = 851

func main() {
	// Parse command-line flags.
	name := flag.String("name", "hal-ads-server", "HAL component name")
	port := flag.Int("port", 48898, "TCP port for the ADS server")
	bind := flag.String("bind", "0.0.0.0", "IP address to bind the ADS server to")
	amsNetIDStr := flag.String("ams-net-id", "192.168.0.99.1.1", "Local AMS Net ID")
	verbose := flag.Bool("verbose", false, "Enable verbose debug logging")
	xmlMode := flag.Bool("xml", false, "Generate PLCopen TC6 XML from config file and write to stdout, then exit")
	flag.Parse()

	if flag.NArg() < 1 {
		log.Fatalf("Usage: %s [options] <config-file>", os.Args[0])
	}
	configFile := flag.Arg(0)

	// Load config file.
	f, err := os.Open(configFile)
	if err != nil {
		log.Fatalf("Cannot open config file %q: %v", configFile, err)
	}
	defer f.Close()
	aliases, tree, err := ParseTreeWithAliases(f)
	if err != nil {
		log.Fatalf("Config parse error: %v", err)
	}

	// -xml mode: generate PLCopen TC6 XML and exit without starting the server.
	if *xmlMode {
		if err := GenerateXML(os.Stdout, tree, aliases); err != nil {
			log.Fatalf("XML generation error: %v", err)
		}
		return
	}

	// Parse AMS Net ID.
	amsNetID, err := ads.ParseAMSNetID(*amsNetIDStr)
	if err != nil {
		log.Fatalf("Invalid AMS Net ID: %v", err)
	}

	layoutPins, err := ComputeLayout(tree, aliases)
	if err != nil {
		log.Fatalf("Layout error: %v", err)
	}

	if len(layoutPins) == 0 {
		log.Fatalf("Config file %q defines no pins", configFile)
	}
	log.Printf("Loaded %d symbol(s) from %s", len(layoutPins), configFile)

	// Create HAL component.
	comp, err := hal.NewComponent(*name)
	if err != nil {
		log.Fatalf("Failed to create HAL component %q: %v", *name, err)
	}
	defer comp.Exit()

	// Build symbol table and HAL pins.
	st := ads.NewSymbolTable()
	if _, err := NewBridge(comp, layoutPins, st, aliases); err != nil {
		log.Fatalf("Failed to create HAL pins: %v", err)
	}
	// Propagate @struct type names and GUIDs to container group symbols so that
	// ADSIGRP_SYM_INFOBYNAMEEX responses include the correct TwinCAT metadata.
	applyContainerTypeInfo(tree, "", st, aliases)

	// Mark component ready (allows 'loadusr -W' to proceed).
	if err := comp.Ready(); err != nil {
		log.Fatalf("Failed to mark component ready: %v", err)
	}
	log.Printf("HAL component %q ready with %d pin(s)", *name, len(layoutPins))

	// Start ADS TCP server.
	addr := *bind + ":" + strconv.Itoa(*port)
	srv := ads.NewServer(addr, amsNetID, DefaultAMSPort, st, *verbose)
	if err := srv.Start(); err != nil {
		log.Fatalf("Failed to start ADS server: %v", err)
	}
	defer srv.Stop()

	// Wait for HAL shutdown or OS signal.
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)

	done := make(chan struct{})
	go func() {
		for comp.Running() {
			time.Sleep(100 * time.Millisecond)
		}
		close(done)
	}()

	select {
	case <-done:
	case sig := <-sigCh:
		log.Printf("Received signal: %v", sig)
	}

	log.Printf("Shutting down %s", *name)
}
