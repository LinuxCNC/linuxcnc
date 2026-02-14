# hal-go - Go Bindings for LinuxCNC HAL

[![Go Version](https://img.shields.io/badge/go-1.21+-blue.svg)](https://golang.org/dl/)

Go bindings for LinuxCNC's Hardware Abstraction Layer (HAL), enabling userspace HAL components to be written in Go.

## Overview

HAL (Hardware Abstraction Layer) is the core communication mechanism in LinuxCNC. This package allows Go programs to:

- Register as HAL components
- Export pins that can be connected to signals
- Communicate with other HAL components via shared memory
- Integrate seamlessly into the LinuxCNC ecosystem

## Requirements

- Go 1.21 or later
- LinuxCNC (installed or RIP build)
- CGO enabled (`CGO_ENABLED=1`)
- GCC or compatible C compiler

## Installation

The hal-go package is installed as part of LinuxCNC when Go support is enabled during the build process.

### From Source (LinuxCNC)

When building LinuxCNC from source with Go support:

```bash
cd src
./configure --enable-go  # or use ./autogen.sh
make
```

The hal-go package will be built and installed to `share/linuxcnc/hal-go/` (in the source tree for RIP builds, or system-wide for installed builds).

### Using hal-go

- **Within LinuxCNC source tree**: The hal-go package is automatically available
- **For standalone components**: Use the `hal-go-template` (see Building Standalone Components below)

## Quick Start

Here's a simple example of a HAL component that doubles an input value:

```go
package main

import (
    "log"
    "time"

    "linuxcnc.org/hal"
)

func main() {
    // Create component
    comp, err := hal.NewComponent("doubler")
    if err != nil {
        log.Fatal(err)
    }
    defer comp.Exit()

    // Create pins
    input, _ := hal.NewPin[float64](comp, "input", hal.In)
    output, _ := hal.NewPin[float64](comp, "output", hal.Out)

    // Mark component ready
    if err := comp.Ready(); err != nil {
        log.Fatal(err)
    }

    log.Println("Doubler component ready")

    // Main loop
    for comp.Running() {
        output.Set(input.Get() * 2.0)
        time.Sleep(10 * time.Millisecond)
    }

    log.Println("Component shutting down")
}
```

## API Overview

### Component Lifecycle

```go
// Create component
comp, err := hal.NewComponent("mycomp")
defer comp.Exit()

// Create pins (must be done before Ready())
pin1, _ := hal.NewPin[float64](comp, "pin1", hal.In)
pin2, _ := hal.NewPin[bool](comp, "pin2", hal.Out)

// Mark component ready
comp.Ready()

// Main loop
for comp.Running() {
    // ... do work ...
}
```

### Pin Types

The package supports all HAL data types through Go's generics:

| Go Type   | HAL Type   | Description              |
|-----------|------------|--------------------------|
| `bool`    | `HAL_BIT`  | Boolean value            |
| `float64` | `HAL_FLOAT`| 64-bit floating point    |
| `int32`   | `HAL_S32`  | Signed 32-bit integer    |
| `uint32`  | `HAL_U32`  | Unsigned 32-bit integer  |

### Pin Directions

| Direction | Description                          |
|-----------|--------------------------------------|
| `hal.In`  | Input pin (component reads)          |
| `hal.Out` | Output pin (component writes)        |
| `hal.IO`  | Bidirectional pin (read and write)   |

### Creating Pins

```go
// Type-safe pin creation with generics
boolPin, _   := hal.NewPin[bool](comp, "enable", hal.In)
floatPin, _  := hal.NewPin[float64](comp, "speed", hal.Out)
int32Pin, _  := hal.NewPin[int32](comp, "count", hal.IO)
uint32Pin, _ := hal.NewPin[uint32](comp, "state", hal.Out)
```

### Reading and Writing Pins

```go
// Read from input pin
value := inputPin.Get()

// Write to output pin
outputPin.Set(42.0)

// Bidirectional pin
current := ioPin.Get()
ioPin.Set(current + 1)
```

## Integration with LinuxCNC

HAL components written in Go integrate fully with LinuxCNC:

```bash
# Load the component
halcmd loadusr -W go-doubler

# View component info
halcmd show comp doubler

# View pins
halcmd show pin doubler

# Connect pins to signals
halcmd net speed-in doubler.input <= some-other.output
halcmd net speed-out doubler.output => another-comp.input

# Start HAL
halcmd start
```

## Building Components

### Within LinuxCNC Source Tree

To build the example passthrough component:

```bash
cd src
make
```

The examples are built as part of the main LinuxCNC build when Go support is enabled.

### Standalone Components (hal-go-template)

For building Go HAL components **outside** the LinuxCNC source tree, a template with auto-detecting build system is provided.

**Location (installed):**
- System install: `/usr/share/linuxcnc/hal-go-template/`
- RIP build: `$EMC2_HOME/share/linuxcnc/hal-go-template/`

**Quick Start:**

```bash
# Copy the template
cp -r /usr/share/linuxcnc/hal-go-template ~/my-hal-component
cd ~/my-hal-component

# Build (with installed LinuxCNC)
make

# Or with RIP build
source /path/to/linuxcnc/scripts/rip-environment
make

# Test
halrun
halcmd: loadusr -W ./mycomponent
halcmd: show pin mycomponent.*
halcmd: exit
```

The template includes:
- `Makefile` - Auto-detects LinuxCNC installation (RIP or system)
- `go.mod` - Go module definition
- `main.go` - Example passthrough component
- `README.md` - Complete usage instructions

The template uses Go workspaces to reference the installed hal-go package, so you don't need to copy the HAL bindings into your project.

## Examples

The `examples/passthrough` directory contains a complete working example demonstrating all pin types and proper component lifecycle.

## Testing

To run the test suite:

```bash
cd src/hal/hal-go
./tests/run_tests.sh
```

Tests require a working LinuxCNC installation or RIP build with `rtapi.conf` configured.

## Documentation

- Package documentation: `go doc linuxcnc.org/hal`
- [LinuxCNC HAL Introduction](https://linuxcnc.org/docs/html/hal/intro.html)
- [HAL C API Reference](../hal.h)
- [Python HAL Bindings](../../../lib/python/hal.py)

## License

This code is part of LinuxCNC and is licensed under the GNU Lesser General Public License (LGPL) version 2 or later.

## References

- [LinuxCNC](https://linuxcnc.org/)
- [HAL Introduction](https://linuxcnc.org/docs/html/hal/intro.html)
- [HAL C API](../hal.h)
- [Python HAL Bindings](../../../lib/python/hal.py)

## Acknowledgments

This implementation follows the design patterns established by the Python HAL bindings (`halmodule.cc`) and is inspired by the successful integration of multiple languages in the LinuxCNC ecosystem.
