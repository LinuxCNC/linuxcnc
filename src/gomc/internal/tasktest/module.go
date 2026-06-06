// Package tasktest is an integration test module that loads alongside milltask
// and exercises the emccmd/emcstat APIs. It validates that state transitions,
// mode switches, and command execution produce the expected observable state.
//
// Usage in a HAL file (must appear AFTER milltask):
//
//	load tasktest
//
// The module runs its test suite in Start(), prints results, and returns an
// error if any test fails (which causes the launcher to abort).
package tasktest

import (
	"encoding/json"
	"fmt"
	"log/slog"
	"os"
	"syscall"
	"time"
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emccmd"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emcstat"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

func init() {
	gomc.RegisterModule("tasktest", factory)
}

func factory(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	return &testModule{logger: logger.With("module", "tasktest")}, nil
}

type testModule struct {
	logger *slog.Logger
	done   chan error
}

func (m *testModule) Start() error {
	reg := apiserver.DefaultRegistry()
	if reg == nil {
		return fmt.Errorf("tasktest: no API registry")
	}

	// Look up emcstat API for status queries (both C and Go milltask register this).
	emcstatCbs, err := reg.GetAPI("emcstat", "milltask", 1)
	if err != nil {
		return fmt.Errorf("tasktest: emcstat lookup: %w", err)
	}

	// For emccmd: prefer WatchRegistry (Go milltask, proper error propagation),
	// fall back to C dispatch (C milltask).
	wreg := apiserver.DefaultWatchRegistry()
	var emccmdCbs unsafe.Pointer
	if wreg == nil || wreg.Get("emccmd", "milltask") == nil {
		// C milltask path: use registry dispatch
		emccmdCbs, err = reg.GetAPI("emccmd", "milltask", 1)
		if err != nil {
			return fmt.Errorf("tasktest: emccmd lookup: %w", err)
		}
	}

	h := &testHarness{
		emccmdCbs:  emccmdCbs,
		emcstatCbs: emcstatCbs,
		wreg:       wreg,
		logger:     m.logger,
	}

	// Run tests in a goroutine because the C milltask's Start() is called
	// AFTER Go module Start() returns. We need to wait for milltask to be
	// ready before exercising it.
	m.done = make(chan error, 1)
	go func() {
		// Wait for milltask to be ready (getStat returns valid data with
		// a non-zero state indicating the task loop is running).
		deadline := time.Now().Add(5 * time.Second)
		for time.Now().Before(deadline) {
			stat, err := h.getStat()
			if err == nil && stat.Task.State > 0 {
				break
			}
			time.Sleep(50 * time.Millisecond)
		}

		results := h.runAll()
		m.done <- results.report(m.logger)
		// Signal the launcher to shut down after tests complete.
		syscall.Kill(os.Getpid(), syscall.SIGTERM)
	}()
	return nil
}

// Wait blocks until tests complete and returns the result.
func (m *testModule) Wait() error {
	if m.done == nil {
		return nil
	}
	return <-m.done
}

func (m *testModule) Stop() {
	// Wait for tests to finish (with timeout to avoid deadlock).
	if m.done != nil {
		select {
		case err := <-m.done:
			if err != nil {
				m.logger.Error("tasktest failed", "error", err)
			} else {
				m.logger.Info("tasktest: all tests passed")
			}
		case <-time.After(5 * time.Second):
			m.logger.Error("tasktest: Stop() timed out waiting for tests")
		}
	}
}
func (m *testModule) Destroy() {}

// testHarness provides helpers for calling emccmd/emcstat.
type testHarness struct {
	emccmdCbs  unsafe.Pointer         // C milltask dispatch (nil for Go milltask)
	emcstatCbs unsafe.Pointer
	wreg       *apiserver.WatchRegistry // Go milltask commands (nil for C milltask)
	logger     *slog.Logger
}

// --- emccmd call helpers ---

func (h *testHarness) setState(state int32) (int32, error) {
	return h.callEmccmd("set_state", map[string]interface{}{"state": state})
}

func (h *testHarness) setMode(mode int32) (int32, error) {
	return h.callEmccmd("set_mode", map[string]interface{}{"mode": mode})
}

func (h *testHarness) autoCmd(cmd int32, line int32) (int32, error) {
	return h.callEmccmd("auto_cmd", map[string]interface{}{"cmd": cmd, "line": line})
}

func (h *testHarness) programOpen(file string) (int32, error) {
	return h.callEmccmd("program_open", map[string]interface{}{"file": file})
}

func (h *testHarness) home(joint int32) (int32, error) {
	return h.callEmccmd("home", map[string]interface{}{"joint": joint})
}

func (h *testHarness) abort() (int32, error) {
	return h.callEmccmd("abort", nil)
}

func (h *testHarness) jog(jogType, axisOrJoint int32, velocity, distance float64) (int32, error) {
	return h.callEmccmd("jog", map[string]interface{}{
		"jog_type":      jogType,
		"jjogmode":      false,
		"axis_or_joint": axisOrJoint,
		"velocity":      velocity,
		"distance":      distance,
	})
}

func (h *testHarness) flood(on bool) (int32, error) {
	return h.callEmccmd("flood", map[string]interface{}{"on": on})
}

func (h *testHarness) mist(on bool) (int32, error) {
	return h.callEmccmd("mist", map[string]interface{}{"on": on})
}

func (h *testHarness) spindle(cmd int32, speed float64) (int32, error) {
	return h.callEmccmd("spindle", map[string]interface{}{
		"cmd":         cmd,
		"speed":       speed,
		"spindle_num": int32(0),
		"wait":        int32(0),
	})
}

func (h *testHarness) setFeedOverride(rate float64) (int32, error) {
	return h.callEmccmd("set_feed_override", map[string]interface{}{"rate": rate})
}

func (h *testHarness) setRapidOverride(rate float64) (int32, error) {
	return h.callEmccmd("set_rapid_override", map[string]interface{}{"rate": rate})
}

func (h *testHarness) setSpindleOverride(rate float64, spindle int32) (int32, error) {
	return h.callEmccmd("set_spindle_override", map[string]interface{}{"rate": rate, "spindle_num": spindle})
}

func (h *testHarness) setMaxVelocity(vel float64) (int32, error) {
	return h.callEmccmd("set_max_velocity", map[string]interface{}{"velocity": vel})
}

func (h *testHarness) jogStop(axisOrJoint int32) (int32, error) {
	return h.callEmccmd("jog_stop", map[string]interface{}{
		"jjogmode":      false,
		"axis_or_joint": axisOrJoint,
	})
}

func (h *testHarness) unhome(joint int32) (int32, error) {
	return h.callEmccmd("unhome", map[string]interface{}{"joint": joint})
}

func (h *testHarness) overrideLimits(joint int32) (int32, error) {
	return h.callEmccmd("override_limits", map[string]interface{}{"joint": joint})
}

func (h *testHarness) teleopEnable(enable bool) (int32, error) {
	return h.callEmccmd("teleop_enable", map[string]interface{}{"enable": enable})
}

func (h *testHarness) setOptionalStop(on bool) (int32, error) {
	return h.callEmccmd("set_optional_stop", map[string]interface{}{"on": on})
}

func (h *testHarness) setBlockDelete(on bool) (int32, error) {
	return h.callEmccmd("set_block_delete", map[string]interface{}{"on": on})
}

func (h *testHarness) lube(on bool) (int32, error) {
	return h.callEmccmd("lube", map[string]interface{}{"on": on})
}

func (h *testHarness) mdi(command string) (int32, error) {
	return h.callEmccmd("mdi", map[string]interface{}{"command": command})
}

func (h *testHarness) taskPlanSynch() (int32, error) {
	return h.callEmccmd("task_plan_synch", nil)
}

func (h *testHarness) loadToolTable(file string) (int32, error) {
	return h.callEmccmd("load_tool_table", map[string]interface{}{"file": file})
}

func (h *testHarness) waitComplete(timeout float64) (int32, error) {
	return h.callEmccmd("wait_complete", map[string]interface{}{"timeout": timeout})
}

// callEmccmd dispatches an emccmd method by name.
// Uses WatchRegistry for Go milltask (proper Go dispatch), C dispatch for C milltask.
func (h *testHarness) callEmccmd(method string, params map[string]interface{}) (int32, error) {
	var reqJSON []byte
	if params != nil {
		var err error
		reqJSON, err = json.Marshal(params)
		if err != nil {
			return -1, fmt.Errorf("marshal params: %w", err)
		}
	}

	// Try WatchRegistry path first (Go milltask).
	if h.wreg != nil {
		api := h.wreg.Get("emccmd", "milltask")
		if api != nil {
			for _, cmd := range api.Commands {
				if cmd.Name == method {
					resp, err := cmd.Handler(json.RawMessage(reqJSON))
					if err != nil {
						return -1, err
					}
					var result int32
					if err := json.Unmarshal(resp, &result); err != nil {
						return -1, fmt.Errorf("unmarshal result: %w", err)
					}
					return result, nil
				}
			}
			return -1, fmt.Errorf("emccmd command %q not found in watch registry", method)
		}
	}

	// Fall back to C dispatch path (C milltask).
	var dispatch apiserver.DispatchFunc
	for _, fn := range emccmd.EmccmdMeta.Funcs {
		if fn.Name == method {
			dispatch = fn.Dispatch
			break
		}
	}
	if dispatch == nil {
		return -1, fmt.Errorf("emccmd method %q not found in meta", method)
	}

	resp, err := dispatch(h.emccmdCbs, reqJSON)
	if err != nil {
		return -1, err
	}

	var result int32
	if err := json.Unmarshal(resp, &result); err != nil {
		return -1, fmt.Errorf("unmarshal result: %w", err)
	}
	return result, nil
}

// getStat calls emcstat.get_stat and returns the full status.
func (h *testHarness) getStat() (*emcstat.StatFull, error) {
	dispatch := emcstat.EmcstatMeta.Funcs[0].Dispatch // get_stat is the only function
	resp, err := dispatch(h.emcstatCbs, nil)
	if err != nil {
		return nil, err
	}
	var stat emcstat.StatFull
	if err := json.Unmarshal(resp, &stat); err != nil {
		return nil, fmt.Errorf("unmarshal stat: %w", err)
	}
	return &stat, nil
}

// waitForState polls emcstat until the given state is reached or timeout.
func (h *testHarness) waitForState(wantState emcstat.TaskState, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		stat, err := h.getStat()
		if err != nil {
			return err
		}
		if stat.Task.State == wantState {
			return nil
		}
		time.Sleep(10 * time.Millisecond)
	}
	return fmt.Errorf("timeout waiting for state %d", wantState)
}

// waitForMotionEnabled polls until motion.enabled is true or timeout.
func (h *testHarness) waitForMotionEnabled(timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		stat, err := h.getStat()
		if err != nil {
			return err
		}
		if stat.Motion.Enabled {
			return nil
		}
		time.Sleep(10 * time.Millisecond)
	}
	return fmt.Errorf("timeout waiting for motion enabled")
}

// waitForMotionMode polls until motion.mode matches or timeout.
func (h *testHarness) waitForMotionMode(mode emcstat.TrajMode, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		stat, err := h.getStat()
		if err != nil {
			return err
		}
		if stat.Motion.Mode == mode {
			return nil
		}
		time.Sleep(10 * time.Millisecond)
	}
	return fmt.Errorf("timeout waiting for motion mode %d", mode)
}

// waitForInterpState polls until interp_state matches or timeout.
func (h *testHarness) waitForInterpState(state emcstat.InterpState, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		stat, err := h.getStat()
		if err != nil {
			return err
		}
		if stat.Task.InterpState == state {
			return nil
		}
		time.Sleep(10 * time.Millisecond)
	}
	return fmt.Errorf("timeout waiting for interp state %d", state)
}

// waitForHomed polls until the given joint is homed or timeout.
func (h *testHarness) waitForHomed(joint int, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		stat, err := h.getStat()
		if err != nil {
			return err
		}
		if joint < len(stat.Homed) && stat.Homed[joint] {
			return nil
		}
		time.Sleep(10 * time.Millisecond)
	}
	return fmt.Errorf("timeout waiting for joint %d homed", joint)
}

// waitForPosition polls until position.x is within tolerance or timeout.
func (h *testHarness) waitForPosition(axis int, target, tol float64, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		stat, err := h.getStat()
		if err != nil {
			return err
		}
		var pos float64
		switch axis {
		case 0:
			pos = stat.Position.X
		case 1:
			pos = stat.Position.Y
		case 2:
			pos = stat.Position.Z
		}
		if pos >= target-tol && pos <= target+tol {
			return nil
		}
		time.Sleep(10 * time.Millisecond)
	}
	return fmt.Errorf("timeout waiting for axis %d at %f", axis, target)
}

// waitForInPosition polls until motion.in_position is true or timeout.
func (h *testHarness) waitForInPosition(timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		stat, err := h.getStat()
		if err != nil {
			return err
		}
		if stat.Motion.InPosition {
			return nil
		}
		time.Sleep(10 * time.Millisecond)
	}
	return fmt.Errorf("timeout waiting for in_position")
}

// waitForNotInPosition polls until motion.in_position is false or timeout.
// Used to detect that motion has actually started before waiting for completion.
func (h *testHarness) waitForNotInPosition(timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		stat, err := h.getStat()
		if err != nil {
			return err
		}
		if !stat.Motion.InPosition {
			return nil
		}
		time.Sleep(10 * time.Millisecond)
	}
	// Not an error — motion may have started and finished very quickly.
	return nil
}
