package hal

import (
	"fmt"
	"path"
	"strings"
)

// StartThreads starts all HAL realtime threads.
// This is the point at which realtime functions start being called.
// Equivalent to "halcmd start".
func StartThreads() error {
	return halStartThreads()
}

// StopThreads stops all HAL realtime threads.
// This should be called before any component that is part of a system exits.
// Equivalent to "halcmd stop".
func StopThreads() error {
	return halStopThreads()
}

// ListComponents returns the names of all currently loaded HAL components.
// Equivalent to "halcmd list comp".
func ListComponents() ([]string, error) {
	return halListComponents()
}

// UnloadAll unloads all HAL components except the specified one.
// Pass the caller's own component ID to avoid unloading itself.
// Equivalent to "halcmd unload all".
func UnloadAll(exceptCompID int) error {
	return halUnloadAll(exceptCompID)
}

// ===== Signal commands =====

// NewSig creates a new HAL signal with the given name and type.
// Equivalent to "halcmd newsig <name> <type>".
func NewSig(name string, halType PinType) error {
	return halNewSig(name, halType)
}

// DelSig deletes a HAL signal by name.
// Equivalent to "halcmd delsig <name>".
func DelSig(name string) error {
	return halDelSig(name)
}

// SetS sets the value of a HAL signal by name.
// The value is provided as a string and parsed according to the signal's type.
// Equivalent to "halcmd sets <name> <value>".
func SetS(name string, value string) error {
	return halSetS(name, value)
}

// GetS returns the current value of a HAL signal as a string.
// Equivalent to "halcmd gets <name>".
func GetS(name string) (string, error) {
	return halGetS(name)
}

// SType returns the data type of a HAL signal.
// Equivalent to "halcmd stype <name>".
func SType(name string) (PinType, error) {
	return halSType(name)
}

// ===== Pin/param value commands =====

// SetP sets the value of a HAL pin or parameter by name.
// The value is provided as a string and parsed according to the pin/param type.
// Equivalent to "halcmd setp <name> <value>".
func SetP(name string, value string) error {
	return halSetP(name, value)
}

// GetP returns the current value of a HAL pin or parameter as a string.
// Equivalent to "halcmd getp <name>".
func GetP(name string) (string, error) {
	return halGetP(name)
}

// PType returns the data type of a HAL pin or parameter.
// Equivalent to "halcmd ptype <name>".
func PType(name string) (PinType, error) {
	return halPType(name)
}

// ===== Link/net commands =====

// removeArrows filters out HAL arrow tokens (=>, <=, <=>) from a pin list.
// Arrow tokens are valid in .hal files for readability but have no functional meaning.
func removeArrows(pins []string) []string {
	result := make([]string, 0, len(pins))
	for _, p := range pins {
		if p != "=>" && p != "<=" && p != "<=>" {
			result = append(result, p)
		}
	}
	return result
}

// Net connects one or more pins to a signal, creating the signal if needed.
// Arrow tokens (=>, <=, <=>) are automatically stripped.
// Equivalent to "halcmd net <signame> [pins...]".
func Net(signame string, pins ...string) error {
	filtered := removeArrows(pins)
	if len(filtered) == 0 {
		return fmt.Errorf("net: no pins specified for signal %q", signame)
	}
	return halNet(signame, filtered)
}

// LinkPS links a pin to a signal.
// Equivalent to "halcmd linkps <pin> <sig>".
func LinkPS(pin, sig string) error {
	return halLinkPS(pin, sig)
}

// LinkSP links a signal to a pin (argument order reversed from LinkPS).
// Equivalent to "halcmd linksp <sig> <pin>".
func LinkSP(sig, pin string) error {
	return halLinkPS(pin, sig)
}

// UnlinkP unlinks a pin from its signal.
// Equivalent to "halcmd unlinkp <pin>".
func UnlinkP(pin string) error {
	return halUnlinkP(pin)
}

// ===== Thread function commands =====

// AddF adds a function to a HAL thread.
// pos is the position in the thread's execution order (-1 appends at end).
// Equivalent to "halcmd addf <funct> <thread> [pos]".
func AddF(funct, thread string, pos int) error {
	return halAddF(funct, thread, pos)
}

// DelF removes a function from a HAL thread.
// Equivalent to "halcmd delf <funct> <thread>".
func DelF(funct, thread string) error {
	return halDelF(funct, thread)
}

// ===== RT component management =====

// LoadRT loads a realtime HAL module (kernel module or RTAPI component).
// Additional arguments are passed to the module.
// Equivalent to "halcmd loadrt <mod> [args...]".
func LoadRT(mod string, args ...string) error {
	return halLoadRT(mod, args)
}

// UnloadRT unloads a realtime HAL module.
// Equivalent to "halcmd unloadrt <mod>".
func UnloadRT(mod string) error {
	return halUnloadRT(mod)
}

// ===== User-space component management =====

// LoadUSROptions controls how a user-space HAL component is started.
type LoadUSROptions struct {
	// WaitReady causes LoadUSR to block until the component calls hal_ready().
	WaitReady bool

	// WaitName is the component name to wait for when WaitReady is set.
	// If empty, the program name is used.
	WaitName string

	// WaitExit causes LoadUSR to block until the child process exits.
	WaitExit bool

	// NoStdin redirects the child's stdin to /dev/null.
	NoStdin bool

	// TimeoutSecs is the maximum number of seconds to wait when WaitReady
	// or WaitExit is set. 0 uses the default (10 seconds for WaitReady).
	TimeoutSecs int
}

// LoadUSR starts a user-space HAL component process.
// opts controls wait behaviour and stdin handling.
// Equivalent to "halcmd loadusr [flags] <prog> [args...]".
func LoadUSR(opts *LoadUSROptions, prog string, args ...string) error {
	var flags int
	var waitName string
	var timeout int

	if opts != nil {
		if opts.WaitReady {
			flags |= 1
		}
		if opts.WaitExit {
			flags |= 2
		}
		if opts.NoStdin {
			flags |= 4
		}
		waitName = opts.WaitName
		timeout = opts.TimeoutSecs
	}

	return halLoadUSR(flags, waitName, timeout, prog, args)
}

// UnloadUSR sends SIGTERM to the process owning the named user-space component.
// Equivalent to "halcmd unloadusr <comp>".
func UnloadUSR(comp string) error {
	return halUnloadUSR(comp)
}

// Unload unloads a HAL component by name, trying RT first then user-space.
// Equivalent to "halcmd unload <comp>".
func Unload(comp string) error {
	// Try RT unload first; if it fails, try user-space unload.
	if err := halUnloadRT(comp); err == nil {
		return nil
	}
	return halUnloadUSR(comp)
}

// WaitUSR waits until the named user-space component disappears from HAL.
// Uses a 30-second default timeout.
// Equivalent to waiting for "halcmd unloadusr <comp>" to complete.
func WaitUSR(comp string) error {
	return halWaitUSR(comp, 0)
}

// ===== Lock/unlock =====

// parseLockLevel maps a string level name to the HAL lock bitmask.
// Valid levels: "none", "load", "config", "params", "run", "tune", "all".
func parseLockLevel(level string) (int, error) {
	switch strings.ToLower(level) {
	case "none":
		return 0, nil // HAL_LOCK_NONE
	case "load":
		return 1, nil // HAL_LOCK_LOAD
	case "config":
		return 2, nil // HAL_LOCK_CONFIG
	case "params":
		return 4, nil // HAL_LOCK_PARAMS
	case "run":
		return 8, nil // HAL_LOCK_RUN
	case "tune":
		return 3, nil // HAL_LOCK_TUNE (LOAD | CONFIG)
	case "all":
		return 255, nil // HAL_LOCK_ALL
	default:
		return 0, fmt.Errorf("unknown lock level %q: valid levels are none, load, config, params, run, tune, all", level)
	}
}

// Lock sets the HAL lock level to restrict which commands are permitted.
// Equivalent to "halcmd lock <level>".
func Lock(level string) error {
	lvl, err := parseLockLevel(level)
	if err != nil {
		return err
	}
	return halSetLock(lvl)
}

// Unlock sets the HAL lock level to allow previously restricted commands.
// Call Unlock("none") to fully unlock (allow all commands).
// The level argument specifies the new lock level, same as Lock —
// lower levels mean less restriction (e.g., "none" < "tune" < "all").
// Equivalent to "halcmd unlock <level>".
func Unlock(level string) error {
	lvl, err := parseLockLevel(level)
	if err != nil {
		return err
	}
	return halSetLock(lvl)
}

// ===== Query commands =====

// List returns the names of HAL objects of the given type that match any of
// the provided glob patterns. If no patterns are given, all objects are listed.
// halType is one of: "pin", "sig", "param", "funct", "thread", "comp".
// Equivalent to "halcmd list <type> [patterns...]".
func List(halType string, patterns ...string) ([]string, error) {
	if len(patterns) == 0 {
		patterns = []string{""}
	}

	var listFn func(string) ([]string, error)
	switch strings.ToLower(halType) {
	case "pin":
		listFn = halListPins
	case "sig":
		listFn = halListSigs
	case "param":
		listFn = halListParams
	case "funct":
		listFn = halListFuncts
	case "thread":
		listFn = halListThreads
	case "comp":
		listFn = func(pat string) ([]string, error) {
			all, err := halListComponents()
			if err != nil {
				return nil, err
			}
			if pat == "" {
				return all, nil
			}
			// Filter by pattern using path.Match glob semantics.
			// Note: C shims handle patterns for pin/sig/param/funct/thread
			// via fnmatch internally; for comp we filter in Go.
			var filtered []string
			for _, name := range all {
				if matched, _ := matchPattern(pat, name); matched {
					filtered = append(filtered, name)
				}
			}
			return filtered, nil
		}
	default:
		return nil, fmt.Errorf("list: unknown type %q: valid types are pin, sig, param, funct, thread, comp", halType)
	}

	// Collect results for all patterns, deduplicating by name.
	seen := make(map[string]struct{})
	var all []string
	for _, pat := range patterns {
		names, err := listFn(pat)
		if err != nil {
			return nil, err
		}
		for _, name := range names {
			if _, ok := seen[name]; !ok {
				seen[name] = struct{}{}
				all = append(all, name)
			}
		}
	}
	return all, nil
}

// matchPattern does a glob-style match in Go for the "comp" list case.
// Returns true if name matches pat using path.Match syntax (*, ?, [...] wildcards).
// Note: this uses Go's path.Match semantics, not C fnmatch; '**' is not supported.
func matchPattern(pat, name string) (bool, error) {
	if pat == "" {
		return true, nil
	}
	// Use path.Match for glob patterns (*, ?, [...] syntax).
	return path.Match(pat, name)
}
