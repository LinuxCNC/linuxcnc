package realtime

import "os/exec"

// Known LinuxCNC shared memory keys (from realtime.in).
// These are removed with ipcrm -M <key> which removes by key rather than id.
var linuxcncShmKeys = []string{
	"0x48414c32", // HAL_KEY
	"0x90280A48", // RTAPI_KEY
	"0x48484c34", // UUID_KEY
}

// cleanupIPC removes known LinuxCNC IPC resources (shared memory segments).
// This mirrors the Unload() function in scripts/realtime.in for uspace.
//
// Individual ipcrm errors are logged at debug level and suppressed, matching
// the `2>/dev/null` behaviour of the legacy script — segments may simply not
// exist when cleanup is called.
func (m *Manager) cleanupIPC() error {
	for _, key := range linuxcncShmKeys {
		// ipcrm -M <key>  removes the shared memory segment identified by key.
		out, err := exec.Command("ipcrm", "-M", key).CombinedOutput()
		if err != nil {
			m.logger.Debug("ipcrm returned error (segment may not exist)",
				"key", key, "error", err, "output", string(out))
		}
	}
	return nil
}
