# GMI - Generated Message Interface

GMI provides a unified interface system for inter-module communication in LinuxCNC,
intended to replace NML with a modern, type-safe approach.

## Directory Structure

```
src/gmi/
├── idl/           # Interface Definition Language files (.gmi)
│   ├── hal.gmi    # HAL component API
│   ├── halcmd.gmi # HAL command REST API
│   └── README.md  # IDL format documentation
├── lib/           # C runtime library (libgmi)
│   ├── gmi.h      # Main include header
│   ├── gmi_*.c/h  # Library implementation
│   ├── Submakefile
│   └── README.md  # Library API documentation
└── README.md      # This file
```

## Components

### IDL Compiler (`gmicompile`)

Compiles `.gmi` interface definitions to code:

```bash
# Parse and dump AST
gmicompile --parse hal.gmi

# Generate C server header (types, callbacks)
gmicompile --server-c hal.gmi -o hal_api.h

# Generate C REST client (uses libgmi)
gmicompile --client-c halcmd.gmi -o halcmd_client
```

The compiler is built from Go source in `src/gomc/cmd/gmicompile/`.

### Runtime Library (`libgmi`)

C library providing common utilities for generated code:
- Error handling (`gmi_error.h`)
- Dynamic buffers (`gmi_types.h`)
- JSON parsing/building (`gmi_json.h`, wraps cJSON)
- HTTP client (`gmi_http.h`, wraps libcurl)

See `lib/README.md` for API documentation.

## Generated Code

Generated code goes to `src/generated/` (gitignored).

## Building

```bash
# Build gmicompile
cd src/gomc && go build ./cmd/gmicompile

# Build libgmi (via main Makefile)
make  # includes src/gmi/lib/Submakefile
```

## Dependencies

Both are required and checked by `./configure`:

```bash
sudo apt install libcurl4-openssl-dev libcjson-dev
```
