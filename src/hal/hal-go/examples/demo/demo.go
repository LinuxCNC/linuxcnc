// Demo program showing the hal-go API (Phase 1 stub implementation)
// Build with: go build demo.go
// Note: This is a demonstration of the API structure only.
// Actual HAL integration will be added in Phase 2+.
package main

import (
"fmt"
"log"

hal "github.com/linuxcnc/hal-go"
)

func main() {
fmt.Println("=== Golang HAL Component Demo (Phase 1) ===")
fmt.Println()

// Create component
fmt.Println("Creating component 'demo'...")
comp, err := hal.NewComponent("demo")
if err != nil {
log.Fatal(err)
}
defer comp.Exit()
fmt.Printf("✓ Component created: %s (ID: %d)\n", comp.Name(), comp.ID())
fmt.Println()

// Create pins of different types
fmt.Println("Creating pins...")

enablePin, err := hal.NewPin[bool](comp, "enable", hal.In)
if err != nil {
log.Fatal(err)
}
fmt.Printf("✓ Pin created: %s (%s, %s)\n", enablePin.Name(), enablePin.Type(), enablePin.Direction())

speedPin, err := hal.NewPin[float64](comp, "speed", hal.In)
if err != nil {
log.Fatal(err)
}
fmt.Printf("✓ Pin created: %s (%s, %s)\n", speedPin.Name(), speedPin.Type(), speedPin.Direction())

outputPin, err := hal.NewPin[float64](comp, "output", hal.Out)
if err != nil {
log.Fatal(err)
}
fmt.Printf("✓ Pin created: %s (%s, %s)\n", outputPin.Name(), outputPin.Type(), outputPin.Direction())

counterPin, err := hal.NewPin[int32](comp, "counter", hal.Out)
if err != nil {
log.Fatal(err)
}
fmt.Printf("✓ Pin created: %s (%s, %s)\n", counterPin.Name(), counterPin.Type(), counterPin.Direction())

statusPin, err := hal.NewPin[uint32](comp, "status", hal.Out)
if err != nil {
log.Fatal(err)
}
fmt.Printf("✓ Pin created: %s (%s, %s)\n", statusPin.Name(), statusPin.Type(), statusPin.Direction())
fmt.Println()

// Mark component ready
fmt.Println("Marking component ready...")
if err := comp.Ready(); err != nil {
log.Fatal(err)
}
fmt.Printf("✓ Component is ready: %t\n", comp.IsReady())
fmt.Println()

// Demonstrate pin operations
fmt.Println("Demonstrating pin operations...")

// Set values (stub implementation)
enablePin.Set(true)
speedPin.Set(1500.0)
outputPin.Set(3000.0)
counterPin.Set(42)
statusPin.Set(0xFF)

// Read values
fmt.Printf("  enable = %v\n", enablePin.Get())
fmt.Printf("  speed = %.1f\n", speedPin.Get())
fmt.Printf("  output = %.1f\n", outputPin.Get())
fmt.Printf("  counter = %d\n", counterPin.Get())
fmt.Printf("  status = 0x%X\n", statusPin.Get())
fmt.Println()

// Component state
fmt.Println("Component state:")
fmt.Printf("  Running: %t\n", comp.Running())
fmt.Printf("  Ready: %t\n", comp.IsReady())
fmt.Println()

fmt.Println("=== Demo Complete ===")
fmt.Println()
fmt.Println("Note: This is Phase 1 with stub implementations.")
fmt.Println("Phase 2+ will add CGO bindings to the HAL C library.")
}
