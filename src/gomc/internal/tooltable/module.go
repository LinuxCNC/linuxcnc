// Package tooltable implements a tool table gomod backed by SQLite.
//
// It registers as "tooltable" and exposes the tooltable GMI API for CRUD
// operations on tools. Each instance opens its own SQLite database file.
// On first run, if a legacy .tbl file exists, it is imported automatically.
package tooltable

import (
	"database/sql"
	"fmt"
	"log/slog"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/tooltable"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/pkg/gomc"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"

	_ "modernc.org/sqlite"
)

func init() {
	gomc.RegisterModule("tooltable", newTooltable)
}

type module struct {
	logger *slog.Logger
	db     *sql.DB
	mu     sync.RWMutex
}

func newTooltable(ini *inifile.IniFile, logger *slog.Logger, name string, args []string) (gomc.Module, error) {
	dbPath := ""
	for _, arg := range args {
		if k, v, ok := strings.Cut(arg, "="); ok && k == "db" {
			dbPath = v
		}
	}
	if dbPath == "" {
		if v := ini.Get("EMCIO", "TOOL_TABLE"); v != "" {
			if strings.HasSuffix(v, ".tbl") {
				dbPath = strings.TrimSuffix(v, ".tbl") + ".db"
			} else {
				dbPath = v
			}
		}
	}
	if dbPath == "" {
		dbPath = "tool.db"
	}

	// Resolve relative to INI directory.
	iniDir := filepath.Dir(ini.SourceFile())
	if !filepath.IsAbs(dbPath) && iniDir != "" {
		dbPath = filepath.Join(iniDir, dbPath)
	}

	// Check if DB exists (for import decision).
	_, dbErr := os.Stat(dbPath)
	needsImport := os.IsNotExist(dbErr)

	db, err := sql.Open("sqlite", dbPath)
	if err != nil {
		return nil, fmt.Errorf("tooltable: open db %s: %w", dbPath, err)
	}

	if _, err := db.Exec("PRAGMA journal_mode=WAL"); err != nil {
		db.Close()
		return nil, fmt.Errorf("tooltable: set WAL: %w", err)
	}

	if err := createSchema(db); err != nil {
		db.Close()
		return nil, fmt.Errorf("tooltable: create schema: %w", err)
	}

	m := &module{logger: logger, db: db}

	// Import legacy .tbl if this is the first run.
	if needsImport {
		tblPath := ""
		if v := ini.Get("EMCIO", "TOOL_TABLE"); strings.HasSuffix(v, ".tbl") {
			tblPath = v
			if !filepath.IsAbs(tblPath) && iniDir != "" {
				tblPath = filepath.Join(iniDir, tblPath)
			}
		}
		if tblPath != "" {
			if err := m.importTbl(tblPath); err != nil {
				logger.Warn("tooltable: import .tbl failed", "path", tblPath, "error", err)
			} else {
				logger.Info("tooltable: imported legacy .tbl", "path", tblPath)
			}
		}
	}

	// Register API.
	reg := apiserver.DefaultRegistry()
	if err := tooltable.RegisterTooltableAPI(reg, name, m); err != nil {
		db.Close()
		return nil, fmt.Errorf("tooltable: register API: %w", err)
	}

	logger.Info("tooltable: ready", "db", dbPath, "instance", name)
	return m, nil
}

func (m *module) Start() error { return nil }
func (m *module) Stop()        {}
func (m *module) Destroy() {
	if m.db != nil {
		m.db.Close()
	}
}

// --- Schema ---

func createSchema(db *sql.DB) error {
	_, err := db.Exec(`CREATE TABLE IF NOT EXISTS tools (
		toolno     INTEGER PRIMARY KEY,
		pocketno   INTEGER NOT NULL DEFAULT 0,
		x_offset   REAL NOT NULL DEFAULT 0,
		y_offset   REAL NOT NULL DEFAULT 0,
		z_offset   REAL NOT NULL DEFAULT 0,
		a_offset   REAL NOT NULL DEFAULT 0,
		b_offset   REAL NOT NULL DEFAULT 0,
		c_offset   REAL NOT NULL DEFAULT 0,
		u_offset   REAL NOT NULL DEFAULT 0,
		v_offset   REAL NOT NULL DEFAULT 0,
		w_offset   REAL NOT NULL DEFAULT 0,
		diameter   REAL NOT NULL DEFAULT 0,
		frontangle REAL NOT NULL DEFAULT 0,
		backangle  REAL NOT NULL DEFAULT 0,
		orientation INTEGER NOT NULL DEFAULT 0,
		comment    TEXT NOT NULL DEFAULT ''
	)`)
	return err
}

// --- TooltableCallbacks implementation ---

func (m *module) ListTools() ([]tooltable.ToolEntry, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	rows, err := m.db.Query(`SELECT toolno, pocketno, x_offset, y_offset, z_offset,
		a_offset, b_offset, c_offset, u_offset, v_offset, w_offset,
		diameter, frontangle, backangle, orientation, comment FROM tools ORDER BY toolno`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var tools []tooltable.ToolEntry
	for rows.Next() {
		var t tooltable.ToolEntry
		if err := rows.Scan(&t.Toolno, &t.Pocketno,
			&t.XOffset, &t.YOffset, &t.ZOffset,
			&t.AOffset, &t.BOffset, &t.COffset,
			&t.UOffset, &t.VOffset, &t.WOffset,
			&t.Diameter, &t.Frontangle, &t.Backangle,
			&t.Orientation, &t.Comment); err != nil {
			return nil, err
		}
		tools = append(tools, t)
	}
	return tools, rows.Err()
}

func (m *module) GetTool(toolno int32) (*tooltable.ToolEntry, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	var t tooltable.ToolEntry
	err := m.db.QueryRow(`SELECT toolno, pocketno, x_offset, y_offset, z_offset,
		a_offset, b_offset, c_offset, u_offset, v_offset, w_offset,
		diameter, frontangle, backangle, orientation, comment FROM tools WHERE toolno = ?`, toolno).
		Scan(&t.Toolno, &t.Pocketno,
			&t.XOffset, &t.YOffset, &t.ZOffset,
			&t.AOffset, &t.BOffset, &t.COffset,
			&t.UOffset, &t.VOffset, &t.WOffset,
			&t.Diameter, &t.Frontangle, &t.Backangle,
			&t.Orientation, &t.Comment)
	if err == sql.ErrNoRows {
		return &tooltable.ToolEntry{}, nil
	}
	if err != nil {
		return nil, err
	}
	return &t, nil
}

func (m *module) PutTool(toolno int32, entry tooltable.ToolEntry) (*tooltable.PutToolResult, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	_, err := m.db.Exec(`INSERT OR REPLACE INTO tools
		(toolno, pocketno, x_offset, y_offset, z_offset,
		 a_offset, b_offset, c_offset, u_offset, v_offset, w_offset,
		 diameter, frontangle, backangle, orientation, comment)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
		toolno, entry.Pocketno,
		entry.XOffset, entry.YOffset, entry.ZOffset,
		entry.AOffset, entry.BOffset, entry.COffset,
		entry.UOffset, entry.VOffset, entry.WOffset,
		entry.Diameter, entry.Frontangle, entry.Backangle,
		entry.Orientation, entry.Comment)
	if err != nil {
		return nil, err
	}
	return &tooltable.PutToolResult{Ok: true, Index: toolno}, nil
}

func (m *module) DeleteTool(toolno int32) (*tooltable.DeleteResult, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	res, err := m.db.Exec(`DELETE FROM tools WHERE toolno = ?`, toolno)
	if err != nil {
		return nil, err
	}
	n, _ := res.RowsAffected()
	return &tooltable.DeleteResult{Ok: n > 0}, nil
}
