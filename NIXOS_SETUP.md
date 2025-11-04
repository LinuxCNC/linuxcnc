# LinuxCNC on NixOS - Setup Guide

This guide explains how to build and run LinuxCNC on NixOS with real-time kernel support.

## Overview

LinuxCNC requires a real-time kernel for proper operation with CNC hardware. This Nix flake provides:

1. **LinuxCNC package** with all dependencies
2. **NixOS module** for real-time kernel configuration
3. **Development environment** for building and testing

## Quick Start

### Option 1: Using the Flake Directly

```bash
# Build LinuxCNC
nix build

# Run LinuxCNC in simulation mode
nix run

# Enter development shell
nix develop
```

### Option 2: NixOS System Integration

Add to your NixOS configuration:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    linuxcnc.url = "github:LinuxCNC/linuxcnc";
  };

  outputs = { self, nixpkgs, linuxcnc }: {
    nixosConfigurations.your-machine = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        ./configuration.nix
        linuxcnc.nixosModules.default
        {
          services.linuxcnc = {
            enable = true;
            enableRealtimeKernel = true;
          };

          # Add your user to realtime group
          users.users.youruser.extraGroups = [ "realtime" ];
        }
      ];
    };
  };
}
```

## Real-Time Kernel

### What is RT-PREEMPT?

LinuxCNC requires deterministic, low-latency response times (typically < 50 microseconds). The RT-PREEMPT patches modify the Linux kernel to provide:

- Fully preemptible kernel
- High-resolution timers
- Priority inheritance for mutexes
- Reduced interrupt latency

### Enabling Real-Time Kernel

The NixOS module automatically configures:

1. **RT-PREEMPT kernel** (`linuxPackages_rt_latest`)
2. **Kernel parameters** for real-time performance
3. **Security limits** for real-time priority
4. **CPU isolation** for dedicated real-time cores

### Testing Real-Time Performance

After booting with the RT kernel:

```bash
# Check kernel configuration
uname -a  # Should show "PREEMPT RT"

# Test latency (install rt-tests first)
sudo cyclictest -p 80 -t5 -n

# LinuxCNC latency test
latency-test
```

**Target latency values:**
- **Base thread**: < 25 microseconds (for software step generation)
- **Servo thread**: < 100 microseconds (for servo motor control)

If latency is too high:
- Disable CPU frequency scaling
- Disable hyperthreading in BIOS
- Disable power management
- Isolate CPUs for real-time use
- Check for problematic hardware (GPU drivers, network cards)

## Build Configuration

### Configure Flags

The flake uses these configure options:

- `--with-realtime=uspace`: User-space real-time (no kernel modules needed)
- `--enable-non-distributable=yes`: Include all features
- `--with-boost-python`: Python bindings

### Custom Build

To modify the build:

1. Edit `flake.nix`
2. Adjust `configureFlags` in the `linuxcnc` derivation
3. Rebuild: `nix build`

## Hardware Support

### Parallel Port (Legacy)

```nix
# Automatically enabled by the module
boot.kernelModules = [ "parport" "parport_pc" ];

# Check available ports
cat /proc/ioports | grep parport
```

### Mesa FPGA Cards

```nix
environment.systemPackages = [ pkgs.mesaflash ];
```

Configure in LinuxCNC with appropriate HAL files.

### EtherCAT

Requires additional setup:
- IgH EtherCAT Master for Linux
- Kernel modules
- Network interface isolation

### Ethernet Motion Control

```nix
# Prevent NetworkManager from managing the interface
networking.networkmanager.unmanaged = [ "eth1" ];
```

## Simulation Mode (No Hardware)

You can use LinuxCNC without real hardware for:
- G-code visualization
- HAL configuration testing
- User interface development

```bash
# Run LinuxCNC and select a 'sim' configuration
linuxcnc

# Or specify directly
linuxcnc configs/sim/axis/axis.ini
```

Simulation mode works without the real-time kernel, but performance will be limited.

## Development

### Building from Source

```bash
# Clone repository
git clone https://github.com/LinuxCNC/linuxcnc.git
cd linuxcnc

# Enter development shell
nix develop

# Build
cd src
./autogen.sh
./configure --with-realtime=uspace
make -j$(nproc)

# Run in place
source ../scripts/rip-environment
linuxcnc
```

### Adding Dependencies

Edit `flake.nix` and add to `buildInputs` or `nativeBuildInputs`.

## Troubleshooting

### Build Failures

**Error: "boost_python not found"**
- The flake automatically detects the Python version
- Check Python version compatibility with Boost

**Error: "autoconf not found"**
- Ensure `nativeBuildInputs` includes `autoconf`, `automake`

### Runtime Issues

**Error: "Real-time kernel not detected"**
- Check: `uname -a | grep PREEMPT`
- Verify: `services.linuxcnc.enableRealtimeKernel = true;`
- Rebuild: `sudo nixos-rebuild switch`

**Error: "Permission denied" on parallel port**
- Check udev rules: `/etc/udev/rules.d/`
- Verify user is in `realtime` group: `groups`

**High Latency**
- Run latency test: `latency-test`
- Check kernel parameters: `cat /proc/cmdline`
- Disable unnecessary services
- Update BIOS settings (disable C-states, speedstep)

### Performance Tuning

```bash
# Check CPU governor
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Should show "performance" on all CPUs

# Check isolated CPUs
cat /sys/devices/system/cpu/isolated

# Monitor real-time threads
ps -eLo pid,tid,class,rtprio,comm | grep FF
```

## Resources

- **LinuxCNC Documentation**: https://linuxcnc.org/docs/
- **RT-PREEMPT Wiki**: https://wiki.linuxfoundation.org/realtime/start
- **NixOS Manual**: https://nixos.org/manual/nixos/stable/
- **LinuxCNC Forums**: https://forum.linuxcnc.org/

## Contributing

To contribute improvements to the Nix packaging:

1. Fork the repository
2. Make changes to `flake.nix`
3. Test: `nix build` and `nix develop`
4. Submit pull request

## License

LinuxCNC is released under GPL v2+ and LGPL v3+ licenses. See `COPYING` for details.
