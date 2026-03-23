// Package launcher — validate.go implements cross-section INI dependency
// validation that runs after INI parsing but before any subprocess is started.
package launcher

import "fmt"

// validateDependencies checks cross-section INI dependencies and returns an
// error if the configuration is contradictory or incomplete.
//
// Rules (checked for all modes):
//   - At least one [HAL]HALFILE is required.
//
// Rules (cross-section dependencies):
//   - [HAL]HALUI without [TASK]TASK → error (halui communicates via NML).
//   - [EMCIO]EMCIO or [IO]IO without [TASK]TASK → error (iocontrol communicates via NML).
//
// Rules (when [TASK]TASK is set):
//   - [KINS]KINEMATICS is required.
//   - [TRAJ]COORDINATES is required.
//   - [EMCMOT]SERVO_PERIOD is required.
//   - [RS274NGC]PARAMETER_FILE is required.
func (l *Launcher) validateDependencies() error {
	hasTask := l.ini.Get("TASK", "TASK") != ""
	hasHalUI := l.ini.Get("HAL", "HALUI") != ""
	hasIO := l.ini.Get("IO", "IO") != "" || l.ini.Get("EMCIO", "EMCIO") != ""
	halFiles := l.ini.GetAll("HAL", "HALFILE")

	// At least one [HAL]HALFILE is required in all modes.
	if len(halFiles) == 0 {
		return fmt.Errorf("at least one [HAL]HALFILE is required")
	}

	// [HAL]HALUI requires [TASK]TASK (halui communicates with the task controller via NML).
	if hasHalUI && !hasTask {
		return fmt.Errorf("[HAL]HALUI is set but [TASK]TASK is missing — halui requires the task controller")
	}

	// [EMCIO]EMCIO or [IO]IO requires [TASK]TASK (iocontrol communicates with task via NML).
	if hasIO && !hasTask {
		return fmt.Errorf("IO controller is configured ([EMCIO]EMCIO or [IO]IO) but [TASK]TASK is missing — iocontrol requires the task controller")
	}

	// [TASK]TASK requires certain configuration sections.
	if hasTask {
		if l.ini.Get("KINS", "KINEMATICS") == "" {
			return fmt.Errorf("[TASK]TASK is set but [KINS]KINEMATICS is missing")
		}
		if l.ini.Get("TRAJ", "COORDINATES") == "" {
			return fmt.Errorf("[TASK]TASK is set but [TRAJ]COORDINATES is missing")
		}
		if l.ini.Get("EMCMOT", "SERVO_PERIOD") == "" {
			return fmt.Errorf("[TASK]TASK is set but [EMCMOT]SERVO_PERIOD is missing")
		}
		if l.ini.Get("RS274NGC", "PARAMETER_FILE") == "" {
			return fmt.Errorf("[TASK]TASK is set but [RS274NGC]PARAMETER_FILE is missing")
		}
	}

	return nil
}
