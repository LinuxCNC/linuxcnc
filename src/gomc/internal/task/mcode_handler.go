package task

import (
	"fmt"
	"sync"
)

// mcodeHandlerNum is the number of user-defined M-codes (M100-M199).
const mcodeHandlerNum = 100

// McodeCall holds the parameters for an M-code handler invocation.
type McodeCall struct {
	Mcode   int32
	P       float64
	Q       float64
	abortCh <-chan struct{} // becomes readable on abort
}

// AbortRequested returns true if abort has been signaled.
func (c *McodeCall) AbortRequested() bool {
	select {
	case <-c.abortCh:
		return true
	default:
		return false
	}
}

// McodeHandlerFunc is the signature for M-code handlers.
// Returns 0 on success, non-zero on error.
// Values 32-63 are mapped to user_defined_result (existing convention).
type McodeHandlerFunc func(call *McodeCall) int

// mcodeHandler manages the M100-M199 handler registry and worker goroutine.
type mcodeHandler struct {
	mu       sync.Mutex
	handlers [mcodeHandlerNum]McodeHandlerFunc

	// Worker state
	jobCh   chan mcodeJob
	abortCh chan struct{} // closed to signal abort to running handler
	doneCh  chan struct{} // closed when worker exits

	// Result from last completed handler
	resultMu sync.Mutex
	result   int
	done     bool
}

type mcodeJob struct {
	mcode   int
	p, q    float64
	abortCh <-chan struct{}
}

// newMcodeHandler creates and starts the handler worker.
func newMcodeHandler() *mcodeHandler {
	h := &mcodeHandler{
		jobCh:   make(chan mcodeJob, 1),
		abortCh: make(chan struct{}),
		doneCh:  make(chan struct{}),
	}
	go h.worker()
	return h
}

// RegisterHandler registers a handler for an M-code (100-199).
func (h *mcodeHandler) RegisterHandler(mcode int, fn McodeHandlerFunc) error {
	if mcode < 100 || mcode > 199 {
		return fmt.Errorf("mcode_handler: invalid mcode %d (must be 100-199)", mcode)
	}
	idx := mcode - 100
	h.mu.Lock()
	defer h.mu.Unlock()
	if h.handlers[idx] != nil {
		return fmt.Errorf("mcode_handler: M%d already has a handler", mcode)
	}
	h.handlers[idx] = fn
	return nil
}

// HasHandler returns true if a handler is registered for the given M-code.
func (h *mcodeHandler) HasHandler(mcode int) bool {
	if mcode < 100 || mcode > 199 {
		return false
	}
	h.mu.Lock()
	defer h.mu.Unlock()
	return h.handlers[mcode-100] != nil
}

// Submit submits an M-code for execution. Returns error if worker is busy.
func (h *mcodeHandler) Submit(mcode int, p, q float64) error {
	if mcode < 100 || mcode > 199 {
		return fmt.Errorf("mcode_handler: invalid mcode %d", mcode)
	}
	h.mu.Lock()
	fn := h.handlers[mcode-100]
	h.mu.Unlock()
	if fn == nil {
		return fmt.Errorf("mcode_handler: no handler for M%d", mcode)
	}

	// Reset abort channel for new job
	h.mu.Lock()
	h.abortCh = make(chan struct{})
	h.done = false
	abortCh := h.abortCh
	h.mu.Unlock()

	select {
	case h.jobCh <- mcodeJob{mcode: mcode, p: p, q: q, abortCh: abortCh}:
		return nil
	default:
		return fmt.Errorf("mcode_handler: worker busy")
	}
}

// CheckDone returns (result, true) if the last submitted job is complete.
func (h *mcodeHandler) CheckDone() (int, bool) {
	h.resultMu.Lock()
	defer h.resultMu.Unlock()
	if h.done {
		h.done = false
		return h.result, true
	}
	return 0, false
}

// Abort signals the running handler to stop.
func (h *mcodeHandler) Abort() {
	h.mu.Lock()
	ch := h.abortCh
	h.mu.Unlock()
	select {
	case <-ch:
		// already closed
	default:
		close(ch)
	}
}

// Stop shuts down the worker goroutine.
func (h *mcodeHandler) Stop() {
	close(h.jobCh)
	h.Abort()
	<-h.doneCh
}

// worker is the goroutine that executes M-code handlers sequentially.
func (h *mcodeHandler) worker() {
	defer close(h.doneCh)

	for job := range h.jobCh {
		h.mu.Lock()
		fn := h.handlers[job.mcode-100]
		h.mu.Unlock()

		if fn == nil {
			h.resultMu.Lock()
			h.result = -1
			h.done = true
			h.resultMu.Unlock()
			continue
		}

		call := &McodeCall{
			Mcode:   int32(job.mcode),
			P:       job.p,
			Q:       job.q,
			abortCh: job.abortCh,
		}

		result := fn(call)

		h.resultMu.Lock()
		h.result = result
		h.done = true
		h.resultMu.Unlock()
	}
}
