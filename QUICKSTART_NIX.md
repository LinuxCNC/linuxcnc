# LinuxCNC NixOS Quick Start

This is the fastest way to get LinuxCNC running on NixOS.

## Prerequisites

- NixOS system (or Nix package manager with flakes enabled)
- Flakes enabled in your Nix configuration

To enable flakes, add to `/etc/nixos/configuration.nix`:
```nix
nix.settings.experimental-features = [ "nix-command" "flakes" ];
```

## Method 1: Try it without installing (Simulation Mode)

```bash
# Run LinuxCNC directly from the flake
nix run github:LinuxCNC/linuxcnc

# Or locally
git clone https://github.com/LinuxCNC/linuxcnc.git
cd linuxcnc
nix run
```

This will start LinuxCNC in simulation mode without installing anything permanently.

## Method 2: Development Shell

```bash
# Clone the repository
git clone https://github.com/LinuxCNC/linuxcnc.git
cd linuxcnc

# Enter development environment
nix develop

# You now have all build tools and dependencies available
# Build manually if needed:
cd src
./autogen.sh
./configure --with-realtime=uspace
make -j$(nproc)
```

## Method 3: Install on NixOS System (with Real-Time Kernel)

This is the **recommended method** for production CNC use.

### Step 1: Convert your system configuration to flakes

Create `/etc/nixos/flake.nix`:
```nix
{
  description = "My NixOS CNC Machine";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    linuxcnc.url = "github:LinuxCNC/linuxcnc";
  };

  outputs = { self, nixpkgs, linuxcnc }: {
    nixosConfigurations.cnc-machine = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        ./configuration.nix
        linuxcnc.nixosModules.default
      ];
    };
  };
}
```

### Step 2: Enable LinuxCNC in your configuration

Edit `/etc/nixos/configuration.nix`:
```nix
{ config, pkgs, ... }:

{
  # Import your existing hardware configuration
  imports = [ ./hardware-configuration.nix ];

  # Enable LinuxCNC with real-time kernel
  services.linuxcnc = {
    enable = true;
    enableRealtimeKernel = true;
  };

  # Add your user to the realtime group
  users.users.youruser = {
    isNormalUser = true;
    extraGroups = [ "wheel" "realtime" "dialout" ];
  };

  # Rest of your system configuration...
}
```

### Step 3: Rebuild your system

```bash
cd /etc/nixos
sudo nixos-rebuild switch
```

### Step 4: Reboot

After rebooting, you'll be running the real-time kernel.

Verify with:
```bash
uname -a  # Should show "PREEMPT RT"
```

### Step 5: Run LinuxCNC

```bash
linuxcnc  # Select a configuration
```

## Verifying Real-Time Performance

After setup, test the real-time performance:

```bash
# LinuxCNC latency test
latency-test
```

Run this for several minutes while:
- Moving windows
- Browsing the web
- Playing videos
- Running disk I/O

**Target values:**
- Servo thread: < 100 µs (microseconds)
- Base thread: < 25 µs (for software stepping)

If latency is too high, see `NIXOS_SETUP.md` for tuning tips.

## Simulation Configurations

LinuxCNC includes several simulation configurations for testing without hardware:

```bash
linuxcnc ~/linuxcnc/configs/sim/axis/axis.ini
```

Available sim configs:
- **axis**: Standard 3-axis mill
- **gmoccapy**: Modern touchscreen interface
- **touchy**: Touch-friendly interface
- **gscreen**: Customizable screen interface
- **lathe**: Lathe configuration
- **puma**: 6-axis robot arm

## Hardware Setup

Once your system is running:

1. **Parallel Port**: Automatically configured, check `/proc/ioports`
2. **Mesa Cards**: Install mesaflash and configure
3. **Modbus**: Built-in support enabled
4. **EtherCAT**: Requires additional configuration

See `NIXOS_SETUP.md` for detailed hardware setup.

## Troubleshooting

### "Nix command not found"
Install Nix: `sh <(curl -L https://nixos.org/nix/install) --daemon`

### "experimental feature 'flakes' is disabled"
Enable flakes:
```bash
mkdir -p ~/.config/nix
echo "experimental-features = nix-command flakes" >> ~/.config/nix/nix.conf
```

### Build fails
Check you have all prerequisites:
```bash
nix flake check  # Verify flake configuration
```

### Can't access hardware
Ensure your user is in the correct groups:
```bash
groups  # Should show: realtime dialout
```

### High latency
1. Verify RT kernel: `uname -a`
2. Check CPU governor: `cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor`
3. Should show "performance"
4. See tuning guide in `NIXOS_SETUP.md`

## Next Steps

- Read `NIXOS_SETUP.md` for comprehensive setup guide
- Check `nixos-configuration-example.nix` for advanced configuration
- Visit https://linuxcnc.org/docs/ for LinuxCNC documentation
- Join https://forum.linuxcnc.org/ for community support

## Building from Source

If you want to modify LinuxCNC itself:

```bash
git clone https://github.com/LinuxCNC/linuxcnc.git
cd linuxcnc
nix develop

# Make your changes to the source code

cd src
./autogen.sh
./configure --with-realtime=uspace --prefix=$PWD/..
make -j$(nproc)

# Run in place
cd ..
source scripts/rip-environment
linuxcnc
```

## Support

- **Documentation**: https://linuxcnc.org/docs/
- **Forum**: https://forum.linuxcnc.org/
- **IRC**: #linuxcnc on Libera.Chat
- **GitHub**: https://github.com/LinuxCNC/linuxcnc

---

**Note**: The real-time kernel is essential for controlling actual CNC machines. Without it, LinuxCNC runs in simulation mode only, which is fine for testing G-code and learning the interface.
