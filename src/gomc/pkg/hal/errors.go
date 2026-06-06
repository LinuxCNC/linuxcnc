package hal

import "fmt"

// Error represents a HAL error with additional context.
type Error struct {
	// Code is the error code (negative values indicate errors).
	Code int

	// Message is the human-readable error message.
	Message string

	// Op is the operation that failed (e.g., "hal_init", "hal_pin_new").
	Op string
}

// Error implements the error interface.
func (e *Error) Error() string {
	if e.Op != "" {
		return fmt.Sprintf("%s: %s (code %d)", e.Op, e.Message, e.Code)
	}
	return fmt.Sprintf("%s (code %d)", e.Message, e.Code)
}

// Common HAL errors.
// These error codes are based on typical RTAPI/HAL error codes.
var (
	// ErrInvalidName indicates an invalid component or pin name.
	ErrInvalidName = &Error{
		Code:    -22, // -EINVAL
		Message: "invalid component or pin name",
	}

	// ErrNotReady indicates the component is not ready for operation.
	ErrNotReady = &Error{
		Code:    -1,
		Message: "component not ready",
	}

	// ErrAlreadyReady indicates the component is already marked as ready.
	ErrAlreadyReady = &Error{
		Code:    -16, // -EBUSY
		Message: "component already marked ready",
	}

	// ErrInitFailed indicates HAL initialization failed.
	ErrInitFailed = &Error{
		Code:    -1,
		Message: "HAL initialization failed",
	}

	// ErrPinCreateFailed indicates pin creation failed.
	ErrPinCreateFailed = &Error{
		Code:    -1,
		Message: "failed to create pin",
	}

	// ErrComponentNotFound indicates the component ID is invalid.
	ErrComponentNotFound = &Error{
		Code:    -22, // -EINVAL
		Message: "component not found",
	}

	// ErrNoMemory indicates insufficient memory in HAL shared memory.
	ErrNoMemory = &Error{
		Code:    -12, // -ENOMEM
		Message: "insufficient HAL shared memory",
	}
)

// newError creates a new Error with the specified operation, message, and code.
func newError(op string, message string, code int) *Error {
	return &Error{
		Op:      op,
		Message: message,
		Code:    code,
	}
}
