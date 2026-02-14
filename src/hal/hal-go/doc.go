/*
Package hal provides Go bindings for LinuxCNC's Hardware Abstraction Layer (HAL).

HAL is the Hardware Abstraction Layer used by LinuxCNC to transfer realtime data
to and from I/O devices and other low-level modules. This package allows Go
programs to create userspace HAL components that can interact with other HAL
components (both realtime and userspace).

# Overview

A HAL component is a program that exports pins and/or parameters. Pins can be
connected to signals, which allow components to exchange data. The HAL maintains
all data in shared memory, enabling efficient communication between components.

# Basic Usage

The typical flow for a HAL component is:

 1. Create a component with NewComponent()
 2. Create pins with NewPin[T]()
 3. Mark the component ready with Ready()
 4. Enter a main loop that checks Running()
 5. Clean up with Exit() (typically via defer)

Example:

	package main

	import (
		"log"
		"time"

		"linuxcnc.org/hal"
	)

	func main() {
		// Create component
		comp, err := hal.NewComponent("go-example")
		if err != nil {
			log.Fatal(err)
		}
		defer comp.Exit()

		// Create pins
		input, err := hal.NewPin[float64](comp, "input", hal.In)
		if err != nil {
			log.Fatal(err)
		}

		output, err := hal.NewPin[float64](comp, "output", hal.Out)
		if err != nil {
			log.Fatal(err)
		}

		enable, err := hal.NewPin[bool](comp, "enable", hal.In)
		if err != nil {
			log.Fatal(err)
		}

		// Mark component ready
		if err := comp.Ready(); err != nil {
			log.Fatal(err)
		}

		log.Println("Component ready, entering main loop")

		// Main loop
		for comp.Running() {
			if enable.Get() {
				output.Set(input.Get() * 2.0)
			}
			time.Sleep(10 * time.Millisecond)
		}

		log.Println("Component shutting down")
	}

# Pin Types

HAL supports several data types for pins:

  - bool (HAL_BIT): Boolean values
  - float64 (HAL_FLOAT): 64-bit floating point
  - int32 (HAL_S32): Signed 32-bit integer
  - uint32 (HAL_U32): Unsigned 32-bit integer

Pins are created using the generic NewPin[T]() function, which provides
compile-time type safety:

	boolPin, _ := hal.NewPin[bool](comp, "enable", hal.In)
	floatPin, _ := hal.NewPin[float64](comp, "velocity", hal.Out)
	int32Pin, _ := hal.NewPin[int32](comp, "count", hal.IO)
	uint32Pin, _ := hal.NewPin[uint32](comp, "status", hal.Out)

# Pin Directions

Pins have a direction that specifies how data flows:

  - In: Component reads the pin value (input)
  - Out: Component writes the pin value (output)
  - IO: Component can both read and write (bidirectional)

# Build Requirements

This package uses CGO to interface with the LinuxCNC HAL library. To build 
programs using this package, you need:

  - Go 1.21 or later
  - LinuxCNC development headers
  - CGO_ENABLED=1

# Integration with LinuxCNC

HAL components written in Go integrate seamlessly with the rest of LinuxCNC:

  - Use 'halcmd show comp' to see loaded components
  - Use 'halcmd show pin' to see exported pins
  - Use 'halcmd net' to connect pins to signals
  - Use 'halcmd loadusr -W' to wait for component to be ready

# Signal Handling

HAL components automatically handle SIGTERM and SIGINT for graceful shutdown.
The Running() method returns false when a shutdown signal is received, allowing
the component to clean up and exit properly.

# References

For more information about LinuxCNC HAL:

  - HAL Manual: https://linuxcnc.org/docs/html/hal/intro.html
  - HAL C API: src/hal/hal.h
  - Python HAL bindings: lib/python/hal.py
*/
package hal
