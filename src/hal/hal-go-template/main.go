// Example HAL component - Passthrough
//
// This component demonstrates the hal-go API by creating pins of each type
// and copying input values to corresponding output pins.
//
// Pins created:
//   - mycomponent.in-bit    (input)  -> mycomponent.out-bit    (output)
//   - mycomponent.in-float  (input)  -> mycomponent.out-float  (output)
//   - mycomponent.in-s32    (input)  -> mycomponent.out-s32    (output)
//   - mycomponent.in-u32    (input)  -> mycomponent.out-u32    (output)
//   - mycomponent.in-str    (input)  -> mycomponent.out-str    (output)
//
// Note: string pins use the HAL "port" type and require a port signal to be
// created and linked before data can transfer (e.g. newsig my-msg port /
// net my-msg comp.out-str other.in-str / sets my-msg 1024).
//
// Usage:
//   halrun
//   halcmd: loadusr -W ./mycomponent
//   halcmd: setp mycomponent.in-float 123.456
//   halcmd: show pin mycomponent.*
//   halcmd: unload mycomponent

package main

import (
	"log"
	"time"

	"linuxcnc.org/hal"
)

func main() {
	// Create component - name should match binary name for loadusr -W
	comp, err := hal.NewComponent("mycomponent")
	if err != nil {
		log.Fatalf("Failed to create component: %v", err)
	}
	defer comp.Exit()

	// Create input pins
	inBit, err := hal.NewPin[bool](comp, "in-bit", hal.In)
	if err != nil {
		log.Fatalf("Failed to create in-bit pin: %v", err)
	}

	inFloat, err := hal.NewPin[float64](comp, "in-float", hal.In)
	if err != nil {
		log.Fatalf("Failed to create in-float pin: %v", err)
	}

	inS32, err := hal.NewPin[int32](comp, "in-s32", hal.In)
	if err != nil {
		log.Fatalf("Failed to create in-s32 pin: %v", err)
	}

	inU32, err := hal.NewPin[uint32](comp, "in-u32", hal.In)
	if err != nil {
		log.Fatalf("Failed to create in-u32 pin: %v", err)
	}

	inStr, err := hal.NewPin[string](comp, "in-str", hal.In)
	if err != nil {
		log.Fatalf("Failed to create in-str pin: %v", err)
	}

	// Create output pins
	outBit, err := hal.NewPin[bool](comp, "out-bit", hal.Out)
	if err != nil {
		log.Fatalf("Failed to create out-bit pin: %v", err)
	}

	outFloat, err := hal.NewPin[float64](comp, "out-float", hal.Out)
	if err != nil {
		log.Fatalf("Failed to create out-float pin: %v", err)
	}

	outS32, err := hal.NewPin[int32](comp, "out-s32", hal.Out)
	if err != nil {
		log.Fatalf("Failed to create out-s32 pin: %v", err)
	}

	outU32, err := hal.NewPin[uint32](comp, "out-u32", hal.Out)
	if err != nil {
		log.Fatalf("Failed to create out-u32 pin: %v", err)
	}

	outStr, err := hal.NewPin[string](comp, "out-str", hal.Out)
	if err != nil {
		log.Fatalf("Failed to create out-str pin: %v", err)
	}

	// Mark component as ready
	if err := comp.Ready(); err != nil {
		log.Fatalf("Failed to mark component ready: %v", err)
	}
	log.Println("mycomponent ready")

	// Main loop - copy inputs to outputs
	for comp.Running() {
		outBit.Set(inBit.Get())
		outFloat.Set(inFloat.Get())
		outS32.Set(inS32.Get())
		outU32.Set(inU32.Get())
		outStr.Set(inStr.Get())

		time.Sleep(10 * time.Millisecond)
	}

	log.Println("mycomponent exiting")
}
