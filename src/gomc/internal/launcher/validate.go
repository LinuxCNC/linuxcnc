// Package launcher — validate.go implements cross-section INI dependency
// validation that runs after INI parsing but before any subprocess is started.
package launcher

import "fmt"

// validateDependencies checks cross-section INI dependencies and returns an
// error if the configuration is contradictory or incomplete.
//
// Rules:
//   - At least one [HAL]HALFILE is required.
func (l *Launcher) validateDependencies() error {
	halFiles := l.ini.GetAll("HAL", "HALFILE")

	// At least one [HAL]HALFILE is required.
	if len(halFiles) == 0 {
		return fmt.Errorf("at least one [HAL]HALFILE is required")
	}

	return nil
}
