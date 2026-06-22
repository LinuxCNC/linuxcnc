// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package cgen

import (
	"fmt"
	"io"
	"strings"

	"github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/ast"
)

// EmitMeta writes author and license comment lines to w using the given
// comment prefix (e.g. "//" for C/Go/TS, "#" for Python).
func EmitMeta(w io.Writer, api *ast.API, prefix string) {
	for _, a := range api.Authors {
		fmt.Fprintf(w, "%s Authors: %s\n", prefix, a)
	}
	if api.License != "" {
		fmt.Fprintf(w, "%s License: %s\n", prefix, api.License)
	}
}

// MetaLines returns the author/license block as a string using the given
// comment prefix. Includes a trailing newline if non-empty.
func MetaLines(api *ast.API, prefix string) string {
	if len(api.Authors) == 0 && api.License == "" {
		return ""
	}
	var b strings.Builder
	for _, a := range api.Authors {
		fmt.Fprintf(&b, "%s Authors: %s\n", prefix, a)
	}
	if api.License != "" {
		fmt.Fprintf(&b, "%s License: %s\n", prefix, api.License)
	}
	return b.String()
}
