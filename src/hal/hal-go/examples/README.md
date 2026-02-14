# HAL-Go Examples

## passthrough

A simple component that demonstrates basic HAL-Go usage by copying input pins to output pins.

### Building

```bash
cd passthrough
go build -o passthrough
```

### Running

```bash
halrun
loadusr ./passthrough
show pin passthrough.*
# Set an input pin
setp passthrough.in-float 42.5
# Read the output
show pin passthrough.out-float
# Unload the component
unload passthrough
```

### Pins

| Pin Name | Type | Direction | Description |
|----------|------|-----------|-------------|
| passthrough.in-bit | bit | in | Input bit |
| passthrough.out-bit | bit | out | Output bit (copy of in-bit) |
| passthrough.in-float | float | in | Input float |
| passthrough.out-float | float | out | Output float (copy of in-float) |
| passthrough.in-s32 | s32 | in | Input signed 32-bit |
| passthrough.out-s32 | s32 | out | Output s32 (copy of in-s32) |
| passthrough.in-u32 | u32 | in | Input unsigned 32-bit |
| passthrough.out-u32 | u32 | out | Output u32 (copy of in-u32) |

## Testing

### Signal Handling Tests

1. **SIGTERM test** (halcmd unload):
   ```bash
   halrun
   loadusr ./passthrough
   unload passthrough
   # Should exit cleanly without errors
   ```

2. **SIGINT test** (Ctrl+C):
   ```bash
   ./passthrough
   # Press Ctrl+C
   # Should exit cleanly with "Received signal interrupt" message
   ```

3. **Verify no zombie processes**:
   ```bash
   halrun
   loadusr ./passthrough
   unload passthrough
   ps aux | grep passthrough
   # Should show no passthrough processes
   ```

### Functionality Tests

1. **Pin creation verification**:
   ```bash
   halrun
   loadusr ./passthrough
   show pin passthrough.*
   # Should show all 8 pins
   ```

2. **Value passthrough test**:
   ```bash
   halrun
   loadusr ./passthrough
   setp passthrough.in-float 123.456
   show pin passthrough.out-float
   # Should show 123.456
   ```
