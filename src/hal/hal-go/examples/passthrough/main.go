// passthrough is a simple HAL component that copies input pins to output pins.
// It demonstrates the basic usage of the hal-go package.
//
// Usage:
//   go build -o passthrough
//   halrun
//   loadusr ./passthrough
//   show pin passthrough.*
//
// Pins created:
//   passthrough.in-bit      (bit, in)
//   passthrough.out-bit     (bit, out)
//   passthrough.in-float    (float, in)
//   passthrough.out-float   (float, out)
//   passthrough.in-s32      (s32, in)
//   passthrough.out-s32     (s32, out)
//   passthrough.in-u32      (u32, in)
//   passthrough.out-u32     (u32, out)

package main

import (
	"log"
	"time"

	"linuxcnc.org/hal"
)

func main() {
	// Create component
	comp, err := hal.NewComponent("passthrough")
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

	// Mark component ready
	if err := comp.Ready(); err != nil {
		log.Fatalf("Failed to mark component ready: %v", err)
	}

	log.Println("passthrough component ready")

	// Main loop - copy inputs to outputs
	for comp.Running() {
		outBit.Set(inBit.Get())
		outFloat.Set(inFloat.Get())
		outS32.Set(inS32.Get())
		outU32.Set(inU32.Get())

		time.Sleep(10 * time.Millisecond)
	}

	log.Println("passthrough component exiting")
}
