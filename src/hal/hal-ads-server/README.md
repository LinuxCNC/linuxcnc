# hal-ads-server

A LinuxCNC HAL component that implements a Beckhoff ADS/TwinCAT protocol server.
It bridges ADS symbol reads and writes from TwinCAT HMI clients directly to
LinuxCNC HAL pins.

## Architecture

```
┌─────────────────┐   TCP    ┌──────────────────────────────────────────────┐
│  TwinCAT HMI    │─────────▶│  hal-ads-server  (Go binary)                │
│  Client         │◀─────────│                                              │
└─────────────────┘          │  ┌──────────┐  ┌────────┐  ┌─────────────┐ │
                              │  │ ads/     │──│ bridge │──│ hal-go pins │ │
                              │  │ server   │  │        │  │ Get/Set     │ │
                              │  │ protocol │  │        │  │             │ │
                              │  └──────────┘  └────────┘  └─────────────┘ │
                              └──────────────────────────────────────────────┘
                                         │                      │
                                    Config file           HAL shared mem
```

**No cyclic polling loop is needed.** ADS read/write requests directly trigger
HAL pin `Get()`/`Set()` operations.

## Building

### Within the LinuxCNC source tree (RIP build)

```bash
source scripts/rip-environment
cd src/hal/hal-ads-server
make
```

### With an installed LinuxCNC

```bash
cd src/hal/hal-ads-server
make
```

## Usage

```
hal-ads-server [options] <config-file>

Options:
  -name string        HAL component name (default "hal-ads-server")
  -port int           TCP port for ADS server (default 48898)
  -bind string        IP address to bind the ADS server to (default "0.0.0.0")
  -ams-net-id string  AMS Net ID of this device (default "192.168.0.99.1.1")
  -verbose            Enable verbose debug logging
```

### Loading from halcmd

```bash
halcmd loadusr -W hal-ads-server /path/to/my-symbols.cfg
```

## Config File Format

The config file uses consistent indentation (any number of spaces or tabs) to define the ADS symbol hierarchy. The indent style is auto-detected from the first indented line; all subsequent lines must use the same character (spaces or tabs) and a consistent multiple of that indent unit.
Each leaf line (with `in`, `out`, `inout`, or `pad`) creates a HAL pin (except
`pad`, which only reserves space in the process image).

```
stDISPLAY_DATA
  in bErrRest bool
  in aFlags[1..8] BOOL
  struct stPOOL ST_POOL
  struct stPOOLs[1..9] ST_POOL
```

The `struct` keyword references a named struct type declared with `@struct`. Both
`struct varName TypeName` (single instance) and `struct varName[start..end] TypeName`
(array of struct instances) are supported. Similarly, all direction keywords
(`in`, `out`, `inout`, `pad`) accept an optional `[start..end]` suffix to declare
a scalar array inline.

Plain unnamed containers (struct groupings without a type) still use
depth-based indentation:

```
DISPLAY_DATA
  stData
    in bFlag BOOL
    stItems[1..9]
      in bReady bool
      out nErrCount dint
```

### Direction semantics

| Config dir | HAL direction | Meaning                                                      |
|------------|---------------|--------------------------------------------------------------|
| `in`       | HAL_IN        | HMI writes to HAL (component reads)                         |
| `out`      | HAL_OUT       | HMI reads from HAL (component writes)                       |
| `inout`    | HAL_IO        | Bidirectional: both HMI and HAL can read and write          |
| `pad`      | *(none)*      | Reserved space; no HAL pin, not listed in ADS symbol table  |

### Automatic struct alignment

The server automatically applies TwinCAT default pack mode 0 (natural
C-compiler-style alignment) when computing byte offsets in the process image:

- Each field is placed at the next address that is a multiple of its natural
  alignment (BOOL/BYTE: 1, WORD/INT: 2, DWORD/REAL/TIME: 4, LREAL: 8,
  STRING(n): 1).
- A struct's start is aligned to the maximum member alignment.
- A struct's size is padded to a multiple of the maximum member alignment
  (tail padding), which ensures correct layout for arrays of structs.

This means **no manual `pad` entries are needed for alignment**. A clean config
without `pad` entries will produce the same byte offsets as an equivalent config
with explicit alignment padding — the two approaches are idempotent.

The `pad` direction is still accepted and useful for reserving space for fields
that are present in the TwinCAT struct but not exposed as HAL pins (e.g.
reserved or future-use fields). Pad entries occupy process-image space (reads
return zeros; writes are discarded) but are not registered in the ADS symbol
table and do not create HAL pins.

### ADS type to HAL pin type mapping

| ADS/TwinCAT Type              | HAL Pin Type | Wire Size |
|-------------------------------|--------------|-----------|
| BOOL                          | bit          | 1 byte    |
| BYTE, USINT                   | u32          | 1 byte    |
| WORD, UINT                    | u32          | 2 bytes   |
| DWORD, UDINT, TIME, TOD, DATE | u32          | 4 bytes   |
| SINT                          | s32          | 1 byte    |
| INT                           | s32          | 2 bytes   |
| DINT                          | s32          | 4 bytes   |
| REAL                          | float        | 4 bytes   |
| LREAL                         | float        | 8 bytes   |
| STRING(n)                     | port/string  | n+1 bytes |

### HAL pin names

For the example above (with component name `hal-ads-server`):

```
hal-ads-server.stDISPLAY_DATA.bErrRest           (bit, in)
hal-ads-server.stDISPLAY_DATA.aFlags.1            (bit, in)
hal-ads-server.stDISPLAY_DATA.stPOOLs.1.bReady   (bit, in)
hal-ads-server.stDISPLAY_DATA.stPOOLs.1.nErrCount (s32, out)
...through stPOOLs.9...
```

### ADS symbol names (as seen by the HMI)

```
stDISPLAY_DATA.bErrRest
stDISPLAY_DATA.aFlags[1]
stDISPLAY_DATA.stPOOLs[1].bReady
stDISPLAY_DATA.stPOOLs[1].nErrCount
...through stPOOLs[9]...
```

### `@enum` and `@struct` directives

Before the symbol tree, you may declare named TwinCAT type aliases with `@enum`
(for enumeration types) or `@struct` (for struct types):

```
@enum <EnumName> <BaseType> <GUID>
  <memberName> [intValue]
  ...

@struct <StructName> <GUID>
  <dir> <fieldName> <Type>
  struct <fieldName> <OtherStructType>
  struct <fieldName>[start..end] <OtherStructType>
  ...
```

Example:

```
# @enum members: explicit value sets the member value; subsequent members
# without a value auto-increment from the previous value.
@enum EN_DISP_POOL_STATE WORD 4bb8098e-6846-4a59-915d-71a3e3d369c0
  empty 0       # explicit value 0
  newForm       # auto-increments to 1
  formLoaded    # auto-increments to 2

@struct ST_POOL 5d3e96cf-52f5-40f7-8f2f-03f772b420db
  in eState EN_DISP_POOL_STATE
  in fTemp REAL

@struct ST_DATA 354914ab-5602-4319-a5dd-d33707893044
  in bGlobalErr BOOL
  struct aPools[1..4] ST_POOL

DISPLAY_DATA
  struct stData ST_DATA
```

- **EnumName / StructName** — the TwinCAT derived type name (stored upper-cased).
  Use this name as the `TYPE` field for leaf nodes in the config, or as the
  `StructType` argument to `struct varName TypeName`.
- **BaseType** — (`@enum` only) the underlying ADS primitive type (e.g. `WORD`, `DINT`).
  Used for size/alignment computation and HAL pin creation.
- **GUID** — the 16-byte data-type GUID in Microsoft COM wire format
  (`xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`). Returned in
  `ADSIGRP_SYM_INFOBYNAMEEX` (0xF009) responses so TwinCAT HMI clients can
  match the correct symbol type, enabling enum display, range checks, etc.

When a leaf node references an alias type name, the node retains the alias name
(e.g. `EN_DISP_MSGTYPE`) so the HMI sees the correct derived type in all
symbol-info responses. The base type is resolved internally for HAL pin
allocation and byte-offset computation.

## ADS Protocol Support

The server implements the ADS/AMS protocol over TCP (default port 48898):

| ADS Function              | IndexGroup | Description                          |
|---------------------------|------------|--------------------------------------|
| ReadDeviceInfo            | –          | Returns device name and version      |
| ReadState                 | –          | Returns ADS state (Run = 5)          |
| ReadWrite (handle by name)| 0xF003     | Allocate symbol handle by name       |
| Read (value by handle)    | 0xF005     | Read HAL pin via handle              |
| Write (value by handle)   | 0xF005     | Write HAL pin via handle             |
| Write (release handle)    | 0xF006     | Release previously allocated handle  |
| ReadWrite (symbol info)   | 0xF007     | Read symbol metadata by name         |
| Read (symbol count)       | 0xF009     | Number of registered symbols         |
| ReadWrite (extended info) | 0xF009     | Extended symbol info by name (`ADSIGRP_SYM_INFOBYNAMEEX`) — returns standard metadata plus `arrayDim`, 16-byte `dataTypeGUID`, and per-dimension array bounds |
| Read (symbol list info)   | 0xF00A     | Upload size and symbol count         |
| Read (symbol list)        | 0xF00B     | Full symbol list upload              |
| Read/Write (process image)| 0x4040     | Direct access by byte offset         |

> **Note:** Index group `0xF009` serves two purposes. An ADS **Read** returns the
> symbol count. An ADS **ReadWrite** (write data = symbol name) returns the
> extended symbol info (`ADSIGRP_SYM_INFOBYNAMEEX`) used by TwinCAT HMI
> clients to resolve type metadata including array dimensions and data-type GUIDs.

## License

Part of LinuxCNC. Licensed under the GNU LGPL v2+.
