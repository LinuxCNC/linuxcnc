// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package tooltable implements a tool table gomod using the persist API.
//
// It registers as "tooltable" and exposes the tooltable GMI API for CRUD
// operations on tools. Storage is delegated to the generic persistence
// service (looked up as "persistence" by default, overrideable via
// persistence=<instance>).
//
// On first run, if a legacy .tbl file exists, it is imported automatically.
package tooltable

import (
	"encoding/json"
	"fmt"
	"log/slog"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"unsafe"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/persist"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/tooltable"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

const persistNamespace = "tooltable"

func init() {
	gomc.RegisterModule("tooltable", newTooltable)
}

type module struct {
	logger          *slog.Logger
	name            string
	ini             *inifile.IniFile
	persistInstance string
	db              *persist.PersistClient
	dbHandle        int32
	mu              sync.RWMutex
}

func newTooltable(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	persistInst := "persistence"
	for _, arg := range args {
		if k, v, ok := strings.Cut(arg, "="); ok && k == "persist_instance" {
			persistInst = v
		}
	}

	m := &module{
		logger:          logger,
		name:            name,
		ini:             ini,
		persistInstance: persistInst,
	}

	// Register tooltable API (actual persist lookup deferred to Start).
	reg := apiserver.DefaultRegistry()
	if err := tooltable.RegisterTooltableAPI(reg, name, m); err != nil {
		return nil, fmt.Errorf("tooltable: register API: %w", err)
	}

	logger.Info("tooltable: registered", "instance", name, "persistence", persistInst)
	return m, nil
}

func (m *module) Start() error {
	// Look up the persist API.
	reg := apiserver.DefaultRegistry()
	cbs, err := reg.GetAPIFor(m.name, "persist", m.persistInstance, 2)
	if err != nil {
		return fmt.Errorf("tooltable: persist API lookup (%s): %w", m.persistInstance, err)
	}
	m.db = persist.NewPersistClient(unsafe.Pointer(cbs))

	// Open the tooltable namespace.
	res, err := m.db.Open(persistNamespace)
	if err != nil {
		return fmt.Errorf("tooltable: persist open namespace: %w", err)
	}
	m.dbHandle = res.Handle

	// Import legacy .tbl if namespace is empty.
	entries, _ := m.db.GetEntries(m.dbHandle)
	if len(entries) == 0 {
		m.tryImportLegacy()
	}

	m.logger.Info("tooltable: ready", "persistence", m.persistInstance)
	return nil
}

func (m *module) Stop()    {}
func (m *module) Destroy() {}

// tryImportLegacy attempts to import a legacy .tbl file on first run.
func (m *module) tryImportLegacy() {
	v := m.ini.Get("EMCIO", "TOOL_TABLE")
	if !strings.HasSuffix(v, ".tbl") {
		return
	}
	tblPath := v
	iniDir := filepath.Dir(m.ini.SourceFile())
	if !filepath.IsAbs(tblPath) && iniDir != "" {
		tblPath = filepath.Join(iniDir, tblPath)
	}
	if _, err := os.Stat(tblPath); err != nil {
		return
	}
	if err := m.importTbl(tblPath); err != nil {
		m.logger.Warn("tooltable: import .tbl failed", "path", tblPath, "error", err)
	} else {
		m.logger.Info("tooltable: imported legacy .tbl", "path", tblPath)
	}
}

// --- TooltableCallbacks implementation ---

func (m *module) ListTools() ([]tooltable.ToolEntry, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	entries, err := m.db.GetEntries(m.dbHandle)
	if err != nil {
		return nil, err
	}

	tools := make([]tooltable.ToolEntry, 0, len(entries))
	for _, e := range entries {
		var t tooltable.ToolEntry
		if err := json.Unmarshal([]byte(e.Value), &t); err != nil {
			continue
		}
		tools = append(tools, t)
	}
	return tools, nil
}

func (m *module) GetTool(toolno int32) (*tooltable.ToolEntry, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	key := strconv.FormatInt(int64(toolno), 10)
	entry, err := m.db.GetEntry(m.dbHandle, key)
	if err != nil {
		return &tooltable.ToolEntry{}, nil
	}

	var t tooltable.ToolEntry
	if err := json.Unmarshal([]byte(entry.Value), &t); err != nil {
		return nil, err
	}
	return &t, nil
}

func (m *module) PutTool(toolno int32, entry tooltable.ToolEntry) (*tooltable.PutToolResult, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	entry.Toolno = toolno
	data, err := json.Marshal(entry)
	if err != nil {
		return nil, err
	}

	key := strconv.FormatInt(int64(toolno), 10)
	if _, err := m.db.SetEntry(m.dbHandle, key, string(data)); err != nil {
		return nil, err
	}
	return &tooltable.PutToolResult{Ok: true, Index: toolno}, nil
}

func (m *module) DeleteTool(toolno int32) (*tooltable.DeleteResult, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	key := strconv.FormatInt(int64(toolno), 10)
	res, err := m.db.DeleteEntry(m.dbHandle, key)
	if err != nil {
		return nil, err
	}
	return &tooltable.DeleteResult{Ok: res.Ok}, nil
}
