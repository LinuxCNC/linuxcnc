// Package inifile provides a parser for the LinuxCNC INI file format.
//
// The LinuxCNC INI format supports:
//   - Section headers: [SECTION_NAME]
//   - Key-value pairs: KEY = VALUE
//   - Comments: lines starting with # (excluding #INCLUDE) or ;
//   - #INCLUDE directives for recursive file inclusion
//   - Repeated keys (all values are preserved)
package inifile

// Entry represents a single key-value pair within an INI section.
type Entry struct {
	// Key is the entry key (left side of =).
	Key string
	// Value is the entry value (right side of =, trimmed, inline comments stripped).
	Value string
}

// Section represents a named section in an INI file.
type Section struct {
	// Name is the section name (without brackets).
	Name string
	// Entries contains all key-value pairs in this section, in order.
	// The same key may appear multiple times.
	Entries []Entry
}

// IniFile holds the fully parsed content of one or more INI files
// (including any files pulled in via #INCLUDE).
type IniFile struct {
	// Sections contains all parsed sections, in the order they were encountered.
	Sections []Section

	// sourceFile is the path of the root file being parsed (used in error messages).
	sourceFile string
}

// SourceFile returns the absolute path of the root INI file that was parsed.
func (ini *IniFile) SourceFile() string {
	return ini.sourceFile
}

// GetSection returns all entries in the named section, in the order they
// appear in the file.  If the section appears more than once (e.g. via
// #INCLUDE), the entries are concatenated in file order.  Returns nil if the
// section does not exist.
func (ini *IniFile) GetSection(section string) []Entry {
	var result []Entry
	for i := range ini.Sections {
		if ini.Sections[i].Name == section {
			result = append(result, ini.Sections[i].Entries...)
		}
	}
	return result
}
