{ pkgs ? import <nixpkgs> {} }:

let
  # Get the LinuxCNC overlay
  linuxcnc-nix = import (fetchTarball {
    url = "https://github.com/mattywillo/linuxcnc-nix/archive/master.tar.gz";
  }) { inherit pkgs; };
  
  # Apply the overlay to get LinuxCNC packages
  pkgs-with-linuxcnc = import <nixpkgs> {
    overlays = [ linuxcnc-nix.overlay ];
  };
in

pkgs.mkShell {
  buildInputs = [
    pkgs-with-linuxcnc.linuxcnc
    pkgs.glade  # For GUI development
  ];
  
  shellHook = ''
    echo "LinuxCNC environment ready!"
    echo "Run 'linuxcnc' and select a 'sim' configuration for G-code visualization"
  '';
}