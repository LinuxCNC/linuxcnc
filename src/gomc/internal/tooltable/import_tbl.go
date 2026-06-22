// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package tooltable

import (
	"bufio"
	"fmt"
	"os"
	"strconv"
	"strings"
)

// importTbl parses a legacy LinuxCNC .tbl file and inserts tools into the database.
func (m *module) importTbl(path string) error {
	f, err := os.Open(path)
	if err != nil {
		return err
	}
	defer f.Close()

	tx, err := m.db.Begin()
	if err != nil {
		return err
	}
	defer tx.Rollback()

	stmt, err := tx.Prepare(`INSERT OR REPLACE INTO tools
		(toolno, pocketno, x_offset, y_offset, z_offset,
		 a_offset, b_offset, c_offset, u_offset, v_offset, w_offset,
		 diameter, frontangle, backangle, orientation, comment)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`)
	if err != nil {
		return err
	}
	defer stmt.Close()

	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" || line[0] == ';' {
			continue
		}
		t, err := parseTblLine(line)
		if err != nil {
			continue // skip unparsable lines
		}
		if _, err := stmt.Exec(t.toolno, t.pocketno,
			t.x, t.y, t.z, t.a, t.b, t.c, t.u, t.v, t.w,
			t.diameter, t.frontangle, t.backangle, t.orientation, t.comment); err != nil {
			return fmt.Errorf("insert tool %d: %w", t.toolno, err)
		}
	}
	if err := scanner.Err(); err != nil {
		return err
	}
	return tx.Commit()
}

type tblEntry struct {
	toolno, pocketno, orientation   int32
	x, y, z, a, b, c, u, v, w       float64
	diameter, frontangle, backangle float64
	comment                         string
}

// parseTblLine parses a line in LinuxCNC .tbl format:
// T<n> P<n> X<f> Y<f> Z<f> A<f> B<f> C<f> U<f> V<f> W<f> D<f> I<f> J<f> Q<n> ;<comment>
func parseTblLine(line string) (tblEntry, error) {
	var t tblEntry

	// Extract comment (everything after ;)
	if idx := strings.Index(line, ";"); idx >= 0 {
		t.comment = strings.TrimSpace(line[idx+1:])
		line = line[:idx]
	}

	fields := strings.Fields(line)
	if len(fields) == 0 {
		return t, fmt.Errorf("empty line")
	}

	for _, field := range fields {
		if len(field) < 2 {
			continue
		}
		key := field[0]
		val := field[1:]
		switch key {
		case 'T':
			n, err := strconv.ParseInt(val, 10, 32)
			if err != nil {
				return t, err
			}
			t.toolno = int32(n)
		case 'P':
			n, err := strconv.ParseInt(val, 10, 32)
			if err != nil {
				return t, err
			}
			t.pocketno = int32(n)
		case 'X':
			t.x, _ = strconv.ParseFloat(val, 64)
		case 'Y':
			t.y, _ = strconv.ParseFloat(val, 64)
		case 'Z':
			t.z, _ = strconv.ParseFloat(val, 64)
		case 'A':
			t.a, _ = strconv.ParseFloat(val, 64)
		case 'B':
			t.b, _ = strconv.ParseFloat(val, 64)
		case 'C':
			t.c, _ = strconv.ParseFloat(val, 64)
		case 'U':
			t.u, _ = strconv.ParseFloat(val, 64)
		case 'V':
			t.v, _ = strconv.ParseFloat(val, 64)
		case 'W':
			t.w, _ = strconv.ParseFloat(val, 64)
		case 'D':
			t.diameter, _ = strconv.ParseFloat(val, 64)
		case 'I':
			t.frontangle, _ = strconv.ParseFloat(val, 64)
		case 'J':
			t.backangle, _ = strconv.ParseFloat(val, 64)
		case 'Q':
			n, _ := strconv.ParseInt(val, 10, 32)
			t.orientation = int32(n)
		}
	}

	if t.toolno == 0 {
		return t, fmt.Errorf("no tool number")
	}
	return t, nil
}
