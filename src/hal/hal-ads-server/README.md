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
  -ams-net-id string  AMS Net ID of this device (default "5.80.201.232.1.1")
  -verbose            Enable verbose debug logging
```

### Loading from halcmd

```bash
halcmd loadusr -W hal-ads-server /path/to/my-symbols.cfg
```

## Config File Format

The config file uses 2-space indentation to define the ADS symbol hierarchy.
Each leaf line (with `in` or `out`) creates a HAL pin.

```
stDISPLAY_DATA
  in bErrRest bool
  stPOOL[1..9]
    in bReady bool
    out nErrCount dint
    in bAck bool
    in sName string(32)
```

### Direction semantics

| Config dir | HAL direction | Meaning                                    |
|------------|---------------|--------------------------------------------|
| `in`       | HAL_IN        | HMI writes to HAL (component reads)        |
| `out`      | HAL_OUT       | HMI reads from HAL (component writes)      |

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
hal-ads-server.stDISPLAY_DATA.stPOOL.1.bReady    (bit, in)
hal-ads-server.stDISPLAY_DATA.stPOOL.1.nErrCount (s32, out)
hal-ads-server.stDISPLAY_DATA.stPOOL.1.bAck      (bit, in)
hal-ads-server.stDISPLAY_DATA.stPOOL.1.sName     (port, in)
...through stPOOL.9...
```

### ADS symbol names (as seen by the HMI)

```
stDISPLAY_DATA.bErrRest
stDISPLAY_DATA.stPOOL[1].bReady
stDISPLAY_DATA.stPOOL[1].nErrCount
...through stPOOL[9]...
```

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
| Read (symbol list info)   | 0xF00A     | Upload size and symbol count         |
| Read (symbol list)        | 0xF00B     | Full symbol list upload              |
| Read/Write (process image)| 0x4040     | Direct access by byte offset         |

## License

Part of LinuxCNC. Licensed under the GNU LGPL v2+.
