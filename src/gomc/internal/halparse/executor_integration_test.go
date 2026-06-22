//go:build cgo && haltest

// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2

package halparse

import "testing"

// TestExecuteToken_LiveHAL is a placeholder for live HAL environment tests.
// These require a running HAL instance and are skipped in normal CI.
func TestExecuteToken_LiveHAL(t *testing.T) {
	t.Skip("requires live HAL environment")
}

// TestParseResultExecute_LiveHAL is a placeholder for live HAL environment tests.
// These require a running HAL instance and are skipped in normal CI.
func TestParseResultExecute_LiveHAL(t *testing.T) {
	t.Skip("requires live HAL environment")
}
