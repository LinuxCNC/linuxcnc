// Package config provides compile-time path configuration for the LinuxCNC launcher.
// Variables are set via -ldflags -X at build time, matching the @VAR@ substitutions
// in the legacy linuxcnc.in bash script.
package config

// Compile-time path variables. These are set via -ldflags when building:
//
//	-X 'github.com/sittner/linuxcnc/src/launcher/config.EMC2Home=/usr/lib/linuxcnc'
var (
	// EMC2Home is the top-level LinuxCNC installation directory (@EMC2_HOME@).
	EMC2Home string

	// EMC2BinDir is the directory containing LinuxCNC binaries (@EMC2_BIN_DIR@).
	EMC2BinDir string

	// EMC2TclDir is the directory containing LinuxCNC Tcl files (@EMC2_TCL_DIR@).
	EMC2TclDir string

	// EMC2HelpDir is the directory containing LinuxCNC help files (@EMC2_HELP_DIR@).
	EMC2HelpDir string

	// EMC2RtlibDir is the directory containing LinuxCNC realtime libraries (@EMC2_RTLIB_DIR@).
	EMC2RtlibDir string

	// EMC2ConfigPath is the colon-separated list of config search directories (@LINUXCNC_CONFIG_PATH@).
	EMC2ConfigPath string

	// EMC2NCFilesDir is the directory containing LinuxCNC NC files (@EMC2_NCFILES_DIR@).
	EMC2NCFilesDir string

	// EMC2LangDir is the directory containing LinuxCNC language files (@EMC2_LANG_DIR@).
	EMC2LangDir string

	// EMC2ImageDir is the directory containing LinuxCNC images (@EMC2_IMAGE_DIR@).
	EMC2ImageDir string

	// EMC2TclLibDir is the directory containing LinuxCNC Tcl library files (@EMC2_TCL_LIB_DIR@).
	EMC2TclLibDir string

	// HalibDir is the directory containing HAL library files (@HALLIB_DIR@).
	HalibDir string

	// EMC2Version is the LinuxCNC version string (@EMC2VERSION@).
	EMC2Version string

	// RunInPlace indicates whether LinuxCNC is being run from the build directory ("yes" or "no") (@RUN_IN_PLACE@).
	RunInPlace string

	// DefaultNmlFile is the path to the default NML configuration file (@DEFAULT_NMLFILE@).
	DefaultNmlFile string

	// ModExt is the module file extension for loadable kernel modules (@MODEXT@).
	ModExt string

	// KernelVers is the required kernel version string (@KERNEL_VERS@).
	KernelVers string
)
