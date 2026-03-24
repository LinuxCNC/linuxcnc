package main

import (
	"log"
	"time"

	"github.com/sittner/linuxcnc/src/launcher/pkg/hal"
)

func main() {
	comp, err := hal.NewComponent("str-sender")
	if err != nil {
		log.Fatalf("Failed to create component: %v", err)
	}
	defer comp.Exit()

	outPin, err := hal.NewPin[string](comp, "out", hal.Out)
	if err != nil {
		log.Fatalf("Failed to create out pin: %v", err)
	}

	if err := comp.Ready(); err != nil {
		log.Fatalf("Failed to mark ready: %v", err)
	}
	log.Println("str-sender ready")

	// Write the test string continuously so receiver can pick it up
	for comp.Running() {
		outPin.Set("hello from go")
		time.Sleep(10 * time.Millisecond)
	}

	log.Println("str-sender exiting")
}
