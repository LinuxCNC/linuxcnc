// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package gomc

import "sync"

// LogErrorFunc is called when a C module emits an ERROR-level log message.
// component is the module instance name (e.g. "motmod"), msg is the text.
type LogErrorFunc func(component, msg string)

var (
	hookMu   sync.RWMutex
	logHooks []LogErrorFunc
)

// OnLogError registers a callback that will be invoked for every ERROR-level
// log message from C modules. Used by milltask to forward motion error
// messages (like "joint N following error") to the operator message list,
// matching the old reportError() → error buffer path.
func OnLogError(fn LogErrorFunc) {
	hookMu.Lock()
	defer hookMu.Unlock()
	logHooks = append(logHooks, fn)
}

// NotifyLogError is called by the launcher's log drain loop for ERROR-level
// messages. It fans out to all registered hooks.
func NotifyLogError(component, msg string) {
	hookMu.RLock()
	defer hookMu.RUnlock()
	for _, fn := range logHooks {
		fn(component, msg)
	}
}
