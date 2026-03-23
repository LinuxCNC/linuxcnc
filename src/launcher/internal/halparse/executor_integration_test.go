//go:build cgo && haltest

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
