package hal_test

import (
	"fmt"

	"linuxcnc.org/hal"
)

// Example demonstrates basic usage of the hal package.
func Example() {
// Create component
comp, err := hal.NewComponent("example")
if err != nil {
fmt.Printf("Error creating component: %v\n", err)
return
}
defer comp.Exit()

// Create pins
input, _ := hal.NewPin[float64](comp, "input", hal.In)
output, _ := hal.NewPin[bool](comp, "output", hal.Out)
strPin, _ := hal.NewPin[string](comp, "message", hal.Out)

// Mark ready
comp.Ready()

// Use pins
input.Set(3.14)
fmt.Printf("Input value: %.2f\n", input.Get())

output.Set(true)
fmt.Printf("Output value: %t\n", output.Get())

fmt.Printf("String type: %s\n", strPin.Type())

fmt.Printf("Component: %s\n", comp.Name())
fmt.Printf("Running: %t\n", comp.Running())

// Output:
// Input value: 3.14
// Output value: true
// String type: PORT
// Component: example
// Running: true
}
