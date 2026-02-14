package hal

import (
	"fmt"
	"log"
	"os"
	"os/signal"
	"sync"
	"syscall"
)

// Component represents a HAL component.
// A component is the basic unit of HAL functionality. It can export pins
// and parameters that other components can connect to via signals.
type Component struct {
	// id is the HAL component ID assigned by hal_init().
	id int

	// name is the unique name of the component.
	name string

	// ready indicates whether the component has been marked ready.
	ready bool

	// running indicates whether the component should continue running.
	running bool

	// done is used to signal the signal handler goroutine to exit.
	done chan struct{}

	// mu protects the component state.
	mu sync.RWMutex
}

// NewComponent creates and initializes a new HAL component.
//
// The name must be unique across all HAL components in the system and
// must not exceed 47 characters (HAL_NAME_LEN).
//
// This calls hal_init() via CGO to register the component with HAL.
//
// Returns the component on success, or an error if initialization fails.
func NewComponent(name string) (*Component, error) {
	if name == "" || len(name) > 47 {
		return nil, newError("NewComponent", ErrInvalidName.Message, ErrInvalidName.Code)
	}

	// Call hal_init() to register the component
	id, err := halInit(name)
	if err != nil {
		return nil, err
	}

	comp := &Component{
		id:      id,
		name:    name,
		ready:   false,
		running: true,
		done:    make(chan struct{}),
	}

	// Set up signal handling for graceful shutdown
	comp.setupSignalHandler()

	return comp, nil
}

// Ready marks the component as ready for operation.
//
// This must be called after all pins and parameters have been created,
// but before the component enters its main loop. It allows halcmd's
// 'loadusr -W' to wait until the component is ready.
//
// This calls hal_ready() via CGO.
func (c *Component) Ready() error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.ready {
		return newError("Ready", ErrAlreadyReady.Message, ErrAlreadyReady.Code)
	}

	// Call hal_ready()
	if err := halReady(c.id); err != nil {
		return err
	}

	c.ready = true
	return nil
}

// Running returns true while the component should continue running.
//
// The main loop should check this periodically and exit cleanly when
// it returns false. This will be set to false when a shutdown signal
// (SIGTERM or SIGINT) is received.
func (c *Component) Running() bool {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.running
}

// Exit cleans up the component and releases all HAL resources.
//
// This should be called (typically via defer) when the component is
// shutting down. It unregisters the component and removes all pins
// and parameters.
//
// This calls hal_exit() via CGO.
func (c *Component) Exit() error {
	c.mu.Lock()
	defer c.mu.Unlock()

	// Signal the signal handler goroutine to exit
	select {
	case <-c.done:
		// Already closed
	default:
		close(c.done)
	}

	// Call hal_exit()
	if err := halExit(c.id); err != nil {
		// Mark as not running even on error
		c.running = false
		return err
	}

	c.running = false
	return nil
}

// Name returns the component name.
func (c *Component) Name() string {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.name
}

// ID returns the HAL component ID.
//
// This ID is used internally for HAL API calls and is assigned by
// hal_init().
func (c *Component) ID() int {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.id
}

// IsReady returns true if the component has been marked ready.
func (c *Component) IsReady() bool {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.ready
}

// Stop signals the component to stop running.
//
// This sets the running flag to false, which will cause Running() to
// return false and the main loop to exit. This is typically called
// by signal handlers.
func (c *Component) Stop() {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.running = false
}

// String returns a string representation of the component.
func (c *Component) String() string {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return fmt.Sprintf("Component{name=%s, id=%d, ready=%t, running=%t}",
		c.name, c.id, c.ready, c.running)
}

// setupSignalHandler sets up handlers for SIGTERM and SIGINT signals.
//
// When either signal is received, the component's running flag is set to false,
// which causes Running() to return false and allows the main loop to exit
// gracefully. This ensures hal_exit() is called via defer.
//
// SIGTERM is sent by halcmd when unloading a component.
// SIGINT is sent when the user presses Ctrl+C.
//
// The goroutine is properly cleaned up when Exit() is called.
func (c *Component) setupSignalHandler() {
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGTERM, syscall.SIGINT)

	go func() {
		select {
		case sig := <-sigChan:
			log.Printf("Received signal %v, initiating shutdown", sig)
			c.mu.Lock()
			c.running = false
			c.mu.Unlock()
			// Stop listening for signals
			signal.Stop(sigChan)
		case <-c.done:
			// Component exiting, clean up
			signal.Stop(sigChan)
		}
	}()
}
