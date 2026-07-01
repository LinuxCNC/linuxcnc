// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package persist_sqlite implements a generic persistence gomod backed by SQLite.
//
// It registers as "persist_sqlite" and exposes the persist GMI API for
// handle-based key-value storage. Each namespace gets its own <namespace>.db
// file. Consumers call Open(namespace) to get a handle, then use the handle
// for all subsequent operations.
//
// Load: load persist_sqlite <persistence> [dbpath=<dir>]
// Default db directory: db/ next to the INI file.
package persist_sqlite

import (
	"database/sql"
	"fmt"
	"log/slog"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/persist"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"

	_ "modernc.org/sqlite"
)

func init() {
	gomc.RegisterModule("persist_sqlite", newPersistSQLite)
}

type nsHandle struct {
	namespace string
	db        *sql.DB
}

type module struct {
	logger  *slog.Logger
	dbDir   string
	mu      sync.RWMutex
	handles []nsHandle // indexed by handle value
}

func newPersistSQLite(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	dbDir := ""
	for _, arg := range args {
		if k, v, ok := strings.Cut(arg, "="); ok && k == "dbpath" {
			dbDir = v
		}
	}

	// Default: db/ directory next to INI file.
	iniDir := filepath.Dir(ini.SourceFile())
	if dbDir == "" {
		dbDir = filepath.Join(iniDir, "db")
	} else if !filepath.IsAbs(dbDir) && iniDir != "" {
		dbDir = filepath.Join(iniDir, dbDir)
	}

	// Create directory if it doesn't exist.
	if err := os.MkdirAll(dbDir, 0755); err != nil {
		return nil, fmt.Errorf("persist_sqlite: create db dir %s: %w", dbDir, err)
	}

	m := &module{logger: logger, dbDir: dbDir}

	// Register API.
	reg := apiserver.DefaultRegistry()
	if err := persist.RegisterPersistAPI(reg, name, m); err != nil {
		return nil, fmt.Errorf("persist_sqlite: register API: %w", err)
	}

	logger.Info("persist_sqlite: ready", "dir", dbDir, "instance", name)
	return m, nil
}

func (m *module) Start() error { return nil }
func (m *module) Stop()        {}
func (m *module) Destroy() {
	m.mu.Lock()
	defer m.mu.Unlock()
	for i := range m.handles {
		if m.handles[i].db != nil {
			m.handles[i].db.Close()
			m.handles[i].db = nil
		}
	}
}

// --- Validation ---

// validName checks that a namespace name is safe for use as a filename.
func validName(name string) bool {
	if name == "" {
		return false
	}
	for _, c := range name {
		if !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
			return false
		}
	}
	return true
}

// --- Handle management ---

func (m *module) getHandle(handle int32) (*nsHandle, error) {
	idx := int(handle)
	if idx < 0 || idx >= len(m.handles) || m.handles[idx].db == nil {
		return nil, fmt.Errorf("invalid handle: %d", handle)
	}
	return &m.handles[idx], nil
}

// --- Persist API implementation ---

func (m *module) Open(namespace string) (*persist.OpenResult, error) {
	if !validName(namespace) {
		return nil, fmt.Errorf("invalid namespace: %q", namespace)
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	// Check if already open — return existing handle.
	for i, h := range m.handles {
		if h.namespace == namespace && h.db != nil {
			return &persist.OpenResult{Handle: int32(i)}, nil
		}
	}

	// Open new DB file.
	dbPath := filepath.Join(m.dbDir, namespace+".db")
	db, err := sql.Open("sqlite", dbPath)
	if err != nil {
		return nil, fmt.Errorf("open %s: %w", dbPath, err)
	}
	if _, err := db.Exec("PRAGMA journal_mode=WAL"); err != nil {
		db.Close()
		return nil, fmt.Errorf("set WAL on %s: %w", dbPath, err)
	}
	if _, err := db.Exec("PRAGMA busy_timeout = 5000"); err != nil {
		db.Close()
		return nil, fmt.Errorf("set busy_timeout on %s: %w", dbPath, err)
	}
	if _, err := db.Exec(`CREATE TABLE IF NOT EXISTS entries (
		key     TEXT PRIMARY KEY,
		value   TEXT NOT NULL DEFAULT '',
		updated INTEGER NOT NULL DEFAULT 0
	)`); err != nil {
		db.Close()
		return nil, fmt.Errorf("create table in %s: %w", dbPath, err)
	}

	handle := int32(len(m.handles))
	m.handles = append(m.handles, nsHandle{namespace: namespace, db: db})
	m.logger.Debug("persist_sqlite: opened namespace", "namespace", namespace, "handle", handle)
	return &persist.OpenResult{Handle: handle}, nil
}

func (m *module) Close(handle int32) {
	// No-op: all handles are closed at Destroy().
}

func (m *module) GetNamespaces() ([]string, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	seen := make(map[string]bool)
	for _, h := range m.handles {
		if h.db != nil {
			seen[h.namespace] = true
		}
	}
	// Also scan directory for namespaces not yet opened.
	entries, err := os.ReadDir(m.dbDir)
	if err == nil {
		for _, e := range entries {
			if !e.IsDir() && strings.HasSuffix(e.Name(), ".db") {
				ns := strings.TrimSuffix(e.Name(), ".db")
				if validName(ns) {
					seen[ns] = true
				}
			}
		}
	}
	result := make([]string, 0, len(seen))
	for ns := range seen {
		result = append(result, ns)
	}
	return result, nil
}

func (m *module) GetEntries(handle int32) ([]persist.Entry, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	h, err := m.getHandle(handle)
	if err != nil {
		return nil, err
	}
	rows, err := h.db.Query(`SELECT key, value, updated FROM entries ORDER BY key`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var entries []persist.Entry
	for rows.Next() {
		var e persist.Entry
		if err := rows.Scan(&e.Key, &e.Value, &e.Updated); err != nil {
			return nil, err
		}
		entries = append(entries, e)
	}
	return entries, rows.Err()
}

func (m *module) GetEntry(handle int32, key string) (*persist.Entry, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	h, err := m.getHandle(handle)
	if err != nil {
		return nil, err
	}
	var e persist.Entry
	err = h.db.QueryRow(`SELECT key, value, updated FROM entries WHERE key = ?`, key).
		Scan(&e.Key, &e.Value, &e.Updated)
	if err == sql.ErrNoRows {
		return nil, fmt.Errorf("not found: %s/%s", h.namespace, key)
	}
	if err != nil {
		return nil, err
	}
	return &e, nil
}

func (m *module) SetEntry(handle int32, key, value string) (*persist.SetResult, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	h, err := m.getHandle(handle)
	if err != nil {
		return &persist.SetResult{Ok: false}, err
	}
	now := time.Now().Unix()
	_, err = h.db.Exec(
		`INSERT INTO entries (key, value, updated) VALUES (?, ?, ?)
		 ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated = excluded.updated`,
		key, value, now,
	)
	if err != nil {
		return &persist.SetResult{Ok: false}, err
	}
	return &persist.SetResult{Ok: true}, nil
}

func (m *module) DeleteEntry(handle int32, key string) (*persist.DeleteResult, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	h, err := m.getHandle(handle)
	if err != nil {
		return &persist.DeleteResult{Ok: false}, err
	}
	res, err := h.db.Exec(`DELETE FROM entries WHERE key = ?`, key)
	if err != nil {
		return &persist.DeleteResult{Ok: false}, err
	}
	n, _ := res.RowsAffected()
	return &persist.DeleteResult{Ok: n > 0, Count: int32(n)}, nil
}

func (m *module) SetEntries(handle int32, entries []persist.Entry) (*persist.SetResult, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	h, err := m.getHandle(handle)
	if err != nil {
		return &persist.SetResult{Ok: false}, err
	}
	tx, err := h.db.Begin()
	if err != nil {
		return &persist.SetResult{Ok: false}, err
	}
	defer tx.Rollback()
	now := time.Now().Unix()
	stmt, err := tx.Prepare(
		`INSERT INTO entries (key, value, updated) VALUES (?, ?, ?)
		 ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated = excluded.updated`)
	if err != nil {
		return &persist.SetResult{Ok: false}, err
	}
	defer stmt.Close()
	for _, e := range entries {
		ts := e.Updated
		if ts == 0 {
			ts = now
		}
		if _, err := stmt.Exec(e.Key, e.Value, ts); err != nil {
			return &persist.SetResult{Ok: false}, err
		}
	}
	if err := tx.Commit(); err != nil {
		return &persist.SetResult{Ok: false}, err
	}
	return &persist.SetResult{Ok: true}, nil
}

func (m *module) DeleteAll(handle int32) (*persist.DeleteResult, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	h, err := m.getHandle(handle)
	if err != nil {
		return &persist.DeleteResult{Ok: false}, err
	}
	// Close the DB, remove files.
	h.db.Close()
	dbPath := filepath.Join(m.dbDir, h.namespace+".db")
	os.Remove(dbPath)
	os.Remove(dbPath + "-wal")
	os.Remove(dbPath + "-shm")
	h.db = nil
	return &persist.DeleteResult{Ok: true, Count: 1}, nil
}
