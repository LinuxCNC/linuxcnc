package halfile

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// resolvePath finds a HAL file by searching in:
//  1. The directory containing the INI configuration file (configDir).
//  2. Each directory in halibPath (colon-separated, same as HALLIB_PATH).
//
// If filename starts with "LIB:", the prefix is stripped and the file is
// resolved exclusively from the hallib directories in halibPath (not from
// configDir), matching the legacy linuxcnc.in behaviour.
//
// If filename is already absolute and the file exists, it is returned as-is.
// An error is returned if the file cannot be found in any search location.
func (e *Executor) resolvePath(filename string) (string, error) {
	// LIB: prefix – resolve from hallib directories only, not configDir.
	if strings.HasPrefix(filename, "LIB:") {
		libFile := strings.TrimPrefix(filename, "LIB:")
		for _, dir := range strings.Split(e.halibPath, ":") {
			dir = strings.TrimSpace(dir)
			if dir == "" {
				continue
			}
			candidate := filepath.Join(dir, libFile)
			if _, err := os.Stat(candidate); err == nil {
				abs, err := filepath.Abs(candidate)
				if err != nil {
					return "", fmt.Errorf("resolving path %q: %w", candidate, err)
				}
				return abs, nil
			}
		}
		return "", fmt.Errorf("HAL file %q not found in HALLIB_PATH", filename)
	}

	// Absolute paths are used directly if the file exists.
	if filepath.IsAbs(filename) {
		if _, err := os.Stat(filename); err == nil {
			return filename, nil
		}
		return "", fmt.Errorf("HAL file not found: %s", filename)
	}

	// Build the ordered list of directories to search.
	var searchDirs []string
	if e.configDir != "" {
		searchDirs = append(searchDirs, e.configDir)
	}
	for _, dir := range strings.Split(e.halibPath, ":") {
		dir = strings.TrimSpace(dir)
		if dir != "" {
			searchDirs = append(searchDirs, dir)
		}
	}

	for _, dir := range searchDirs {
		candidate := filepath.Join(dir, filename)
		if _, err := os.Stat(candidate); err == nil {
			abs, err := filepath.Abs(candidate)
			if err != nil {
				return "", fmt.Errorf("resolving path %q: %w", candidate, err)
			}
			return abs, nil
		}
	}

	return "", fmt.Errorf("HAL file %q not found in config dir or HALLIB_PATH", filename)
}
