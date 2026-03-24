package hal

// This file serves as the main entry point for the hal package.
// It re-exports the key types and functions for convenience.

// Version is the version of the hal-go package.
const Version = "0.1.0"

// HAL constant definitions from hal.h
const (
	// NameLen is the maximum length for HAL names (pins, signals, components).
	// This matches HAL_NAME_LEN from hal.h.
	NameLen = 127
)

