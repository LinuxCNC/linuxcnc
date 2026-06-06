# go-comp-template

A skeleton Go module for LinuxCNC's gomc-server.

Go modules are compiled directly into the `gomc-server` binary using
`modcompile add-gomod`. They run in the same process and have direct access
to HAL shared memory — no inter-process communication required.

## Overview

The module registers itself at `init()` time via `gomc.RegisterModule()`.
When gomc-server processes a `load mygomodule` HAL command, it calls the
registered factory function to create an instance.

```
# In your .hal file:
load mygomodule [optional-arguments]
```

Arguments after the module name are passed to the factory as `args []string`.

## Requirements

- Go 1.22 or later (same version used to build LinuxCNC)
- CGO enabled (`CGO_ENABLED=1`)
- LinuxCNC development environment (headers + libraries)

## Building and Installing

```bash
# Source the LinuxCNC environment first:
source /path/to/linuxcnc/scripts/rip-environment

# Verify the module compiles:
make

# Install into gomc-server (copies source, rebuilds binary):
make install

# Remove from gomc-server:
make uninstall
```

`make install` runs `modcompile add-gomod .` which:
1. Copies your source into `src/gomc/external/mygomodule/`
2. Registers it in `packages.conf`
3. Regenerates `imports_generated.go`
4. Rebuilds `gomc-server`

Reinstalling from the same directory auto-overwrites the previous copy.
Installing from a different directory requires `modcompile add-gomod --force .`.

## Module Interface

Your module must call `gomc.RegisterModule` in an `init()` function:

```go
func init() {
    gomc.RegisterModule("mygomodule", newMyGoModule)
}
```

The factory function creates and initializes the module (including HAL
component and pin creation):

```go
func newMyGoModule(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
    // Create HAL component, pins, etc.
    return &myModule{...}, nil
}
```

The `Module` interface has three lifecycle methods:

| Method      | Called when                              | Use for                                      |
|-------------|------------------------------------------|----------------------------------------------|
| `Start()`   | After HAL threads start                  | Start goroutines, open network connections   |
| `Stop()`    | During gomc-server cleanup               | Stop goroutines, close connections           |
| `Destroy()` | After all modules are stopped            | Release HAL components and allocated memory  |

## Usage in a HAL file

```
# Load the module with optional arguments:
load mygomodule config=/path/to/config.ini

# After loading, its HAL pins are available for wiring:
net my-signal mygomodule.in-f  some-component.output-pin
net my-signal mygomodule.out-f some-other-component.input-pin
```

## Customizing the Template

1. Rename the package — update `package mygomodule`, the `init()` registration
   name, and `MODULE_NAME` in the Makefile.
2. Replace the `myGoModule` struct and its `Start`/`Stop`/`Destroy` methods
   with your own implementation.
3. Update the factory function to create your module.
4. Update `go.mod` with your own module path.
5. Build with `make`, install with `make install`.
