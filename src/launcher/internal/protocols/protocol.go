// Package protocols defines the generic Protocol interface for external
// communication protocols (ADS, etc.) that integrate with the launcher.
package protocols

// Protocol is the interface that protocol implementations must satisfy.
//
// The launcher calls these methods in order during startup and shutdown:
//
//  1. Init — create HAL components and pins (after loadusr/loadrt,
//     before net/addf/setp so that HAL files can wire protocol pins).
//  2. Start — open network listeners and begin serving (after HAL threads
//     are started).
//  3. Stop — shut down listeners and release resources (during cleanup).
type Protocol interface {
	// Init creates HAL components and registers pins. Called after
	// ParseResult.Load() but before ParseResult.Execute().
	Init() error

	// Start opens network listeners. Called after HAL threads are started.
	Start() error

	// Stop shuts down all listeners and HAL components.
	Stop()
}
