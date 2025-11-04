# Example NixOS configuration for LinuxCNC with real-time kernel
#
# To use this configuration:
# 1. Add this flake to your system's flake inputs
# 2. Import the NixOS module
# 3. Enable LinuxCNC service
#
# Example flake.nix for your NixOS system:
#
# {
#   inputs = {
#     nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
#     linuxcnc.url = "github:LinuxCNC/linuxcnc";
#   };
#
#   outputs = { self, nixpkgs, linuxcnc }: {
#     nixosConfigurations.your-hostname = nixpkgs.lib.nixosSystem {
#       system = "x86_64-linux";
#       modules = [
#         ./configuration.nix
#         linuxcnc.nixosModules.default
#         {
#           services.linuxcnc = {
#             enable = true;
#             enableRealtimeKernel = true;
#           };
#         }
#       ];
#     };
#   };
# }

{ config, pkgs, ... }:

{
  # Enable LinuxCNC with real-time kernel
  services.linuxcnc = {
    enable = true;
    enableRealtimeKernel = true;
  };

  # Add your user to the realtime group
  users.users.youruser = {
    extraGroups = [ "realtime" "dialout" "video" ];
  };

  # Additional system configuration for optimal real-time performance

  # Disable unnecessary services that might interfere with real-time
  services.xserver.displayManager.gdm.wayland = false;  # Use X11 instead of Wayland

  # System tuning
  boot.kernel.sysctl = {
    # Reduce swappiness for better real-time performance
    "vm.swappiness" = 10;

    # Increase the maximum number of file handles
    "fs.file-max" = 100000;
  };

  # For development/testing without real hardware:
  # You can disable the real-time kernel and use simulation mode
  # services.linuxcnc.enableRealtimeKernel = false;

  # Hardware-specific notes:
  #
  # 1. Parallel Port (legacy hardware):
  #    - The module automatically loads parport and parport_pc
  #    - Check available ports with: cat /proc/ioports | grep parport
  #
  # 2. Mesa Cards (modern FPGA-based I/O):
  #    - Install mesaflash: environment.systemPackages = [ pkgs.mesaflash ];
  #    - Configure appropriate permissions via udev rules
  #
  # 3. EtherCAT:
  #    - Requires additional kernel modules and configuration
  #    - Consider using IgH EtherCAT Master
  #
  # 4. Ethernet-based motion control:
  #    - May need to isolate network interface from NetworkManager
  #    - networking.networkmanager.unmanaged = [ "eth1" ];

  # Monitoring and debugging tools
  environment.systemPackages = with pkgs; [
    # Real-time testing
    # rt-tests  # Uncomment when available in nixpkgs

    # System monitoring
    htop
    iotop

    # Development tools
    gdb
    valgrind

    # LinuxCNC utilities (if built separately)
    # halcmd, halshow, halmeter, etc. are included with linuxcnc package
  ];
}
