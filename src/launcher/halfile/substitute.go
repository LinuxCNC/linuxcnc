package halfile

// substituteLine replaces all [SECTION]KEY patterns in line with the
// corresponding values from the INI file.  Patterns for which no matching
// entry exists are left unchanged.
//
// This mirrors the behaviour of "halcmd -i <file>" INI variable substitution
// but is applied in Go so that the substituted text can be inspected or
// written to a temporary file when necessary.
func (e *Executor) substituteLine(line string) string {
	if e.ini == nil {
		return line
	}
	return e.ini.Substitute(line)
}
