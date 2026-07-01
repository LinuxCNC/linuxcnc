// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// configcheck.go validates kinematics/joint/axis INI configuration consistency.
//
// This is a native Go replacement for lib/hallib/check_config.tcl.
// It validates kinematics consistency and joint/axis limit relationships.
// These checks only apply when a motion controller is configured
// ([KINS]KINEMATICS is set).
package task

import (
	"fmt"
	"strconv"
	"strings"

	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

const configCheckProgname = "check_config"

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

// configCheckResult holds the outcome of a configuration check.
type configCheckResult struct {
	Warnings   []string
	Errors     []string
	KinsModule string // kinematics module name (e.g. "trivkins")
}

func (r *configCheckResult) hasErrors() bool {
	return len(r.Errors) > 0
}

func (r *configCheckResult) formatWarnings() string {
	if len(r.Warnings) == 0 {
		return ""
	}
	var b strings.Builder
	fmt.Fprintf(&b, "\n%s:\n", configCheckProgname)
	if r.KinsModule != "" {
		fmt.Fprintf(&b, "(%s kinematics) WARNING:\n", r.KinsModule)
	}
	for _, w := range r.Warnings {
		fmt.Fprintf(&b, "  %s\n", w)
	}
	b.WriteByte('\n')
	return b.String()
}

func (r *configCheckResult) formatErrors() string {
	if len(r.Errors) == 0 {
		return ""
	}
	var b strings.Builder
	fmt.Fprintf(&b, "\n%s:\n", configCheckProgname)
	if r.KinsModule != "" {
		fmt.Fprintf(&b, "(%s kinematics) ERROR:\n", r.KinsModule)
	}
	for _, e := range r.Errors {
		fmt.Fprintf(&b, "  %s\n", e)
	}
	b.WriteByte('\n')
	return b.String()
}

// runConfigCheck validates the INI configuration for kinematics/joint/axis
// consistency.  Returns a result with warnings and errors, or a non-nil error
// for unexpected failures.
func runConfigCheck(ini *inifile.IniFile) (*configCheckResult, error) {
	r := &configCheckResult{}

	// Kinematics checks only apply when [KINS]KINEMATICS is configured.
	// Configurations without a motion controller (no kinematics) skip these.
	kinsValue := ini.Get("KINS", "KINEMATICS")
	if kinsValue == "" {
		return r, nil
	}

	// [KINS]JOINTS is required when [KINS]KINEMATICS is set.
	jointsStr := ini.Get("KINS", "JOINTS")
	if jointsStr == "" {
		r.Errors = append(r.Errors, "Missing [KINS]JOINTS= (required when [KINS]KINEMATICS is set)")
		return r, nil
	}

	// Parse kinematics.
	module, params := parseKinematics(kinsValue)
	r.KinsModule = module

	// Resolve coordinates.
	coords, coordsSpecified := resolveCoordinates(params)

	// Non-trivkins: early return with info message.
	if module != "trivkins" {
		r.Warnings = append(r.Warnings,
			fmt.Sprintf("Unchecked: [KINS]KINEMATICS=%s", kinsValue))
		return r, nil
	}

	// Build joint→coordinate mapping for trivkins.
	jointIdx := jointsForTrivkins(coords)

	// Extra joints check.
	ccCheckExtraJoints(ini, r)

	// Warn on duplicate values in JOINT_*/AXIS_* sections.
	ccWarnMultipleIniValues(ini, r)

	// Validate identity kinematics limits.
	numJoints, err := strconv.Atoi(strings.TrimSpace(jointsStr))
	if err != nil {
		return nil, fmt.Errorf("invalid [KINS]JOINTS value %q: %w", jointsStr, err)
	}
	ccValidateIdentityKinsLimits(ini, r, numJoints, coords, jointIdx)

	// Consistent coordinates check.
	ccConsistentCoordsForTrivkins(ini, r, coords, coordsSpecified)

	return r, nil
}

// parseKinematics splits a [KINS]KINEMATICS value into the module name and
// a map of parm=value parameters.
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
func jointsForTrivkins(coords []byte) map[byte][]int {
	m := make(map[byte][]int)
	for i, c := range coords {
		m[c] = append(m[c], i)
	}
	return m
}

// ccCheckExtraJoints warns if [EMCMOT]EMCMOT specifies num_extrajoints.
func ccCheckExtraJoints(ini *inifile.IniFile, r *configCheckResult) {
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

// ccWarnMultipleIniValues warns about duplicate keys in JOINT_* and AXIS_*
// sections.
func ccWarnMultipleIniValues(ini *inifile.IniFile, r *configCheckResult) {
	// Accumulate entries per raw section name across all occurrences
	// (a section may appear multiple times via #INCLUDE).
	type sectionData struct {
		keyCounts map[string]int
		keyValues map[string][]string
	}
	accumulated := make(map[string]*sectionData)

	for _, sec := range ini.Sections {
		// Strip namespace prefix to get the effective section name for
		// the check.  This ensures we check each raw section individually
		// without merging namespace overrides with global defaults.
		name := sec.Name
		if ns := ini.Namespace(); ns != "" {
			name = strings.TrimPrefix(name, ns+":")
		}
		if !strings.HasPrefix(name, "JOINT_") && !strings.HasPrefix(name, "AXIS_") {
			continue
		}

		sd := accumulated[sec.Name]
		if sd == nil {
			sd = &sectionData{
				keyCounts: make(map[string]int),
				keyValues: make(map[string][]string),
			}
			accumulated[sec.Name] = sd
		}
		for _, e := range sec.Entries {
			sd.keyCounts[e.Key]++
			sd.keyValues[e.Key] = append(sd.keyValues[e.Key], e.Value)
		}
	}

	for secName, sd := range accumulated {
		// Use the effective (non-prefixed) name in the warning message.
		displayName := secName
		if ns := ini.Namespace(); ns != "" {
			displayName = strings.TrimPrefix(displayName, ns+":")
		}
		for key, count := range sd.keyCounts {
			if count > 1 {
				r.Warnings = append(r.Warnings,
					fmt.Sprintf("Unexpected multiple values [%s]%s: %s",
						displayName, key, strings.Join(sd.keyValues[key], " ")))
			}
		}
	}
}

// ccValidateIdentityKinsLimits checks joint and axis velocity, acceleration,
// and limit consistency for trivkins (identity kinematics).
func ccValidateIdentityKinsLimits(ini *inifile.IniFile, r *configCheckResult, numJoints int, coords []byte, jointIdx map[byte][]int) {
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

// ccConsistentCoordsForTrivkins checks that trivkins coordinates= matches
// [TRAJ]COORDINATES when coordinates= was explicitly specified.
func ccConsistentCoordsForTrivkins(ini *inifile.IniFile, r *configCheckResult, coords []byte, specified bool) {
	trivStr := ccStripWhitespace(string(coords))

	if !specified || strings.EqualFold(trivStr, "XYZABCUVW") {
		return
	}

	trajCoords := ccStripWhitespace(ini.Get("TRAJ", "COORDINATES"))
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

// ccStripWhitespace removes all spaces and tabs from s.
func ccStripWhitespace(s string) string {
	s = strings.ReplaceAll(s, " ", "")
	s = strings.ReplaceAll(s, "\t", "")
	return s
}
