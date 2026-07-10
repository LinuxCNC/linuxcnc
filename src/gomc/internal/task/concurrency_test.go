// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
//
// Regression tests for the command-serialization (cmdMu) and signal-first
// abort design. All of these deadlocked, panicked, or raced before cmdMu /
// seqLifeMu / the finishMDI handoff were introduced. Run with -race.
package task

import (
	"errors"
	"sync"
	"testing"
	"time"
)

// fakeInterp is a scriptable Interpreter for concurrency tests. The
// onExecuteString / onRead / onExecute hooks run on whatever goroutine drives
// the interpreter, exactly like the real canon callbacks.
type fakeInterp struct {
	mu              sync.Mutex
	line            int
	onExecuteString func(cmd string) (int, error)
	onRead          func(call int) (int, error)
	onExecute       func(call int) (int, error)
	onCallLevel     func() int
	reads, execs    int
}

func (f *fakeInterp) IniLoad(string) error { return nil }
func (f *fakeInterp) Init() error          { return nil }
func (f *fakeInterp) Open(string) error    { return nil }

func (f *fakeInterp) Read() (int, error) {
	f.mu.Lock()
	f.reads++
	n := f.reads
	f.line++
	fn := f.onRead
	f.mu.Unlock()
	if fn != nil {
		return fn(n)
	}
	return InterpEndfile, nil
}

func (f *fakeInterp) ReadString(string) (int, error) { return InterpOK, nil }

func (f *fakeInterp) Execute() (int, error) {
	f.mu.Lock()
	f.execs++
	n := f.execs
	fn := f.onExecute
	f.mu.Unlock()
	if fn != nil {
		return fn(n)
	}
	return InterpOK, nil
}

func (f *fakeInterp) ExecuteString(cmd string) (int, error) {
	f.mu.Lock()
	fn := f.onExecuteString
	f.mu.Unlock()
	if fn != nil {
		return fn(cmd)
	}
	return InterpOK, nil
}

func (f *fakeInterp) Synch() error { return nil }
func (f *fakeInterp) Close() error { return nil }
func (f *fakeInterp) Reset() error {
	f.mu.Lock()
	f.reads, f.execs, f.line = 0, 0, 0
	f.mu.Unlock()
	return nil
}
func (f *fakeInterp) Abort(int, string) error { return nil }
func (f *fakeInterp) Line() int               { f.mu.Lock(); defer f.mu.Unlock(); return f.line }
func (f *fakeInterp) SequenceNumber() int     { return 0 }
func (f *fakeInterp) CallLevel() int {
	f.mu.Lock()
	fn := f.onCallLevel
	f.mu.Unlock()
	if fn != nil {
		return fn()
	}
	return 0
}
func (f *fakeInterp) ErrorText(int) string    { return "" }
func (f *fakeInterp) FileName() string        { return "fake.ngc" }
func (f *fakeInterp) Command() string         { return "" }
func (f *fakeInterp) Destroy()                {}
func (f *fakeInterp) ActiveGCodes() []int32   { return make([]int32, activeGCodesLen) }
func (f *fakeInterp) ActiveMCodes() []int32   { return make([]int32, activeMCodesLen) }
func (f *fakeInterp) ActiveSettings() []float64 {
	return make([]float64, activeSettingsLen)
}

// newMDITask builds an ON, MDI-mode task with a running sequencer, a fake
// interpreter, and fast polling. Cleanup stops the sequencer.
func newMDITask(t *testing.T, fi *fakeInterp) *Task {
	t.Helper()
	restore := SetPollInterval(time.Millisecond)
	t.Cleanup(restore)

	task, _, _ := newTestTask()
	task.noForceHoming = true
	task.SetInterpreter(fi)
	bringUp(task)
	if err := task.SetMode(int32(ModeMDI)); err != nil {
		t.Fatalf("SetMode(MDI): %v", err)
	}
	task.StartSequencer()
	t.Cleanup(task.StopSequencer)
	return task
}

// waitIdle polls until the interpreter reports idle or the deadline expires.
func waitIdle(t *testing.T, task *Task, timeout time.Duration) {
	t.Helper()
	ok := waitForCond(timeout, func() bool {
		task.mu.Lock()
		defer task.mu.Unlock()
		return task.interpState == InterpIdle && len(task.mdiQueue) == 0 && task.taskCommand == ""
	})
	if !ok {
		t.Fatalf("task did not become idle within %v", timeout)
	}
}

// TestMDIChain_ExpandsPastQueueSize is the regression test for the sequencer
// self-deadlock: a QUEUED MDI whose execution produces more commands than
// interpQueueSize used to run inside the sequencer goroutine (mdiDoneCmd.
// PostWait -> executeMDI), which then blocked enqueueing into its own full
// queue. With the finishMDI handoff the chain runs off-sequencer and drains.
func TestMDIChain_ExpandsPastQueueSize(t *testing.T) {
	fi := &fakeInterp{}
	fi.onExecuteString = func(cmd string) (int, error) {
		task := activeCanon().task
		switch cmd {
		case "first":
			// Keep the sequencer busy long enough for the second MDI to queue.
			task.EnqueueCmd(&DwellCmd{Seconds: 0.1})
		case "second":
			// Expand well past interpQueueSize (64).
			for i := 0; i < 3*interpQueueSize; i++ {
				task.EnqueueCmd(&LinearMoveCmd{ID: int32(i + 1)})
			}
		}
		return InterpOK, nil
	}
	task := newMDITask(t, fi)

	if err := task.MDI("first"); err != nil {
		t.Fatalf("MDI(first): %v", err)
	}
	if err := task.MDI("second"); err != nil {
		t.Fatalf("MDI(second): %v", err)
	}
	task.mu.Lock()
	queued := len(task.mdiQueue)
	task.mu.Unlock()
	if queued != 1 {
		t.Fatalf("expected second MDI to be queued, mdiQueue len = %d", queued)
	}

	waitIdle(t, task, 5*time.Second)
}

// TestAbort_UnblocksBackpressuredMDI: an MDI holding cmdMu and blocked in
// EnqueueCmd backpressure (sequencer stalled in a long dwell, queue full) must
// be unwedged by Abort. Abort's signal phase runs without cmdMu — if Abort
// queued on cmdMu first, this would deadlock permanently.
func TestAbort_UnblocksBackpressuredMDI(t *testing.T) {
	fi := &fakeInterp{}
	fi.onExecuteString = func(cmd string) (int, error) {
		task := activeCanon().task
		// Sequencer executes the dwell (stalls ~10s), everything after piles
		// up in interpQueue until EnqueueCmd blocks.
		task.EnqueueCmd(&DwellCmd{Seconds: 10})
		for i := 0; i < 2*interpQueueSize; i++ {
			task.EnqueueCmd(&LinearMoveCmd{ID: int32(i + 1)}) // errors after abort are fine
		}
		return InterpOK, nil
	}
	task := newMDITask(t, fi)

	mdiDone := make(chan error, 1)
	go func() { mdiDone <- task.MDI("wedge") }()

	// Wait until the producer is actually blocked (queue full).
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		task.mu.Lock()
		full := t != nil && len(task.interpQueue) >= interpQueueSize
		task.mu.Unlock()
		if full {
			break
		}
		time.Sleep(time.Millisecond)
	}

	abortDone := make(chan struct{})
	go func() { task.Abort(); close(abortDone) }()

	select {
	case <-abortDone:
	case <-time.After(5 * time.Second):
		t.Fatal("Abort did not complete — signal phase failed to unblock the MDI producer")
	}
	select {
	case <-mdiDone:
	case <-time.After(5 * time.Second):
		t.Fatal("MDI did not return after abort")
	}
}

// TestAutoRun_RejectedWhileRunning is the regression test for the double-run
// race: a second Run while a program is mid-run must be rejected, not spawn a
// second producer goroutine on the same interpreter.
func TestAutoRun_RejectedWhileRunning(t *testing.T) {
	release := make(chan struct{})
	fi := &fakeInterp{}
	fi.onRead = func(call int) (int, error) {
		if call == 1 {
			return InterpOK, nil
		}
		<-release // hold the producer in the run loop
		return InterpEndfile, nil
	}
	fi.onExecute = func(call int) (int, error) { return InterpOK, nil }

	restore := SetPollInterval(time.Millisecond)
	defer restore()
	task, _, _ := newTestTask()
	task.noForceHoming = true
	task.SetInterpreter(fi)
	bringUp(task)
	task.SetMode(int32(ModeAuto))
	task.mu.Lock()
	task.programOpen = true
	task.programFile = "fake.ngc"
	task.mu.Unlock()

	if err := task.AutoCommand(AutoRun, 0); err != nil {
		t.Fatalf("first run: %v", err)
	}

	err := task.AutoCommand(AutoRun, 0)
	if !errors.Is(err, ErrBusy) {
		close(release)
		t.Fatalf("second run: want ErrBusy, got %v", err)
	}

	close(release)
	waitIdle(t, task, 5*time.Second)
	task.StopSequencer()
}

// TestStartSequencer_Concurrent hammers concurrent generation changes — this
// used to let two callers pass the same oldDone wait, spawn two loops over one
// queue, and panic on the double close of seqDone.
func TestStartSequencer_Concurrent(t *testing.T) {
	restore := SetPollInterval(time.Millisecond)
	defer restore()
	task, _, _ := newTestTask()

	var wg sync.WaitGroup
	for i := 0; i < 8; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for j := 0; j < 25; j++ {
				task.StartSequencer()
			}
		}()
	}
	wg.Wait()
	if !task.SeqRunning() {
		t.Fatal("sequencer not running after concurrent restarts")
	}
	task.StopSequencer()
	if task.SeqRunning() {
		t.Fatal("sequencer still running after stop")
	}
}

// TestConcurrentCommandStorm drives commands, aborts, stat builds, and state
// changes from many goroutines at once. It asserts nothing beyond "no panic,
// no deadlock, no data race" — the race detector does the real work.
func TestConcurrentCommandStorm(t *testing.T) {
	fi := &fakeInterp{}
	fi.onExecuteString = func(cmd string) (int, error) {
		task := activeCanon().task
		for i := 0; i < 8; i++ {
			task.EnqueueCmd(&LinearMoveCmd{ID: int32(i + 1)})
		}
		return InterpOK, nil
	}
	task := newMDITask(t, fi)

	stop := make(chan struct{})
	var wg sync.WaitGroup
	run := func(fn func()) {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for {
				select {
				case <-stop:
					return
				default:
					fn()
				}
			}
		}()
	}

	run(func() { _ = task.MDI("G0 X1") })
	run(func() { _ = task.Abort(); time.Sleep(time.Millisecond) })
	run(func() { _ = task.BuildStat() })
	run(func() { _ = task.BuildStat() }) // second stat reader (WS + poslog)
	run(func() { _ = task.AutoCommand(AutoPause, 0); _ = task.AutoCommand(AutoResume, 0) })
	run(func() { _ = task.Jog(JogIncrement, false, 0, 10, 1); _ = task.Jog(JogStop, false, 0, 0, 0) })
	run(func() { _ = task.SetOptionalStop(true); _ = task.SetOptionalStop(false) })
	run(func() { _ = task.Flood(true); _ = task.Flood(false) })

	time.Sleep(500 * time.Millisecond)
	close(stop)
	wg.Wait()

	// The machine must still be operable afterwards.
	_ = task.Abort()
	waitIdle(t, task, 5*time.Second)
}

// TestPreflight_RejectsWithoutCmdMu: a command that cannot pass its guards
// must be rejected immediately, without queueing behind whatever command
// currently holds cmdMu. The test wedges cmdMu and issues doomed commands —
// each must return within the deadline (they'd block forever otherwise).
func TestPreflight_RejectsWithoutCmdMu(t *testing.T) {
	task, _, _ := newTestTask() // fresh task is in ESTOP

	task.cmdMu.Lock()
	defer task.cmdMu.Unlock()

	calls := map[string]func() error{
		"MDI":        func() error { return task.MDI("G0 X0") },
		"AutoRun":    func() error { return task.AutoCommand(AutoRun, 0) },
		"Jog":        func() error { return task.Jog(JogContinuous, true, 0, 10, 0) },
		"Spindle":    func() error { return task.Spindle(SpindleForward, 100, 0, 0) },
		"Home":       func() error { return task.Home(0) },
		"Flood":      func() error { return task.Flood(true) },
		"SetStateOn": func() error { return task.SetState(int32(StateOn)) },
	}
	for name, fn := range calls {
		done := make(chan error, 1)
		go func() { done <- fn() }()
		select {
		case err := <-done:
			if err == nil {
				t.Errorf("%s: expected guard rejection, got nil", name)
			}
		case <-time.After(2 * time.Second):
			t.Fatalf("%s: blocked on cmdMu instead of failing its preflight", name)
		}
	}
}

// TestProgramOpen_BusyRejectDoesNotDeadlock: the busy-reject path emits an
// operator error while holding t.mu. Before the message list got its own
// leaf lock (msgMu), operatorError->appendMessage re-locked t.mu and this
// call never returned.
func TestProgramOpen_BusyRejectDoesNotDeadlock(t *testing.T) {
	task, _, _ := newTestTask()
	bringUp(task)
	task.mu.Lock()
	task.interpState = InterpReading // simulate a running program
	task.mu.Unlock()

	done := make(chan error, 1)
	go func() { done <- task.ProgramOpen("x.ngc") }()
	select {
	case err := <-done:
		if !errors.Is(err, ErrBusy) {
			t.Fatalf("want ErrBusy, got %v", err)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("ProgramOpen deadlocked on the busy-reject operator message")
	}
}
