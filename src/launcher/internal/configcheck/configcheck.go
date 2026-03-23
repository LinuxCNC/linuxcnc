// Package configcheck validates INI file configuration for LinuxCNC.
//
// This is a native Go replacement for lib/hallib/check_config.tcl.
// It validates mandatory items, kinematics consistency, and joint/axis
// limit relationships.
package configcheck

import (
	"fmt"
	"strconv"
	"strings"

	"github.com/sittner/linuxcnc/src/launcher/pkg/inifile"
)

const progname = "check_config"

// Default values matching src/emc/nml_intf/emccfg.h, src/emc/ini/inijoint.cc,
// src/emc/ini/iniaxis.cc.
const (
	defaultAxisMaxVelocity      = 1.0
	defaultAxisMaxAcceleration  = 1.0
	defaultJointMaxVelocity     = 1.0
	defaultJointMaxAcceleration = 1.0
	defaultAxisMinLimit         = -1e99
	defaultAxisMaxLimit         = +1e99
)

// allCoords is the full set of coordinate letters, used as the default when
// trivkins coordinates= is not specified.
var allCoords = []byte{'X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W'}

// Result holds the outcome of a configuration check.
type Result struct {
	Warnings []string
	Errors   []string
	// KinsModule is the kinematics module name (e.g. "trivkins"), set after
	// parsing [KINS]KINEMATICS.  Used for formatting output.
	KinsModule string
}

// HasErrors returns true if any fatal errors were recorded.
func (r *Result) HasErrors() bool {
	return len(r.Errors) > 0
}

// FormatWarnings returns the warning block as a single string matching the
// Tcl script's output format, or "" if there are no warnings.
func (r *Result) FormatWarnings() string {
	if len(r.Warnings) == 0 {
		return ""
	}
	var b strings.Builder
	fmt.Fprintf(&b, "\n%s:\n", progname)
	if r.KinsModule != "" {
		fmt.Fprintf(&b, "(%s kinematics) WARNING:\n", r.KinsModule)
	}
	for _, w := range r.Warnings {
		fmt.Fprintf(&b, "  %s\n", w)
	}
	b.WriteByte('\n')
	return b.String()
}

// FormatErrors returns the error block as a single string matching the Tcl
// script's output format, or "" if there are no errors.
func (r *Result) FormatErrors() string {
	if len(r.Errors) == 0 {
		return ""
	}
	var b strings.Builder
	fmt.Fprintf(&b, "\n%s:\n", progname)
	if r.KinsModule != "" {
		fmt.Fprintf(&b, "(%s kinematics) ERROR:\n", r.KinsModule)
	}
	for _, e := range r.Errors {
		fmt.Fprintf(&b, "  %s\n", e)
	}
	b.WriteByte('\n')
	return b.String()
}

// Check validates the INI configuration.  It returns a Result containing any
// warnings (non-fatal) and errors (fatal).  A non-nil error is returned only
// for unexpected failures (not validation errors — those go into Result.Errors).
func Check(ini *inifile.IniFile) (*Result, error) {
	r := &Result{}

	// 1. Mandatory items.
	checkMandatoryItems(ini, r)
	if r.HasErrors() {
		return r, nil
	}

	// 2. Parse kinematics.
	kinsValue := ini.Get("KINS", "KINEMATICS")
	module, params := parseKinematics(kinsValue)
	r.KinsModule = module

	// 3. Resolve coordinates.
	coords, coordsSpecified := resolveCoordinates(params)

	// 4. Non-trivkins: early return with info message.
	if module != "trivkins" {
		r.Warnings = append(r.Warnings,
			fmt.Sprintf("Unchecked: [KINS]KINEMATICS=%s", kinsValue))
		return r, nil
	}

	// 5. Build joint→coordinate mapping for trivkins.
	jointIdx := jointsForTrivkins(coords)

	// 6. Extra joints check.
	checkExtraJoints(ini, r)

	// 7. Warn on duplicate values in JOINT_*/AXIS_* sections.
	warnMultipleIniValues(ini, r)

	// 8. Validate identity kinematics limits.
	jointsStr := ini.Get("KINS", "JOINTS")
	numJoints, err := strconv.Atoi(strings.TrimSpace(jointsStr))
	if err != nil {
		return nil, fmt.Errorf("invalid [KINS]JOINTS value %q: %w", jointsStr, err)
	}
	validateIdentityKinsLimits(ini, r, numJoints, coords, jointIdx)

	// 9. Consistent coordinates check.
	consistentCoordsForTrivkins(ini, r, coords, coordsSpecified)

	return r, nil
}

// checkMandatoryItems verifies that required INI keys are present.
func checkMandatoryItems(ini *inifile.IniFile, r *Result) {
	mandatory := [][2]string{
		{"KINS", "KINEMATICS"},
		{"KINS", "JOINTS"},
	}
	for _, item := range mandatory {
		if ini.Get(item[0], item[1]) == "" {
			r.Errors = append(r.Errors,
				fmt.Sprintf("Missing [%s]%s=", item[0], item[1]))
		}
	}
}

// parseKinematics splits a [KINS]KINEMATICS value into the module name and
// a map of parm=value parameters.
//
// Example: "trivkins coordinates=XZ kinstype=BOTH"
//
//	→ module="trivkins", params={"coordinates":"XZ", "kinstype":"BOTH"}
func parseKinematics(value string) (module string, params map[string]string) {
	params = make(map[string]string)
	fields := strings.Fields(value)
	if len(fields) == 0 {
		return "", params
	}
	module = fields[0]
	for _, f := range fields[1:] {
		if idx := strings.IndexByte(f, '='); idx >= 0 {
			params[f[:idx]] = f[idx+1:]
		}
	}
	return module, params
}

// resolveCoordinates returns the coordinate letters (uppercased) and whether
// coordinates= was explicitly specified.
func resolveCoordinates(params map[string]string) (coords []byte, specified bool) {
	raw, ok := params["coordinates"]
	if !ok || raw == "" {
		return allCoords, false
	}
	upper := strings.ToUpper(raw)
	coords = make([]byte, 0, len(upper))
	for i := 0; i < len(upper); i++ {
		coords = append(coords, upper[i])
	}
	return coords, true
}

// jointsForTrivkins builds a mapping from coordinate letter to joint indices.
// Joint numbers are assigned consecutively in the order of the coordinates
// string (matching trivkins.c behaviour).
func jointsForTrivkins(coords []byte) map[byte][]int {
	m := make(map[byte][]int)
	for i, c := range coords {
		m[c] = append(m[c], i)
	}
	return m
}

// checkExtraJoints warns if [EMCMOT]EMCMOT specifies num_extrajoints.
func checkExtraJoints(ini *inifile.IniFile, r *Result) {
	emcmot := ini.Get("EMCMOT", "EMCMOT")
	if emcmot == "" {
		return
	}
	if !strings.Contains(emcmot, "motmod") {
		return
	}
	fields := strings.Fields(emcmot)
	for _, f := range fields {
		parts := strings.SplitN(f, "=", 2)
		if len(parts) == 2 && parts[0] == "num_extrajoints" {
			numExtra, err := strconv.Atoi(parts[1])
			if err != nil {
				continue
			}
			joints := ini.Get("KINS", "JOINTS")
			r.Warnings = append(r.Warnings,
				fmt.Sprintf("Extra joints specified=%d\n [KINS]JOINTS=%s must accommodate kinematic joints *plus* extra joints",
					numExtra, joints))
			return
		}
	}
}

// warnMultipleIniValues warns about duplicate keys in JOINT_* and AXIS_*
// sections.
func warnMultipleIniValues(ini *inifile.IniFile, r *Result) {
	// Collect unique section names matching JOINT_ or AXIS_.
	seen := make(map[string]bool)
	for _, sec := range ini.Sections {
		if seen[sec.Name] {
			continue
		}
		if !strings.HasPrefix(sec.Name, "JOINT_") && !strings.HasPrefix(sec.Name, "AXIS_") {
			continue
		}
		seen[sec.Name] = true

		// Count occurrences of each key in this section.
		keyCounts := make(map[string]int)
		keyValues := make(map[string][]string)
		entries := ini.GetSection(sec.Name)
		for _, e := range entries {
			keyCounts[e.Key]++
			keyValues[e.Key] = append(keyValues[e.Key], e.Value)
		}
		for key, count := range keyCounts {
			if count > 1 {
				r.Warnings = append(r.Warnings,
					fmt.Sprintf("Unexpected multiple values [%s]%s: %s",
						sec.Name, key, strings.Join(keyValues[key], " ")))
			}
		}
	}
}

// validateIdentityKinsLimits checks joint and axis velocity, acceleration, and
// limit consistency for trivkins (identity kinematics).
func validateIdentityKinsLimits(ini *inifile.IniFile, r *Result, numJoints int, coords []byte, jointIdx map[byte][]int) {
	// Per-joint checks.
	for j := 0; j < numJoints; j++ {
		sec := fmt.Sprintf("JOINT_%d", j)
		if ini.Get(sec, "MAX_VELOCITY") == "" {
			r.Warnings = append(r.Warnings,
				fmt.Sprintf("Unspecified [%s]MAX_VELOCITY,     default used: %.1f", sec, defaultJointMaxVelocity))
		}
		if ini.Get(sec, "MAX_ACCELERATION") == "" {
			r.Warnings = append(r.Warnings,
				fmt.Sprintf("Unspecified [%s]MAX_ACCELERATION, default used: %.1f", sec, defaultJointMaxAcceleration))
		}
	}

	// Per unique coordinate checks.
	uniqueCoords := uniqueBytes(coords)
	for _, c := range uniqueCoords {
		cl := string(c)
		axisSec := "AXIS_" + cl

		// Only check velocity/acceleration if the axis section exists.
		if len(ini.GetSection(axisSec)) > 0 {
			if ini.Get(axisSec, "MAX_VELOCITY") == "" {
				r.Warnings = append(r.Warnings,
					fmt.Sprintf("Unspecified [%s]MAX_VELOCITY,     default used: %.1f", axisSec, defaultAxisMaxVelocity))
			}
			if ini.Get(axisSec, "MAX_ACCELERATION") == "" {
				r.Warnings = append(r.Warnings,
					fmt.Sprintf("Unspecified [%s]MAX_ACCELERATION, default used: %.1f", axisSec, defaultAxisMaxAcceleration))
			}
		}

		// Resolve axis limits (with defaults).
		axisMinStr := ini.Get(axisSec, "MIN_LIMIT")
		axisMaxStr := ini.Get(axisSec, "MAX_LIMIT")
		missingAxisMin := axisMinStr == ""
		missingAxisMax := axisMaxStr == ""

		axisMin := defaultAxisMinLimit
		if !missingAxisMin {
			if v, err := strconv.ParseFloat(strings.TrimSpace(axisMinStr), 64); err == nil {
				axisMin = v
			}
		}
		axisMax := defaultAxisMaxLimit
		if !missingAxisMax {
			if v, err := strconv.ParseFloat(strings.TrimSpace(axisMaxStr), 64); err == nil {
				axisMax = v
			}
		}

		// Check each joint mapped to this coordinate.
		for _, j := range jointIdx[c] {
			jointSec := fmt.Sprintf("JOINT_%d", j)

			jointMinStr := ini.Get(jointSec, "MIN_LIMIT")
			if jointMinStr != "" {
				jlim, err := strconv.ParseFloat(strings.TrimSpace(jointMinStr), 64)
				if err == nil && jlim > axisMin {
					if missingAxisMin {
						r.Warnings = append(r.Warnings,
							fmt.Sprintf("Unspecified [%s]MIN_LIMIT,        default used: %g", axisSec, defaultAxisMinLimit))
					}
					r.Errors = append(r.Errors,
						fmt.Sprintf("[%s]MIN_LIMIT > [%s]MIN_LIMIT (%g > %g)", jointSec, axisSec, jlim, axisMin))
				}
			}

			jointMaxStr := ini.Get(jointSec, "MAX_LIMIT")
			if jointMaxStr != "" {
				jlim, err := strconv.ParseFloat(strings.TrimSpace(jointMaxStr), 64)
				if err == nil && jlim < axisMax {
					if missingAxisMax {
						r.Warnings = append(r.Warnings,
							fmt.Sprintf("Unspecified [%s]MAX_LIMIT,        default used: %g", axisSec, defaultAxisMaxLimit))
					}
					r.Errors = append(r.Errors,
						fmt.Sprintf("[%s]MAX_LIMIT < [%s]MAX_LIMIT (%g < %g)", jointSec, axisSec, jlim, axisMax))
				}
			}
		}
	}
}

// consistentCoordsForTrivkins checks that trivkins coordinates= matches
// [TRAJ]COORDINATES when coordinates= was explicitly specified.
func consistentCoordsForTrivkins(ini *inifile.IniFile, r *Result, coords []byte, specified bool) {
	trivStr := stripWhitespace(string(coords))

	// If coordinates= was not specified (full default set), any [TRAJ]COORDINATES
	// is allowed.
	if !specified || strings.EqualFold(trivStr, "XYZABCUVW") {
		return
	}

	trajCoords := stripWhitespace(ini.Get("TRAJ", "COORDINATES"))
	if !strings.EqualFold(trivStr, trajCoords) {
		r.Warnings = append(r.Warnings,
			fmt.Sprintf("INCONSISTENT coordinates specifications:\n               trivkins coordinates=%s\n               [TRAJ]COORDINATES=%s",
				trivStr, trajCoords))
	}
}

// uniqueBytes returns the unique elements of b, preserving first-occurrence order.
func uniqueBytes(b []byte) []byte {
	seen := make(map[byte]bool)
	var result []byte
	for _, v := range b {
		if !seen[v] {
			seen[v] = true
			result = append(result, v)
		}
	}
	return result
}

// stripWhitespace removes all spaces and tabs from s.
func stripWhitespace(s string) string {
	s = strings.ReplaceAll(s, " ", "")
	s = strings.ReplaceAll(s, "\t", "")
	return s
}
