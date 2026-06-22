// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

// Interpreter defines the interface to the G-code interpreter.
// Implementations include CInterp (cgo bridge to librs274ngc) and
// mocks for testing.
type Interpreter interface {
	// IniLoad loads interpreter configuration from INI file.
	IniLoad(inifile string) error
	// Init initializes the interpreter.
	Init() error
	// Open opens a G-code file for interpretation.
	Open(filename string) error
	// Read reads the next line from the open file.
	// Returns the interpreter result code.
	Read() (int, error)
	// ReadString reads a line from a string buffer.
	ReadString(line string) (int, error)
	// Execute executes the last read line.
	// Returns the interpreter result code.
	Execute() (int, error)
	// ExecuteString executes a command string directly.
	ExecuteString(cmd string) (int, error)
	// Synch synchronizes interpreter state with the machine.
	Synch() error
	// Close closes the currently open file.
	Close() error
	// Reset resets the interpreter to initial state.
	Reset() error
	// Abort notifies the interpreter of an abort condition.
	Abort(reason int, message string) error
	// Line returns the current line number.
	Line() int
	// SequenceNumber returns the current N-word sequence number.
	SequenceNumber() int
	// ErrorText returns the error text for a given return code.
	ErrorText(errcode int) string
	// FileName returns the currently open file name.
	FileName() string
	// Command returns the current command text.
	Command() string
	// Destroy releases interpreter resources.
	Destroy()
	// ActiveGCodes retrieves the active G-code array from the interpreter.
	ActiveGCodes() []int32
	// ActiveMCodes retrieves the active M-code array from the interpreter.
	ActiveMCodes() []int32
	// ActiveSettings retrieves the active settings (feed, speed, etc).
	ActiveSettings() []float64
}

// Interpreter return codes (from interp_return.hh).
const (
	InterpOK            = 0
	InterpExit          = 1
	InterpExecuteFinish = 2
	InterpEndfile       = 3
	InterpFileNotOpen   = 4
	InterpError         = 5
)
