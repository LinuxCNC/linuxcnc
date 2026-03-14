package halfile

import (
	"fmt"
	"os"
	"path/filepath"
)

// executeTwopass delegates HAL file execution to the legacy twopass.tcl script
// when [HAL]TWOPASS is set in the INI file.  It searches for twopass.tcl in
// several locations and runs it via haltcl, passing the INI file path so that
// twopass.tcl can read all [HAL]HALFILE entries itself.
func (e *Executor) executeTwopass() error {
	path, err := e.findTwopassTcl()
	if err != nil {
		return err
	}
	e.logger.Info("TWOPASS: delegating to twopass.tcl", "path", path)
	return e.runHaltcl(path, nil)
}

// findTwopassTcl searches for twopass.tcl in the following locations (in order):
//  1. HALLIB_DIR/twopass.tcl
//  2. $LINUXCNC_TCL_DIR/twopass.tcl
//  3. $LINUXCNC_HOME/tcl/twopass.tcl
//  4. filepath.Dir(HALLIB_DIR)/tcl/twopass.tcl  (RIP build fallback)
//
// It returns the first path that exists, or an error listing all searched paths.
func (e *Executor) findTwopassTcl() (string, error) {
	var candidates []string

	halibDir := e.halibDir()
	if halibDir != "" {
		candidates = append(candidates, filepath.Join(halibDir, "twopass.tcl"))
	}

	if tclDir := os.Getenv("LINUXCNC_TCL_DIR"); tclDir != "" {
		candidates = append(candidates, filepath.Join(tclDir, "twopass.tcl"))
	}

	if home := os.Getenv("LINUXCNC_HOME"); home != "" {
		candidates = append(candidates, filepath.Join(home, "tcl", "twopass.tcl"))
	}

	if halibDir != "" {
		candidates = append(candidates, filepath.Join(filepath.Dir(halibDir), "tcl", "twopass.tcl"))
	}

	for _, p := range candidates {
		if _, err := os.Stat(p); err == nil {
			return p, nil
		}
	}

	return "", fmt.Errorf("TWOPASS: twopass.tcl not found; searched: %v", candidates)
}
