package main

import (
	"fmt"
	"log"
	"time"

	"linuxcnc.org/hal"
)

func main() {
	comp, err := hal.NewComponent("str-receiver")
	if err != nil {
		log.Fatalf("Failed to create component: %v", err)
	}
	defer comp.Exit()

	inPin, err := hal.NewPin[string](comp, "in", hal.In)
	if err != nil {
		log.Fatalf("Failed to create in pin: %v", err)
	}

	if err := comp.Ready(); err != nil {
		log.Fatalf("Failed to mark ready: %v", err)
	}
	log.Println("str-receiver ready")

	// Poll for incoming string
	for comp.Running() {
		msg := inPin.Get()
		if msg != "" {
			// Print in a machine-parseable format for the test script
			fmt.Printf("RECEIVED:%s\n", msg)
			// Keep running so halcmd can unload us cleanly
			// but stop polling — we got what we need
			for comp.Running() {
				time.Sleep(50 * time.Millisecond)
			}
			break
		}
		time.Sleep(10 * time.Millisecond)
	}

	log.Println("str-receiver exiting")
}
