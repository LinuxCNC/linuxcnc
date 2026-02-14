# Demo Example

This is a simple demonstration of the hal-go API (Phase 1 stub implementation).

## Build

```bash
go build demo.go
```

## Run

```bash
./demo
```

## Expected Output

The demo creates a HAL component with various pin types and demonstrates:
- Component creation
- Pin creation for all supported types (bool, float64, int32, uint32)
- Marking component ready
- Reading and writing pin values
- Component state queries

## Note

This is Phase 1 with stub implementations. The API is functional but does not yet
interface with the LinuxCNC HAL C library. Phase 2+ will add CGO bindings for
actual HAL integration.
