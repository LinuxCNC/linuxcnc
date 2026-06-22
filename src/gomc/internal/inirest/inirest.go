// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package inirest exposes the parsed INI file via the generated ini GMI API.
package inirest

import (
	"fmt"
	"os"
	"path/filepath"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/ini"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

type iniImpl struct {
	ini *inifile.IniFile
}

func (im *iniImpl) Query(items []ini.IniQueryItem) ([]ini.IniQueryResult, error) {
	if im.ini == nil {
		return nil, fmt.Errorf("INI file not loaded")
	}

	results := make([]ini.IniQueryResult, len(items))
	for i, q := range items {
		iniFile := im.ini
		if q.Namespace != "" {
			iniFile = iniFile.WithNamespace(q.Namespace)
		}
		if q.All != nil && *q.All {
			vals := iniFile.GetAll(q.Section, q.Key)
			if vals == nil {
				vals = []string{}
			}
			results[i] = ini.IniQueryResult{Values: vals}
		} else {
			v := iniFile.Get(q.Section, q.Key)
			if v == "" && !im.keyExists(q.Section, q.Key) {
				results[i] = ini.IniQueryResult{}
			} else {
				results[i] = ini.IniQueryResult{Value: v}
			}
		}
	}
	return results, nil
}

func (im *iniImpl) GetParameterFile(namespace string) (string, error) {
	if im.ini == nil {
		return "", fmt.Errorf("INI file not loaded")
	}
	ini := im.ini
	if namespace != "" {
		ini = ini.WithNamespace(namespace)
	}
	rel := ini.Get("RS274NGC", "PARAMETER_FILE")
	if rel == "" {
		return "", fmt.Errorf("[RS274NGC]PARAMETER_FILE not set")
	}
	path := rel
	if !filepath.IsAbs(path) {
		path = filepath.Join(filepath.Dir(im.ini.SourceFile()), rel)
	}
	data, err := os.ReadFile(path)
	if err != nil {
		return "", fmt.Errorf("paramfile: %w", err)
	}
	return string(data), nil
}

func (im *iniImpl) keyExists(section, key string) bool {
	entries := im.ini.GetSection(section)
	for _, e := range entries {
		if e.Key == key {
			return true
		}
	}
	return false
}

// Register registers the INI REST API with the given registry.
func Register(reg *apiserver.Registry, parsed *inifile.IniFile) error {
	apiserver.RegisterMeta(ini.IniMeta)
	impl := &iniImpl{ini: parsed}
	return ini.RegisterIniAPI(reg, "ini", impl)
}
