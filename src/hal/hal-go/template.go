package hal

import (
	"bytes"
	"fmt"
	"os"
	"strconv"
	"strings"
	"text/template"
)

// HalTemplateData holds the data context available to HAL file templates.
type HalTemplateData struct {
	INI    map[string]map[string]string
	Axes   []string
	Joints int
	Env    map[string]string
}

// NewHalTemplateData creates a HalTemplateData from an INI data map.
func NewHalTemplateData(ini map[string]map[string]string) *HalTemplateData {
	data := &HalTemplateData{
		INI: ini,
		Env: make(map[string]string),
	}

	// Extract axes from [TRAJ]COORDINATES
	// Coordinates may be space-separated ("X Y Z") or concatenated ("XYZ").
	if traj, ok := ini["TRAJ"]; ok {
		if coords, ok := traj["COORDINATES"]; ok {
			for _, c := range strings.TrimSpace(coords) {
				if c != ' ' && c != '\t' {
					data.Axes = append(data.Axes, string(c))
				}
			}
		}
	}

	// Extract joints from [KINS]JOINTS
	if kins, ok := ini["KINS"]; ok {
		if joints, ok := kins["JOINTS"]; ok {
			if n, err := strconv.Atoi(strings.TrimSpace(joints)); err == nil {
				data.Joints = n
			}
		}
	}

	// Populate environment
	for _, env := range os.Environ() {
		if k, v, ok := strings.Cut(env, "="); ok {
			data.Env[k] = v
		}
	}

	return data
}

// toFloat64 coerces a numeric value to float64. Accepts int, int64, float64,
// and string (parsed via strconv.ParseFloat).
func toFloat64(v any) (float64, error) {
	switch n := v.(type) {
	case int:
		return float64(n), nil
	case int64:
		return float64(n), nil
	case float64:
		return n, nil
	case string:
		return strconv.ParseFloat(n, 64)
	default:
		return 0, fmt.Errorf("cannot convert %T to float64", v)
	}
}

// convertTwoArgs coerces two values to float64 for use in math template functions.
func convertTwoArgs(a, b any) (float64, float64, error) {
	fa, err := toFloat64(a)
	if err != nil {
		return 0, 0, err
	}
	fb, err := toFloat64(b)
	if err != nil {
		return 0, 0, err
	}
	return fa, fb, nil
}

// halTemplateFuncs returns the function map for HAL file templates.
// The ini function is a closure over the provided INI data so that templates
// can call {{ini "SECTION" "KEY"}} without explicitly passing .INI.
// hasJoint and hasAxis are closures over the HalTemplateData for INI-derived
// range checks that require no HAL runtime probing.
func halTemplateFuncs(data *HalTemplateData) template.FuncMap {
	return template.FuncMap{
		// String operations
		"lower":    strings.ToLower,
		"upper":    strings.ToUpper,
		"replace":  strings.ReplaceAll,
		"contains": strings.Contains,
		"split":    strings.Split,
		"join":     strings.Join,
		"printf":   fmt.Sprintf,
		"trim":     strings.TrimSpace,

		// Math operations — accept any numeric type via toFloat64
		"add": func(a, b any) (float64, error) {
			fa, fb, err := convertTwoArgs(a, b)
			return fa + fb, err
		},
		"sub": func(a, b any) (float64, error) {
			fa, fb, err := convertTwoArgs(a, b)
			return fa - fb, err
		},
		"mul": func(a, b any) (float64, error) {
			fa, fb, err := convertTwoArgs(a, b)
			return fa * fb, err
		},
		"div": func(a, b any) (float64, error) {
			fa, fb, err := convertTwoArgs(a, b)
			if err != nil {
				return 0, err
			}
			if fb == 0 {
				return 0, fmt.Errorf("division by zero")
			}
			return fa / fb, nil
		},
		"neg": func(a any) (float64, error) {
			fa, err := toFloat64(a)
			return -fa, err
		},

		// Iteration helpers
		"seq": func(start, end int) []int {
			result := make([]int, 0, end-start)
			for i := start; i < end; i++ {
				result = append(result, i)
			}
			return result
		},
		"count": func(n int) []int {
			result := make([]int, n)
			for i := range result {
				result[i] = i
			}
			return result
		},

		// INI access — closure over the template's own INI data
		"ini": func(section, key string) string {
			if s, ok := data.INI[section]; ok {
				if v, ok := s[key]; ok {
					return v
				}
			}
			return ""
		},

		// Environment access
		"env": os.Getenv,

		// Type conversions
		"atoi": strconv.Atoi,
		"atof": func(s string) (float64, error) {
			return strconv.ParseFloat(s, 64)
		},
		"itoa": strconv.Itoa,

		// INI-derived range checks — no HAL runtime probing needed.
		// hasJoint returns true if joint n is within the configured range [0, .Joints).
		"hasJoint": func(n int) bool {
			return n >= 0 && n < data.Joints
		},
		// hasAxis returns true if the given letter (case-insensitive) appears in .Axes.
		"hasAxis": func(letter string) bool {
			upper := strings.ToUpper(letter)
			for _, a := range data.Axes {
				if strings.ToUpper(a) == upper {
					return true
				}
			}
			return false
		},
	}
}

// RenderHalTemplate renders a HAL file through Go's text/template engine.
// Returns the rendered output as a string.
// If the input contains no template directives (no "{{"), it is returned as-is.
func RenderHalTemplate(name, content string, data *HalTemplateData) (string, error) {
	// Fast path: no template directives
	if !strings.Contains(content, "{{") {
		return content, nil
	}

	tmpl, err := template.New(name).Funcs(halTemplateFuncs(data)).Parse(content)
	if err != nil {
		return "", fmt.Errorf("template parse error in %s: %w", name, err)
	}

	var buf bytes.Buffer
	if err := tmpl.Execute(&buf, data); err != nil {
		return "", fmt.Errorf("template execute error in %s: %w", name, err)
	}

	return buf.String(), nil
}
