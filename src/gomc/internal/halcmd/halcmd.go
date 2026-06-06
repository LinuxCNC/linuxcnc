package halcmd

import (
	"fmt"
	"log/slog"
	"os"
	"path"
	"strings"
	"unsafe"

	hal "github.com/sittner/linuxcnc/src/gomc/pkg/hal"
)

// LogLevel is the dynamic log level variable that controls which messages are
// emitted by the slog handler.  It is set at startup by gomc-server and can be
// changed at runtime via SetDebug / "halcmd debug <level>".
// Levels follow gomc_log_level_t: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR.
var LogLevel slog.LevelVar

// lockAll is the HAL_LOCK_ALL bitmask value.
const lockAll = 255

// SetLock sets the HAL lock level, restricting which commands are permitted.
// This is the low-level counterpart of Lock()/Unlock() and exists so that the
// halparse executor can set lock levels directly via an integer bitmask.
func SetLock(level int) error {
	return halSetLock(level)
}

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

// CreateThreadCPU creates a single HAL realtime thread with CPU affinity.
//
// When cpu=-1, the next available isolated CPU is automatically assigned from
// the pool initialized by InitCPUPool().  If the pool is exhausted the thread
// runs without affinity (a warning is logged in POSIX RT mode).
//
// When cpu>=0, the value is validated against the pool of isolated physical
// cores. An error is returned if the CPU is not available.
//
// Threads must be created fastest-first (ascending period) for rate monotonic
// priority scheduling.
func CreateThreadCPU(name string, periodNs int64, usesFP int, cpu int) error {
	assigned, err := acquireCPU(name, cpu)
	if err != nil {
		return err
	}
	return halCreateThreadCPU(name, periodNs, usesFP, assigned)
}

// ThreadDelete deletes a HAL realtime thread by name.
// The thread must have been stopped (via StopThreads) before deletion.
func ThreadDelete(name string) error {
	return halThreadDelete(name)
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

// DelFunctsByComp removes all functions owned by comp_id from all threads.
// Returns the number of functions removed.
func DelFunctsByComp(compID int) (int, error) {
	return halDelFunctsByComp(compID)
}

// WaitCycleAdvance waits until all threads have advanced their cycle counter
// past the given baseline.  Returns error on timeout (100ms).
func WaitCycleAdvance(baseline uint32) error {
	return halWaitCycleAdvance(baseline)
}

// GetMaxCycleCount returns the current maximum cycle count across all threads.
func GetMaxCycleCount() uint32 {
	return halGetMaxCycleCount()
}

// FindCompID returns the comp_id for a named HAL component, or 0 if not found.
func FindCompID(name string) int {
	return halFindCompID(name)
}

// LockDLHandle locks the PT_LOAD segments of a single dlopen handle
// into memory, preventing page faults during RT execution.
func LockDLHandle(handle unsafe.Pointer) {
	halLockDLHandle(handle)
}

// UnlockDLHandle unlocks the PT_LOAD segments of a single dlopen handle.
func UnlockDLHandle(handle unsafe.Pointer) {
	halUnlockDLHandle(handle)
}

// NewInst creates a new instance of a HAL component type.
// Equivalent to "halcmd newinst <type> <name> [arg]".
func NewInst(compType, name, arg string) error {
	return halNewInst(compType, name, arg)
}

// RtapiInitializeApp initializes the RTAPI application environment.
// It sets up RT rlimits, calls mlockall(MCL_CURRENT) to lock currently-mapped
// pages (libc, librtapi, vdso, and initial Go runtime pages), installs signal
// handlers, and grants I/O privileges.  The function is idempotent: subsequent
// calls return immediately.
//
// This must be called as early as possible — before any HAL or component
// initialization — so that the locked page set is minimal and all RT privileges
// are in place before any RT-sensitive code runs.
func RtapiInitializeApp() {
	halRtapiInitializeApp()
}

// SetLogRing sets the gomc_log ring for the RTAPI message handler.
// Must be called before RtapiAppInit().
func SetLogRing(ring unsafe.Pointer) {
	halSetLogRing(ring)
}

// ClearMsgHandler disconnects the RTAPI message handler so that
// subsequent rtapi_print_msg calls are silently discarded.  Call
// before destroying the log ring.
func ClearMsgHandler() {
	halClearMsgHandler()
}

// RtapiAppInit initializes the in-process RTAPI/HAL environment.
// Sets up HAL shared memory.
// Must be called before hal_init() / hal.NewComponent().
func RtapiAppInit() error {
	return halRtapiAppInit()
}

// RtapiAppCleanup shuts down the in-process RTAPI/HAL environment.
// Tears down HAL threads and releases shared memory.
// Must be called after all components are unloaded.
func RtapiAppCleanup() {
	halRtapiAppCleanup()
}

// ===== Signal commands =====

// NewSig creates a new HAL signal with the given name and type.
// Equivalent to "halcmd newsig <name> <type>".
func NewSig(name string, halType hal.PinType) error {
	return halNewSig(name, halType)
}

// DelSig deletes a HAL signal by name.
// Equivalent to "halcmd delsig <name>".
func DelSig(name string) error {
	return halDelSig(name)
}

// Retain sets the HAL_SIGFLAG_RETAIN flag on a signal.
// The signal must not have any output writers.
// Equivalent to "halcmd retain <name>".
func Retain(name string) error {
	return halRetain(name)
}

// Unretain clears the HAL_SIGFLAG_RETAIN flag on a signal.
// Equivalent to "halcmd unretain <name>".
func Unretain(name string) error {
	return halUnretain(name)
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
func SType(name string) (hal.PinType, error) {
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
func PType(name string) (hal.PinType, error) {
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
// If no pins remain after filtering, Net returns nil (matching halcmd behaviour
// which allows "net signame" with no pins to check/display a signal's status).
// Equivalent to "halcmd net <signame> [pins...]".
func Net(signame string, pins ...string) error {
	filtered := removeArrows(pins)
	if len(filtered) == 0 {
		// No pins specified — no-op, matching halcmd which allows this.
		return nil
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

// ===== Alias commands =====

// Alias creates an alternate name for a HAL pin or parameter.
// kind is "pin" or "param", name is the real name, alias is the alternate name.
// Equivalent to "halcmd alias <kind> <name> <alias>".
func Alias(kind, name, alias string) error {
	return halAlias(kind, name, alias)
}

// UnAlias removes an alias from a HAL pin or parameter.
// kind is "pin" or "param", name is the real or aliased name.
// Equivalent to "halcmd unalias <kind> <name>".
func UnAlias(kind, name string) error {
	return halUnAlias(kind, name)
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
// The level argument names the bits to clear: "unlock all" removes all lock
// bits (resulting in LockNone=0), "unlock tune" removes the tune bits, etc.
// This mirrors the halcmd semantics: unlock sets the lock mask to the
// complement of the named level (lockAll &^ lvl).
// Equivalent to "halcmd unlock <level>".
func Unlock(level string) error {
	lvl, err := parseLockLevel(level)
	if err != nil {
		return err
	}
	return halSetLock(lockAll &^ lvl)
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
	case "retain":
		listFn = halListRetainSigs
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
		return nil, fmt.Errorf("list: unknown type %q: valid types are pin, sig, param, funct, thread, comp, retain", halType)
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
func matchPattern(pat, name string) (bool, error) {
	if pat == "" {
		return true, nil
	}
	return path.Match(pat, name)
}

// ===== Structured info types =====

// CompInfo holds the attributes of a HAL component.
type CompInfo struct {
	Name string `json:"name"`
	ID   int    `json:"id"`
	Type string `json:"type"`
}

// PinInfo holds all attributes of a HAL pin.
type PinInfo struct {
	Name      string `json:"name"`
	Type      string `json:"type"`
	Direction string `json:"direction"`
	Value     string `json:"value"`
	Signal    string `json:"signal,omitempty"`
	Owner     string `json:"owner"`
	HasWriter bool   `json:"has_writer"`
}

// ParamInfo holds all attributes of a HAL parameter.
type ParamInfo struct {
	Name      string `json:"name"`
	Type      string `json:"type"`
	Direction string `json:"direction"`
	Value     string `json:"value"`
	Owner     string `json:"owner"`
}

// SigInfo holds all attributes of a HAL signal.
type SigInfo struct {
	Name  string `json:"name"`
	Type  string `json:"type"`
	Value string `json:"value"`
}

// FunctInfo holds all attributes of a HAL realtime function.
type FunctInfo struct {
	Name    string `json:"name"`
	Owner   string `json:"owner"`
	Users   int32  `json:"users"`
	FP      bool   `json:"fp"`
	MaxTime int64  `json:"maxtime_ns"`
}

// ThreadInfo holds all attributes of a HAL thread.
type ThreadInfo struct {
	Name    string   `json:"name"`
	Period  int64    `json:"period_ns"`
	FP      bool     `json:"fp"`
	Functs  []string `json:"functs"`
	Running bool     `json:"running"`
}

// ShowResult aggregates the results of a Show() call.
type ShowResult struct {
	Comps   []CompInfo   `json:"comps,omitempty"`
	Pins    []PinInfo    `json:"pins,omitempty"`
	Params  []ParamInfo  `json:"params,omitempty"`
	Signals []SigInfo    `json:"signals,omitempty"`
	Functs  []FunctInfo  `json:"functs,omitempty"`
	Threads []ThreadInfo `json:"threads,omitempty"`
}

// StatusInfo holds HAL shared-memory status information.
type StatusInfo struct {
	ShmemFree int    `json:"shmem_free_bytes"`
	LockLevel string `json:"lock_level"`
}

// ===== Show / Save / Status / SetDebug =====

// Show returns structured information about HAL objects of the given type
// that match any of the provided glob patterns.
// halType can be "all", "comp", "pin", "param", "sig", "funct", or "thread".
// If no patterns are given, all objects of that type are returned.
// Equivalent to "halcmd show <type> [patterns...]".
func Show(halType string, patterns ...string) (*ShowResult, error) {
	if len(patterns) == 0 {
		patterns = []string{""}
	}

	result := &ShowResult{}

	showComps := func(pat string) error {
		items, err := halShowComps(pat)
		if err != nil {
			return err
		}
		result.Comps = append(result.Comps, items...)
		return nil
	}
	showPins := func(pat string) error {
		items, err := halShowPins(pat)
		if err != nil {
			return err
		}
		result.Pins = append(result.Pins, items...)
		return nil
	}
	showParams := func(pat string) error {
		items, err := halShowParams(pat)
		if err != nil {
			return err
		}
		result.Params = append(result.Params, items...)
		return nil
	}
	showSigs := func(pat string) error {
		items, err := halShowSigs(pat)
		if err != nil {
			return err
		}
		result.Signals = append(result.Signals, items...)
		return nil
	}
	showFuncts := func(pat string) error {
		items, err := halShowFuncts(pat)
		if err != nil {
			return err
		}
		result.Functs = append(result.Functs, items...)
		return nil
	}
	showThreads := func(pat string) error {
		items, err := halShowThreads(pat)
		if err != nil {
			return err
		}
		result.Threads = append(result.Threads, items...)
		return nil
	}

	var showFns []func(string) error
	switch strings.ToLower(halType) {
	case "all", "":
		showFns = []func(string) error{showComps, showPins, showParams, showSigs, showFuncts, showThreads}
	case "comp":
		showFns = []func(string) error{showComps}
	case "pin":
		showFns = []func(string) error{showPins}
	case "param":
		showFns = []func(string) error{showParams}
	case "sig", "signal":
		showFns = []func(string) error{showSigs}
	case "funct", "function":
		showFns = []func(string) error{showFuncts}
	case "thread":
		showFns = []func(string) error{showThreads}
	default:
		return nil, fmt.Errorf("show: unknown type %q: valid types are all, comp, pin, param, sig, funct, thread", halType)
	}

	for _, fn := range showFns {
		for _, pat := range patterns {
			if err := fn(pat); err != nil {
				return nil, err
			}
		}
	}
	return result, nil
}

// Save serializes the current HAL state as halcmd commands.
// halType selects what to save: "all", "allu", "comp", "alias", "sig",
// "signal", "sigu", "link", "linka", "net", "neta", "netl", "netla",
// "netal", "param", "parameter", or "thread".
// If filename is non-empty the output is written to that file; otherwise
// the lines are returned as a slice.
// Equivalent to "halcmd save [type] [filename]".
func Save(halType string, filename string) ([]string, error) {
	if halType == "" {
		halType = "all"
	}
	lines, err := halSave(halType)
	if err != nil {
		return nil, err
	}
	if filename != "" {
		f, err := os.Create(filename)
		if err != nil {
			return nil, fmt.Errorf("save: cannot open %q: %w", filename, err)
		}
		defer f.Close()
		for _, line := range lines {
			if _, err := fmt.Fprintln(f, line); err != nil {
				return nil, fmt.Errorf("save: write error: %w", err)
			}
		}
		return nil, nil
	}
	return lines, nil
}

// Status returns a summary of HAL shared-memory usage and lock state.
// Equivalent to "halcmd status".
func Status() (*StatusInfo, error) {
	return halStatus()
}

// SetDebug sets the log output verbosity level.
// Levels follow gomc_log_level_t: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR.
// Equivalent to "halcmd debug <level>".
func SetDebug(level int) error {
	switch level {
	case 0:
		LogLevel.Set(slog.LevelDebug)
	case 1:
		LogLevel.Set(slog.LevelInfo)
	case 2:
		LogLevel.Set(slog.LevelWarn)
	case 3:
		LogLevel.Set(slog.LevelError)
	default:
		return fmt.Errorf("invalid debug level %d (valid: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR)", level)
	}
	return nil
}
