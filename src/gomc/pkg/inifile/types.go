// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
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
	// SourceFile is the absolute path of the file this entry was parsed from.
	// Set during parsing (including through #INCLUDE chains).
	SourceFile string
	// SourceLine is the 1-based line number in SourceFile where this entry appears.
	SourceLine int
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

	// namespace is an optional prefix for section lookups. When set,
	// Get("JOINT_0", "KEY") first tries [namespace:JOINT_0], then [JOINT_0].
	// This enables multiple instances to share one INI file with per-instance
	// overrides in namespaced sections like [mill:JOINT_0].
	namespace string
}

// SourceFile returns the absolute path of the root INI file that was parsed.
func (ini *IniFile) SourceFile() string {
	return ini.sourceFile
}

// WithNamespace returns a shallow copy of the IniFile with a namespace prefix
// set for section lookups. Get("JOINT_0", "KEY") on the returned copy first
// checks [prefix:JOINT_0], then falls back to [JOINT_0].
// The underlying Sections slice is shared (not copied).
func (ini *IniFile) WithNamespace(prefix string) *IniFile {
	return &IniFile{
		Sections:   ini.Sections,
		sourceFile: ini.sourceFile,
		namespace:  prefix,
	}
}

// Namespace returns the current namespace prefix, or "" if none is set.
func (ini *IniFile) Namespace() string {
	return ini.namespace
}

// GetSection returns all entries in the named section, in the order they
// appear in the file.  If the section appears more than once (e.g. via
// #INCLUDE), the entries are concatenated in file order.  Returns nil if the
// section does not exist.
//
// When a namespace is set, entries from [namespace:section] are returned first,
// followed by entries from [section] (global fallback).
func (ini *IniFile) GetSection(section string) []Entry {
	var result []Entry
	if ini.namespace != "" {
		nsSection := ini.namespace + ":" + section
		for i := range ini.Sections {
			if ini.Sections[i].Name == nsSection {
				result = append(result, ini.Sections[i].Entries...)
			}
		}
	}
	for i := range ini.Sections {
		if ini.Sections[i].Name == section {
			result = append(result, ini.Sections[i].Entries...)
		}
	}
	return result
}
