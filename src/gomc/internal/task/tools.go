// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"fmt"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/tools"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/tooltable"
	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
)

func init() {
	apiserver.RegisterMeta(tools.ToolsMeta)
}

// Package-level reference to the tooltable client, set during registerTools().
// Used by getToolByPocket (called from canon getters).
var pkgTTClient *tooltable.TooltableClient

func tooltableToToolEntry(t *tooltable.ToolEntry) tools.ToolEntry {
	return tools.ToolEntry{
		Toolno:      t.Toolno,
		Pocketno:    t.Pocketno,
		XOffset:     t.XOffset,
		YOffset:     t.YOffset,
		ZOffset:     t.ZOffset,
		AOffset:     t.AOffset,
		BOffset:     t.BOffset,
		COffset:     t.COffset,
		UOffset:     t.UOffset,
		VOffset:     t.VOffset,
		WOffset:     t.WOffset,
		Diameter:    t.Diameter,
		Frontangle:  t.Frontangle,
		Backangle:   t.Backangle,
		Orientation: t.Orientation,
		Comment:     t.Comment,
	}
}

func toolEntryToTooltable(e *tools.ToolEntry) tooltable.ToolEntry {
	return tooltable.ToolEntry{
		Toolno:      e.Toolno,
		Pocketno:    e.Pocketno,
		XOffset:     e.XOffset,
		YOffset:     e.YOffset,
		ZOffset:     e.ZOffset,
		AOffset:     e.AOffset,
		BOffset:     e.BOffset,
		COffset:     e.COffset,
		UOffset:     e.UOffset,
		VOffset:     e.VOffset,
		WOffset:     e.WOffset,
		Diameter:    e.Diameter,
		Frontangle:  e.Frontangle,
		Backangle:   e.Backangle,
		Orientation: e.Orientation,
		Comment:     e.Comment,
	}
}

// toolsImpl implements tools.ToolsCallbacks via the tooltable GMI client.
type toolsImpl struct {
	module *milltaskModule
}

func (t *toolsImpl) ListTools() ([]tools.ToolEntry, error) {
	entries, err := t.module.ttClient.ListTools()
	if err != nil {
		return nil, err
	}
	result := make([]tools.ToolEntry, len(entries))
	for i := range entries {
		result[i] = tooltableToToolEntry(&entries[i])
	}
	return result, nil
}

func (t *toolsImpl) GetTool(toolno int32) (*tools.ToolEntry, error) {
	entry, err := t.module.ttClient.GetTool(toolno)
	if err != nil {
		return nil, err
	}
	te := tooltableToToolEntry(&entry)
	return &te, nil
}

func (t *toolsImpl) PutTool(toolno int32, entry tools.ToolEntry) (*tools.PutToolResult, error) {
	if toolno <= 0 {
		return nil, fmt.Errorf("toolno must be > 0")
	}
	entry.Toolno = toolno
	ttEntry := toolEntryToTooltable(&entry)
	res, err := t.module.ttClient.PutTool(toolno, ttEntry)
	if err != nil {
		return nil, err
	}
	return &tools.PutToolResult{Ok: res.Ok, Index: res.Index}, nil
}

func (t *toolsImpl) DeleteTool(toolno int32) (*tools.CmdResult, error) {
	_, err := t.module.ttClient.DeleteTool(toolno)
	if err != nil {
		return nil, err
	}
	return &tools.CmdResult{Ok: "true"}, nil
}

func (t *toolsImpl) ReloadTools() (*tools.CmdResult, error) {
	_, err := t.module.LoadToolTable("")
	if err != nil {
		return nil, fmt.Errorf("failed to reload tool table: %v", err)
	}
	return &tools.CmdResult{Ok: "true"}, nil
}

// getToolByPocket returns tool data for a given pocket index.
// Used by the canon getter GetExternalToolTable.
// pocket=0 returns the spindle tool (stored as toolno=0 in tooltable by iocontrol).
// pocket>0 scans tool table for matching pocketno.
func getToolByPocket(pocket int32) (retval int32, toolno int32, offset [9]float64, diameter, frontangle, backangle float64, orientation int32) {
	if pkgTTClient == nil {
		return -1, 0, [9]float64{}, 0, 0, 0, 0
	}

	if pocket == 0 {
		// Spindle tool is stored as toolno=0 by iocontrol's load_tool.
		entry, err := pkgTTClient.GetTool(0)
		if err != nil {
			return -1, 0, [9]float64{}, 0, 0, 0, 0
		}
		offset = [9]float64{
			entry.XOffset, entry.YOffset, entry.ZOffset,
			entry.AOffset, entry.BOffset, entry.COffset,
			entry.UOffset, entry.VOffset, entry.WOffset,
		}
		return 0, entry.Toolno, offset, entry.Diameter, entry.Frontangle, entry.Backangle, entry.Orientation
	}

	// For pocket>0: scan all tools for matching pocketno.
	entries, err := pkgTTClient.ListTools()
	if err != nil {
		return -1, 0, [9]float64{}, 0, 0, 0, 0
	}
	for i := range entries {
		if entries[i].Pocketno == pocket {
			offset = [9]float64{
				entries[i].XOffset, entries[i].YOffset, entries[i].ZOffset,
				entries[i].AOffset, entries[i].BOffset, entries[i].COffset,
				entries[i].UOffset, entries[i].VOffset, entries[i].WOffset,
			}
			return 0, entries[i].Toolno, offset, entries[i].Diameter, entries[i].Frontangle, entries[i].Backangle, entries[i].Orientation
		}
	}
	return -1, 0, [9]float64{}, 0, 0, 0, 0
}
