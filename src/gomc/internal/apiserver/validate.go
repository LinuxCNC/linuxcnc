// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package apiserver

import (
	"fmt"
	"regexp"
	"unicode/utf8"
)

// ValidationError is returned by generated dispatch wrappers when an incoming
// request field violates an @constraint declared in the .gmi IDL. The HTTP
// layer (writeDispatchError) renders it as a 400 with a structured body that
// names the offending field and rule.
//
// Keeping the regex and rune-counting helpers here — rather than in generated
// code — also keeps @regex enforcement structurally Go-server-only: these
// helpers exist only in this package.
type ValidationError struct {
	Field      string `json:"field"`      // dotted path, e.g. "entry.diameter"
	Constraint string `json:"constraint"` // "min", "maxlen", "regex", "enum", ...
	Message    string `json:"message"`    // human-readable
}

func (e *ValidationError) Error() string { return e.Message }

// NewValidationError builds a *ValidationError. Generated code calls this.
func NewValidationError(field, constraint, msg string) *ValidationError {
	return &ValidationError{Field: field, Constraint: constraint, Message: msg}
}

// RuneLen is the length @minlen/@maxlen check for strings: characters, not
// bytes. Slices/arrays use the built-in len().
func RuneLen(s string) int { return utf8.RuneCountInString(s) }

// MustRegex compiles a pattern at package-init time. A pattern that does not
// compile is a codegen bug (the compiler already rejects it via check.Validate),
// so it should crash loudly at startup rather than per request.
func MustRegex(pattern string) *regexp.Regexp { return regexp.MustCompile(pattern) }

// ValidateRegex reports a *ValidationError if v does not fully match re. This is
// the only place regex validation runs — Go server side only, by design.
func ValidateRegex(field string, re *regexp.Regexp, v string) *ValidationError {
	if !re.MatchString(v) {
		return NewValidationError(field, "regex",
			fmt.Sprintf("%s: %q does not match /%s/", field, v, re.String()))
	}
	return nil
}
