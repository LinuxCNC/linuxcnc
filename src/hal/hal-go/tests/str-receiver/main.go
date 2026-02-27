package main

import (
	"log"
	"os"
	"time"

	"linuxcnc.org/hal"
)

func main() {
	// Accept optional output file argument for test automation
	var outFile string
	if len(os.Args) > 1 {
		outFile = os.Args[1]
	}

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
			log.Printf("Received: %s", msg)
			// Write result to file if output path was provided
			if outFile != "" {
				if err := os.WriteFile(outFile, []byte("RECEIVED:"+msg+"\n"), 0644); err != nil {
					log.Printf("Failed to write result file: %v", err)
				}
			}
			// Keep running so halcmd can unload us cleanly
			for comp.Running() {
				time.Sleep(50 * time.Millisecond)
			}
			break
		}
		time.Sleep(10 * time.Millisecond)
	}

	log.Println("str-receiver exiting")
}
