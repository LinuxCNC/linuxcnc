// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package docgen generates man pages from parsed .comp AST.
package docgen

import (
	"fmt"
	"io"
	"strings"
	"time"

	"github.com/sittner/linuxcnc/src/gomc/internal/modcompile/ast"
)

// Generate writes a troff-formatted man page to w.
func Generate(w io.Writer, pkg *ast.Package) error {
	c := &pkg.Component
	section := "9"
	if c.Options["userspace"] == "yes" {
		section = "1"
	}

	// Header comment
	fmt.Fprintf(w, `.\" -*- mode: troff; coding: utf-8 -*-
.\"*******************************************************************
.\"
.\" This file was generated from %s using modcompile.
.\" Modify the source file.
.\"
.\"*******************************************************************

`, c.Pos.File)

	// Title
	fmt.Fprintf(w, ".TH %s \"%s\" \"%s\" \"LinuxCNC Documentation\" \"HAL Component\"\n",
		strings.ToUpper(c.Name), section, time.Now().Format("2006-01-02"))

	// NAME section - use Summary (from component line), first line only
	fmt.Fprintln(w, ".SH NAME")
	fmt.Fprintln(w)
	summary := c.Summary
	// Only take first line of Summary
	if idx := strings.Index(summary, "\n"); idx >= 0 {
		summary = summary[:idx]
	}
	// If no Summary, fall back to first line of Description
	if summary == "" && c.Description != "" {
		summary = c.Description
		if idx := strings.Index(summary, "\n"); idx >= 0 {
			summary = summary[:idx]
		}
	}
	if summary != "" {
		fmt.Fprintf(w, "%s \\- %s\n", c.Name, summary)
	} else {
		fmt.Fprintf(w, "%s\n", c.Name)
	}

	// SYNOPSIS section
	fmt.Fprintln(w, ".SH SYNOPSIS")
	if c.Options["userspace"] == "yes" {
		fmt.Fprintf(w, ".B %s\n", c.Name)
	} else {
		fmt.Fprintln(w, ".HP")
		// Build the load command
		fmt.Fprintf(w, ".B load %s [%s.\\fIN\\fB]", c.Name, c.Name)
		for _, mp := range c.Modparams {
			if mp.Type == "string" {
				fmt.Fprintf(w, " [%s=\\fISTR\\fB]", mp.Name)
			} else {
				fmt.Fprintf(w, " [%s=\\fIN\\fB]", mp.Name)
			}
		}
		fmt.Fprintln(w)

		// Document modparams if they have descriptions
		hasModparamDoc := false
		for _, mp := range c.Modparams {
			if mp.Doc != "" {
				hasModparamDoc = true
				break
			}
		}
		if hasModparamDoc {
			fmt.Fprintln(w, ".RS 4")
			for _, mp := range c.Modparams {
				fmt.Fprintln(w, ".TP")
				fmt.Fprintf(w, "\\fB%s\\fR", mp.Name)
				if mp.Default != "" {
					fmt.Fprintf(w, " [default: %s]", mp.Default)
				}
				fmt.Fprintln(w)
				if mp.Doc != "" {
					fmt.Fprintln(w, mp.Doc)
				}
			}
			fmt.Fprintln(w, ".RE")
		}
	}

	// DESCRIPTION section
	if c.Description != "" {
		fmt.Fprintln(w, ".SH DESCRIPTION")
		fmt.Fprintln(w)
		fmt.Fprintln(w, c.Description)
	}

	// FUNCTIONS section
	if len(c.Functions) > 0 {
		fmt.Fprintln(w, ".SH FUNCTIONS")
		for _, fn := range c.Functions {
			fmt.Fprintln(w, ".TP")
			name := fn.Name
			if name == "_" {
				name = c.Name
			} else if !strings.Contains(name, ".") {
				name = c.Name + "." + name
			}
			fmt.Fprintf(w, "\\fB%s.\\fIN\\fB\\fR", name)
			if fn.FP {
				fmt.Fprintln(w, " (requires a floating-point thread)")
			} else {
				fmt.Fprintln(w)
			}
			if fn.Doc != "" {
				fmt.Fprintln(w, fn.Doc)
			}
		}
	}

	// PINS section
	if len(c.Pins) > 0 {
		fmt.Fprintln(w, ".SH PINS")
		lead := ".TP"
		for _, pin := range c.Pins {
			fmt.Fprintln(w, lead)
			name := toHALMan(c.Name, pin.Name)
			fmt.Fprintf(w, ".B %s\\fR %s %s", name, pin.Type, pin.Dir)
			if pin.ArraySize > 0 {
				nHash := strings.Count(pin.Name, "#")
				if nHash == 0 {
					nHash = 1
				}
				if pin.ArrayPersonality != "" {
					fmt.Fprintf(w, " (M=%0*d..%s)", nHash, 0, pin.ArrayPersonality)
				} else {
					fmt.Fprintf(w, " (M=%0*d..%0*d)", nHash, 0, nHash, pin.ArraySize-1)
				}
			}
			if pin.Personality != "" {
				fmt.Fprintf(w, " [if %s]", pin.Personality)
			}
			if pin.Default != "" {
				fmt.Fprintf(w, " \\fR(default: \\fI%s\\fR)", pin.Default)
			}
			fmt.Fprintln(w, " \\fR")
			if pin.Doc != "" {
				fmt.Fprintln(w, pin.Doc)
				lead = ".TP"
			} else {
				lead = ".br\n.ns\n.TP"
			}
		}
	}

	// PARAMETERS section
	if len(c.Params) > 0 {
		fmt.Fprintln(w, ".SH PARAMETERS")
		lead := ".TP"
		for _, param := range c.Params {
			fmt.Fprintln(w, lead)
			name := toHALMan(c.Name, param.Name)
			fmt.Fprintf(w, ".B %s\\fR %s %s", name, param.Type, param.Dir)
			if param.ArraySize > 0 {
				nHash := strings.Count(param.Name, "#")
				if nHash == 0 {
					nHash = 1
				}
				if param.ArrayPersonality != "" {
					fmt.Fprintf(w, " (M=%0*d..%s)", nHash, 0, param.ArrayPersonality)
				} else {
					fmt.Fprintf(w, " (M=%0*d..%0*d)", nHash, 0, nHash, param.ArraySize-1)
				}
			}
			if param.Personality != "" {
				fmt.Fprintf(w, " [if %s]", param.Personality)
			}
			if param.Default != "" {
				fmt.Fprintf(w, " \\fR(default: \\fI%s\\fR)", param.Default)
			}
			fmt.Fprintln(w, " \\fR")
			if param.Doc != "" {
				fmt.Fprintln(w, param.Doc)
				lead = ".TP"
			} else {
				lead = ".br\n.ns\n.TP"
			}
		}
	}

	// EXAMPLES section
	if c.Examples != "" {
		fmt.Fprintln(w, ".SH EXAMPLES")
		fmt.Fprintln(w)
		fmt.Fprintln(w, c.Examples)
	}

	// SEE ALSO section
	if c.SeeAlso != "" {
		fmt.Fprintln(w, ".SH SEE ALSO")
		fmt.Fprintln(w)
		fmt.Fprintln(w, c.SeeAlso)
	}

	// NOTES section
	if c.Notes != "" {
		fmt.Fprintln(w, ".SH NOTES")
		fmt.Fprintln(w)
		fmt.Fprintln(w, c.Notes)
	}

	// AUTHOR section
	if c.Author != "" {
		fmt.Fprintln(w, ".SH AUTHOR")
		fmt.Fprintln(w)
		fmt.Fprintln(w, c.Author)
	}

	// LICENSE section
	if c.License != "" {
		fmt.Fprintln(w, ".SH LICENSE")
		fmt.Fprintln(w)
		fmt.Fprintln(w, c.License)
	}

	return nil
}

// toHALMan converts a pin/param name to man page format.
// e.g., "in-#" becomes "comp.\fIN\fB.in-\fIM\fB"
func toHALMan(compName, name string) string {
	// Replace # with \fIM\fB for array indices
	name = strings.ReplaceAll(name, "#", "\\fIM\\fB")
	// Replace _ with - for HAL naming convention
	name = strings.ReplaceAll(name, "_", "-")
	return fmt.Sprintf("%s.\\fIN\\fB.%s", compName, name)
}
