// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package tooltable

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"
	"strconv"
	"strings"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/persist"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/tooltable"
)

// importTbl parses a legacy LinuxCNC .tbl file and stores tools via persist API.
func (m *module) importTbl(path string) error {
	f, err := os.Open(path)
	if err != nil {
		return err
	}
	defer f.Close()

	var entries []persist.Entry
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
		tool := tooltable.ToolEntry{
			Toolno:      t.toolno,
			Pocketno:    t.pocketno,
			XOffset:     t.x,
			YOffset:     t.y,
			ZOffset:     t.z,
			AOffset:     t.a,
			BOffset:     t.b,
			COffset:     t.c,
			UOffset:     t.u,
			VOffset:     t.v,
			WOffset:     t.w,
			Diameter:    t.diameter,
			Frontangle:  t.frontangle,
			Backangle:   t.backangle,
			Orientation: t.orientation,
			Comment:     t.comment,
		}
		data, err := json.Marshal(tool)
		if err != nil {
			return fmt.Errorf("marshal tool %d: %w", t.toolno, err)
		}
		entries = append(entries, persist.Entry{
			Key:   strconv.FormatInt(int64(t.toolno), 10),
			Value: string(data),
		})
	}
	if err := scanner.Err(); err != nil {
		return err
	}
	if len(entries) > 0 {
		if _, err := m.db.SetEntries(m.dbHandle, entries); err != nil {
			return err
		}
	}
	return nil
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
