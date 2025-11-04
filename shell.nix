{ pkgs ? import <nixpkgs> {} }:

let
  # Import the LinuxCNC Nix repository
  linuxcnc-nix = import ./linuxcnc-nix { inherit pkgs; };
in
pkgs.mkShell {
  buildInputs = [
    # Try different possible paths for the LinuxCNC package
    (linuxcnc-nix.packages.${pkgs.system}.linuxcnc or
     linuxcnc-nix.linuxcnc or
     linuxcnc-nix.packages.${pkgs.system}.default or
     linuxcnc-nix.defaultPackage.${pkgs.system})
    # Add any other tools you might need
    pkgs.glade  # For GUI development if needed
  ];
  
  shellHook = ''
    echo "LinuxCNC development environment loaded"
    echo "Available commands:"
    echo "  linuxcnc      - Start LinuxCNC"
    echo "  halcmd        - HAL command line"
    echo "  latency-test  - Test system latency"
    echo ""
    echo "For G-code visualization, run 'linuxcnc' and select a 'sim' configuration"
  '';
}
