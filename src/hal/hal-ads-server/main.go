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
//	-name string        HAL component name (default "hal-ads-server")
//	-port int           TCP port for ADS server (default 48898)
//	-ams-net-id string  Local AMS Net ID (default "5.80.201.232.1.1")
//	-verbose            Enable verbose debug logging
package main

import (
	"flag"
	"log"
	"os"
	"strconv"
	"time"

	"linuxcnc.org/hal"

	"linuxcnc.org/hal-ads-server/ads"
)

func main() {
	// Parse command-line flags.
	name := flag.String("name", "hal-ads-server", "HAL component name")
	port := flag.Int("port", 48898, "TCP port for the ADS server")
	amsNetIDStr := flag.String("ams-net-id", "5.80.201.232.1.1", "Local AMS Net ID")
	verbose := flag.Bool("verbose", false, "Enable verbose debug logging")
	flag.Parse()

	if flag.NArg() < 1 {
		log.Fatalf("Usage: %s [options] <config-file>", os.Args[0])
	}
	configFile := flag.Arg(0)

	// Parse AMS Net ID.
	amsNetID, err := ads.ParseAMSNetID(*amsNetIDStr)
	if err != nil {
		log.Fatalf("Invalid AMS Net ID: %v", err)
	}

	// Load config file.
	f, err := os.Open(configFile)
	if err != nil {
		log.Fatalf("Cannot open config file %q: %v", configFile, err)
	}
	configPins, err := ParseConfig(f)
	f.Close()
	if err != nil {
		log.Fatalf("Config parse error: %v", err)
	}

	if len(configPins) == 0 {
		log.Fatalf("Config file %q defines no pins", configFile)
	}
	log.Printf("Loaded %d symbol(s) from %s", len(configPins), configFile)

	// Create HAL component.
	comp, err := hal.NewComponent(*name)
	if err != nil {
		log.Fatalf("Failed to create HAL component %q: %v", *name, err)
	}
	defer comp.Exit()

	// Build symbol table and HAL pins.
	st := ads.NewSymbolTable()
	if _, err := NewBridge(comp, configPins, st); err != nil {
		log.Fatalf("Failed to create HAL pins: %v", err)
	}

	// Mark component ready (allows 'loadusr -W' to proceed).
	if err := comp.Ready(); err != nil {
		log.Fatalf("Failed to mark component ready: %v", err)
	}
	log.Printf("HAL component %q ready with %d pin(s)", *name, len(configPins))

	// Start ADS TCP server.
	addr := ":" + strconv.Itoa(*port)
	srv := ads.NewServer(addr, amsNetID, 851, st, *verbose)
	if err := srv.Start(); err != nil {
		log.Fatalf("Failed to start ADS server: %v", err)
	}

	// Main loop: keep the component alive until shutdown signal.
	for comp.Running() {
		time.Sleep(100 * time.Millisecond)
	}

	log.Printf("Shutting down %s", *name)
	srv.Stop()
}
