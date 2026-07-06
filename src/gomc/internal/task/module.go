// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"fmt"
	"log/slog"
	"os"
	"runtime/cgo"
	"strconv"
	"strings"
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emccmd"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emcerror"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emcio"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emcstat"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/motctl"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/motstat"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/tooltable"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

func init() {
	gomc.RegisterModule("milltask", factory)
}

// Compile-time interface checks.
var _ MotionConfig = (*motctl.MotctlClient)(nil)

func factory(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	logger = logger.With("module", name)
	// Use a namespaced view of the INI so this instance reads [name:SECTION]
	// with fallback to [SECTION].  For the default "milltask" instance name
	// where no namespaced sections exist, all lookups fall through to global.
	nsIni := ini.WithNamespace(name)
	m := &milltaskModule{ini: nsIni, logger: logger, name: name}

	// Parse module parameters.
	for _, arg := range args {
		k, v, ok := strings.Cut(arg, "=")
		if !ok {
			continue
		}
		switch k {
		case "halui":
			m.haluiPrefix = v
		case "motion_instance":
			m.motInstance = v
		case "iocontrol_instance":
			m.ioInstance = v
		case "tooltable_instance":
			m.ttInstance = v
		case "persist_instance":
			m.persistInstance = v
		}
	}

	// Register C-compatible callback structs so C modules (halui) can
	// call emccmd/emcstat via the standard api_get mechanism.
	reg := apiserver.DefaultRegistry()
	if reg == nil {
		return nil, fmt.Errorf("milltask: no API registry available")
	}
	if err := emccmd.RegisterEmccmdAPI(reg, name, m); err != nil {
		return nil, fmt.Errorf("milltask: emccmd register: %w", err)
	}
	if err := emcstat.RegisterEmcstatAPI(reg, name, m); err != nil {
		return nil, fmt.Errorf("milltask: emcstat register: %w", err)
	}
	m.apiCleanup = func() {
		if ptr, err := reg.GetAPIUntracked("emccmd", name, 0); err == nil {
			emccmd.FreeEmccmdCallbacks(ptr)
		}
		if ptr, err := reg.GetAPIUntracked("emcstat", name, 0); err == nil {
			emcstat.FreeEmcstatCallbacks(ptr)
		}
	}

	// Create halui HAL component if halui=<prefix> module parameter was given.
	// Done in factory so pins exist before HAL wiring commands execute.
	if m.haluiPrefix != "" {
		numJoints := getIntOr(ini, "KINS", "JOINTS", 3)
		numSpindles := getIntOr(ini, "TRAJ", "SPINDLES", 1)
		coord := ini.Get("TRAJ", "COORDINATES")
		axisMask := parseAxisMask(coord)
		mdiCmds := ini.GetAll("HALUI", "MDI_COMMAND")
		hu, err := newHalUI(m.haluiPrefix, numJoints, numSpindles, axisMask, mdiCmds)
		if err != nil {
			return nil, fmt.Errorf("milltask: %w", err)
		}
		m.halui = hu
		logger.Info("halui pins exported", "prefix", m.haluiPrefix)
	}

	// Register WebSocket watches and commands (direct Go path, no C thunk).
	m.registerWatches(name)

	return m, nil
}

// milltaskModule wraps Task to satisfy the gomc.Module lifecycle.
type milltaskModule struct {
	ini               *inifile.IniFile
	name              string
	task              *Task
	logger            *slog.Logger
	inihal            *iniHal
	mc                MotionConfig
	apiCleanup        func()
	poslog            posLogger
	interp            *CInterp
	canonTable        *canonCallbackTable
	mon               *monitor
	stopped           bool
	haluiPrefix       string                     // if set, export halui pins with this component name
	halui             *halUI                     // halui HAL component (created in factory)
	motInstance       string                     // motion module instance name (default "motmod")
	ioInstance        string                     // io controller instance name (default "iocontrol")
	ttInstance        string                     // tooltable instance name (default "tooltable")
	persistInstance   string                     // persist instance name (default "persistence")
	iniAccessorHandle cgo.Handle                 // CGo handle for the INI accessor (must be freed)
	ttClient          *tooltable.TooltableClient // tooltable GMI client
	paramIO           *interpParamIOPersist      // persist-backed parameter I/O (nil = file-based)
}

func (m *milltaskModule) Start() error {
	reg := apiserver.DefaultRegistry()
	if reg == nil {
		return fmt.Errorf("milltask: no API registry available")
	}

	// Determine motion module instance name from INI (default "motmod").
	motInstance := m.motInstance
	if motInstance == "" {
		motInstance = "motmod"
	}

	// Determine IO controller instance name.
	ioInstance := m.ioInstance
	if ioInstance == "" {
		ioInstance = "iocontrol"
	}

	// Look up registered GMI callbacks.
	motctlCbs, err := reg.GetAPIFor(m.name, "motctl", motInstance, 1)
	if err != nil {
		return fmt.Errorf("milltask: motctl API lookup (%s): %w", motInstance, err)
	}
	motstatCbs, err := reg.GetAPIFor(m.name, "motstat", motInstance, 1)
	if err != nil {
		return fmt.Errorf("milltask: motstat API lookup (%s): %w", motInstance, err)
	}
	emcioCbs, err := reg.GetAPIFor(m.name, "emcio", ioInstance, 1)
	if err != nil {
		return fmt.Errorf("milltask: emcio API lookup (%s): %w", ioInstance, err)
	}

	// Look up tooltable API.
	ttInstance := m.ttInstance
	if ttInstance == "" {
		ttInstance = "tooltable"
	}
	ttCbs, err := reg.GetAPIFor(m.name, "tooltable", ttInstance, 1)
	if err != nil {
		return fmt.Errorf("milltask: tooltable API lookup (%s): %w", ttInstance, err)
	}
	m.ttClient = tooltable.NewTooltableClient(unsafe.Pointer(ttCbs))

	// Wrap C callback pointers in typed Go clients.
	mc := motctl.NewMotctlClient(unsafe.Pointer(motctlCbs))
	ms := motstat.NewMotstatClient(unsafe.Pointer(motstatCbs))
	ioClient := emcio.NewEmcioClient(unsafe.Pointer(emcioCbs))
	io := &ioAdapter{EmcioClient: ioClient}

	t := NewTask(mc, io, ms, m.logger)
	t.SetIOStatusReader(io)

	// Validate kinematics/joint/axis INI consistency before loading config.
	if err := m.checkConfig(); err != nil {
		return fmt.Errorf("milltask: %w", err)
	}

	// Load configuration from INI and send to motion controller.
	if err := loadConfig(m.ini, t, mc); err != nil {
		return fmt.Errorf("milltask: %w", err)
	}

	// Create inihal HAL component for runtime INI parameter override.
	ih, err := newIniHal(m.name+".inihal", t.numJoints)
	if err != nil {
		return fmt.Errorf("milltask: %w", err)
	}
	ih.initPins(t)

	m.task = t
	m.inihal = ih
	m.mc = mc

	// Wire the error publisher so operator messages reach UI clients.
	// EnsureDrainStarted creates the ring+drain if the C milltask didn't.
	if drain := emcerror.EnsureDrainStarted(m.name); drain != nil {
		t.SetErrorPublisher(&drainErrorPublisher{drain: drain})
	}

	// Forward ERROR-level log messages from motion-related modules to the
	// operator message list. This covers motmod, per-joint homemod instances
	// (regardless of their configured name), and any future servo-thread
	// modules that use gomc_log_errorf.
	gomc.OnLogError(func(component, msg string) {
		t.operatorError(msg)
	})

	// Create and configure the G-code interpreter.
	if err := m.initInterpreter(); err != nil {
		return fmt.Errorf("milltask: %w", err)
	}

	// Start the sequencer goroutine (executes queued motion commands).
	t.StartSequencer()

	// Start the monitoring goroutine (estop, errors, soft limits, inihal, halui).
	m.mon = newMonitor(t, mc, ih, io)
	m.mon.halui = m.halui
	m.mon.start()

	// Register tools API (needs INI for tool table path).
	m.registerTools()

	// Load default program if configured.
	m.loadDefaultProgram()

	m.logger.Info("milltask started")
	return nil
}

func (m *milltaskModule) Stop() {
	m.stopped = true
	if m.mon != nil {
		m.mon.stop()
	}
	m.poslog.stopLogger()
	if m.task != nil {
		m.task.StopSequencer()
		if m.task.mcode != nil {
			m.task.mcode.Stop()
		}
	}
	m.logger.Info("milltask stopping")
}

func (m *milltaskModule) Destroy() {
	if m.interp != nil {
		m.interp.Destroy()
		m.interp = nil
	}
	if m.paramIO != nil {
		m.paramIO.destroy()
		m.paramIO = nil
	}
	if m.iniAccessorHandle != 0 {
		FreeIniAccessor(m.iniAccessorHandle)
		m.iniAccessorHandle = 0
	}
	if m.canonTable != nil {
		m.canonTable.release()
		m.canonTable = nil
	}
	if m.inihal != nil {
		m.inihal.exit()
	}
	if m.mon != nil && m.mon.halui != nil {
		m.mon.halui.exit()
	}
	if m.apiCleanup != nil {
		m.apiCleanup()
	}
	m.logger.Info("milltask destroyed")
}

// initInterpreter creates and configures the G-code interpreter.
func (m *milltaskModule) initInterpreter() error {
	// Determine interpreter library (default: built-in rs274ngc).
	interpLib := m.ini.Get("TASK", "RS274NGC_STARTUP_CODE")
	_ = interpLib // not used for library selection

	interp, err := NewCInterp()
	if err != nil {
		return fmt.Errorf("creating interpreter: %w", err)
	}

	// Set up canon callbacks so interpreter actions flow to the sequencer.
	ct := newCanonCallbackTable(m.task.canon)
	interp.SetCanonCallbacks(ct.ptr())

	// Load INI configuration into interpreter via accessor callbacks.
	// This replaces interp.IniLoad(filename) — the accessor provides
	// namespace-resolved values from the Go INI parser, enabling
	// multi-instance support without the interpreter opening files.
	accHandle, err := interp.IniLoadAccessor(m.ini)
	if err != nil {
		interp.Destroy()
		ct.release()
		return fmt.Errorf("interpreter ini_load_accessor: %w", err)
	}
	m.iniAccessorHandle = accHandle

	// Ensure canon knows the parameter file name so the interpreter can
	// load it during Init(). IniLoad should set this via the callback,
	// but we also set it explicitly as a safety net.
	if pf := m.ini.Get("RS274NGC", "PARAMETER_FILE"); pf != "" {
		m.task.canon.SetParameterFileName(pf)
	}

	// Mark interpreter as running in task mode (not preview mode).
	// This enables save_parameters to actually write the var file.
	interp.SetTaskMode(1)

	// Set up persist-backed parameter I/O (required).
	persistInstance := m.persistInstance
	if persistInstance == "" {
		persistInstance = "persistence"
	}
	reg := apiserver.DefaultRegistry()
	persistCbs, err := reg.GetAPIFor(m.name, "persist", persistInstance, 2)
	if err != nil {
		interp.Destroy()
		ct.release()
		return fmt.Errorf("interpreter: persist API lookup (%s): %w", persistInstance, err)
	}
	m.paramIO = newInterpParamIOPersist(persistCbs)
	m.paramIO.install(interp)

	// Initialize interpreter state.
	if err := interp.Init(); err != nil {
		interp.Destroy()
		ct.release()
		m.paramIO.destroy()
		m.paramIO = nil
		return fmt.Errorf("interpreter init: %w", err)
	}

	m.interp = interp
	m.canonTable = ct
	m.task.SetInterpreter(interp)

	// Register M-code trampoline for all M100-M199 slots so the interpreter
	// calls back into Go when user-defined M-codes are encountered.
	interp.RegisterAllMcodeSlots()

	// Synch interpreter state with motion and populate active G/M codes.
	// The C milltask calls emcTaskPlanSynch() at startup which does interp.synch(),
	// then the stat update calls active_g_codes/m_codes/settings.
	if err := interp.Synch(); err != nil {
		m.logger.Warn("interpreter initial synch failed (no motion?)", "err", err)
	}
	m.task.updateActiveCodes(interp)

	m.logger.Info("interpreter initialized")
	return nil
}

func getIntOr(ini *inifile.IniFile, section, key string, def int) int {
	s := ini.Get(section, key)
	if s == "" {
		return def
	}
	v, err := strconv.Atoi(s)
	if err != nil {
		return def
	}
	return v
}

// ioAdapter wraps the generated EmcioClient to satisfy IOController interface.
type ioAdapter struct {
	*emcio.EmcioClient
}

// GetCmdStatus returns the IO command status (1=DONE, 2=EXEC, 3=ERROR).
func (a *ioAdapter) GetCmdStatus() (int32, error) {
	st, err := a.EmcioClient.GetStatus()
	if err != nil {
		return 0, err
	}
	return int32(st.Status), nil
}

// GetIOFullStatus returns the full IO status for the monitor.
func (a *ioAdapter) GetIOFullStatus() (IOFullStatus, error) {
	st, err := a.EmcioClient.GetStatus()
	if err != nil {
		return IOFullStatus{}, err
	}
	return IOFullStatus{
		Estop:  st.Estop,
		Status: int32(st.Status),
		Reason: st.Reason,
	}, nil
}

// GetToolInSpindle returns the current tool number loaded in the spindle.
func (a *ioAdapter) GetToolInSpindle() (int32, error) {
	st, err := a.EmcioClient.GetStatus()
	if err != nil {
		return 0, err
	}
	return st.Tool.ToolInSpindle, nil
}

// GetPocketPrepped returns the pocket number prepared for next tool change.
func (a *ioAdapter) GetPocketPrepped() (int32, error) {
	st, err := a.EmcioClient.GetStatus()
	if err != nil {
		return 0, err
	}
	return st.Tool.PocketPrepped, nil
}

// drainErrorPublisher implements ErrorPublisher by writing to the emcerror drain.
// The message list append is handled by the caller (operatorError, etc.).
type drainErrorPublisher struct {
	drain *emcerror.PublishErrorDrain
}

func (p *drainErrorPublisher) OperatorError(text string) {
	p.drain.PublishError(emcerror.ErrorKind_OPERATOR_ERROR, text)
}

func (p *drainErrorPublisher) OperatorText(text string) {
	p.drain.PublishError(emcerror.ErrorKind_OPERATOR_TEXT, text)
}

func (p *drainErrorPublisher) OperatorDisplay(text string) {
	p.drain.PublishError(emcerror.ErrorKind_OPERATOR_DISPLAY, text)
}

// loadDefaultProgram opens the program specified by [DISPLAY]OPEN_FILE
// at server startup so all UI clients see the same initial file.
func (m *milltaskModule) loadDefaultProgram() {
	file := m.ini.Get("DISPLAY", "OPEN_FILE")
	if file == "" {
		return
	}
	if _, err := os.Stat(file); err != nil {
		m.logger.Warn("OPEN_FILE not found, skipping", "file", file, "error", err)
		return
	}
	if err := m.task.ProgramOpen(file); err != nil {
		m.logger.Warn("failed to load default program", "file", file, "error", err)
	} else {
		m.logger.Info("loaded default program", "file", file)
	}
}

// checkConfig validates kinematics/joint/axis INI consistency.
// This is the consumer-side validation for settings that milltask reads
// from [KINS], [TRAJ], [JOINT_*], and [AXIS_*] sections.
func (m *milltaskModule) checkConfig() error {
	result, err := runConfigCheck(m.ini)
	if err != nil {
		return fmt.Errorf("config check: %w", err)
	}
	if w := result.formatWarnings(); w != "" {
		m.logger.Warn(w)
	}
	if result.hasErrors() {
		return fmt.Errorf("config check failed:\n%s", result.formatErrors())
	}
	return nil
}
