{
  description = "LinuxCNC - Motion controller for CNC machines and robots with real-time kernel support";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    flake-compat = {
      url = "github:edolstra/flake-compat";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
        };

        linuxcnc = pkgs.stdenv.mkDerivation rec {
          pname = "linuxcnc";
          version = pkgs.lib.strings.fileContents ./VERSION;

          src = ./.;

          nativeBuildInputs = with pkgs; [
            autoconf
            automake
            pkg-config
            gettext
            intltool
            asciidoc
            asciidoc-full
            docbook_xsl
            ghostscript
            groff
            imagemagick
            netpbm
            po4a
            desktop-file-utils
            yapps
            python3Packages.wrapPython
          ];

          buildInputs = with pkgs; [
            # Core dependencies
            boost
            python3
            python3Packages.numpy
            python3Packages.tkinter
            python3Packages.xlib
            python3Packages.pygobject3
            python3Packages.pyopengl
            python3Packages.configobj
            python3Packages.cairocffi
            python3Packages.pillow

            # GUI dependencies
            tcl
            tk
            tclx
            bwidget
            libepoxy
            libGL
            libGLU
            gtk3
            gtksourceview4
            mesa
            xorg.libXmu

            # Hardware/IO libraries
            libmodbus
            libgpiod
            libusb1
            libtirpc
            readline

            # Utilities
            netcat
            procps
            psmisc
            udev
          ];

          # LinuxCNC needs to be built from the src directory
          preConfigure = ''
            cd src
            ./autogen.sh
          '';

          configureFlags = [
            "--with-realtime=uspace"
            "--enable-non-distributable=yes"
            "--with-boost-python=boost_python${pkgs.lib.versions.major pkgs.python3.version}${pkgs.lib.versions.minor pkgs.python3.version}"
          ];

          enableParallelBuilding = true;

          # Skip tests that require real-time kernel or hardware
          doCheck = false;

          meta = with pkgs.lib; {
            description = "LinuxCNC - Motion controller for CNC machines and robots";
            longDescription = ''
              LinuxCNC is a fully-realised CNC machine controller that can interpret
              machine-control programs (such as G-code), plan trajectories and finally
              output low-level signals to machine control hardware.

              LinuxCNC relies on a realtime kernel to support real-time motion control,
              typically updating the position waypoints every millisecond and reacting
              to input within tens of microseconds.
            '';
            homepage = "https://linuxcnc.org/";
            license = with licenses; [ gpl2Plus lgpl3Plus ];
            platforms = platforms.linux;
            maintainers = [ ];
          };
        };

      in
      {
        packages = {
          default = linuxcnc;
          linuxcnc = linuxcnc;
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ linuxcnc ];

          buildInputs = with pkgs; [
            glade
            git
            vim
          ];

          shellHook = ''
            echo "LinuxCNC development environment"
            echo "================================="
            echo ""
            echo "LinuxCNC version: ${linuxcnc.version}"
            echo ""
            echo "Available commands:"
            echo "  nix build      - Build LinuxCNC"
            echo "  nix develop    - Enter development shell (you are here)"
            echo ""
            echo "For G-code visualization, run 'linuxcnc' and select a 'sim' configuration"
            echo ""
            echo "Note: Real-time kernel features require NixOS with RT kernel enabled."
            echo "See nixos-module.nix for configuration details."
            echo ""
          '';
        };

        apps.default = {
          type = "app";
          program = "${linuxcnc}/bin/linuxcnc";
        };
      }
    ) // {
      # NixOS module for real-time kernel configuration
      nixosModules.default = { config, lib, pkgs, ... }:
        with lib;
        let
          cfg = config.services.linuxcnc;
        in
        {
          options.services.linuxcnc = {
            enable = mkEnableOption "LinuxCNC with real-time kernel support";

            package = mkOption {
              type = types.package;
              default = self.packages.${pkgs.system}.linuxcnc;
              description = "LinuxCNC package to use";
            };

            enableRealtimeKernel = mkOption {
              type = types.bool;
              default = true;
              description = ''
                Enable the real-time kernel (RT-PREEMPT patches).
                This is required for proper LinuxCNC operation with hardware.
              '';
            };
          };

          config = mkIf cfg.enable {
            # Install LinuxCNC
            environment.systemPackages = [ cfg.package ];

            # Enable real-time kernel if requested
            boot.kernelPackages = mkIf cfg.enableRealtimeKernel (
              pkgs.linuxPackages_rt_latest
            );

            # Real-time kernel tuning
            boot.kernelParams = mkIf cfg.enableRealtimeKernel [
              "isolcpus=1,2,3"  # Isolate CPUs for real-time tasks (adjust based on your system)
              "idle=poll"        # Prevent CPU from sleeping
              "processor.max_cstate=1"  # Limit C-states
            ];

            # Security limits for real-time performance
            security.pam.loginLimits = [
              {
                domain = "@realtime";
                type = "soft";
                item = "rtprio";
                value = "99";
              }
              {
                domain = "@realtime";
                type = "hard";
                item = "rtprio";
                value = "99";
              }
              {
                domain = "@realtime";
                type = "soft";
                item = "memlock";
                value = "unlimited";
              }
              {
                domain = "@realtime";
                type = "hard";
                item = "memlock";
                value = "unlimited";
              }
            ];

            # Create realtime group
            users.groups.realtime = {};

            # Add your user to realtime group (example - adjust as needed)
            # users.users.youruser.extraGroups = [ "realtime" ];

            # Disable power management features that interfere with real-time
            powerManagement.cpuFreqGovernor = mkIf cfg.enableRealtimeKernel "performance";

            # Module needed for parallel port (if using legacy hardware)
            boot.kernelModules = [ "parport" "parport_pc" ];

            # Allow parallel port access
            services.udev.extraRules = ''
              KERNEL=="parport*", MODE="0666"
            '';
          };
        };
    };
}
