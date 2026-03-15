package hal

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
