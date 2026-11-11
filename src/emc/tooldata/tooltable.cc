/*
** The tool table, as task keeps it. See tooltable.hh.
**
** Reading and writing the tool table file incorporates work from
** tooldata_common.cc, Copyright 2021 Dewey Garrett <dgarrett@panix.com>.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <algorithm>
#include "tooltable.hh"

static bool same_entry(const CANON_TOOL_TABLE &a, const CANON_TOOL_TABLE &b)
{
    return a.toolno == b.toolno
        && a.pocketno == b.pocketno
        && a.offset.tran.x == b.offset.tran.x
        && a.offset.tran.y == b.offset.tran.y
        && a.offset.tran.z == b.offset.tran.z
        && a.offset.a == b.offset.a
        && a.offset.b == b.offset.b
        && a.offset.c == b.offset.c
        && a.offset.u == b.offset.u
        && a.offset.v == b.offset.v
        && a.offset.w == b.offset.w
        && a.diameter == b.diameter
        && a.frontangle == b.frontangle
        && a.backangle == b.backangle
        && a.orientation == b.orientation
        && strncmp(a.comment, b.comment, CANON_TOOL_COMMENT_SIZE) == 0;
}

ToolTable::ToolTable(bool random_toolchanger)
    : random(random_toolchanger)
{
    spindle_entry = tooldata_entry_init();
    if (!random) {
        spindle_entry.pocketno = 0;
    }
    // the storage starts out empty, and so does the record of what is in it
    tooldata_reset();
    tooldata_last_index_set(0);
    publish();
}

// A random changer's tool in pocket `pocketno`, tools.end() if it is empty.
std::map<int, CANON_TOOL_TABLE>::iterator ToolTable::find_pocket(int pocketno)
{
    for (auto it = tools.begin(); it != tools.end(); ++it) {
        if (it->second.pocketno == pocketno) return it;
    }
    return tools.end();
}

ToolTable::Where ToolTable::locate(int toolno, CANON_TOOL_TABLE *entry) const
{
    if (toolno < 0) return NOT_FOUND;
    if (!random && toolno == 0) {
        // no tool
        if (entry) *entry = spindle_entry;
        return SPINDLE;
    }
    auto it = tools.find(toolno);
    if (it == tools.end()) return NOT_FOUND;
    if (entry) *entry = it->second;
    return (random && it->second.pocketno == 0) ? SPINDLE : POCKET;
}

CANON_TOOL_TABLE ToolTable::get_tool(int toolno) const
{
    CANON_TOOL_TABLE tdata = tooldata_entry_init();
    if (locate(toolno, &tdata) == NOT_FOUND) {
        return tooldata_entry_init();
    }
    // a non-random changer finds tool 0 in the spindle, whose entry may be
    // the loaded tool's copy
    if (tdata.toolno != toolno) {
        tdata = tooldata_entry_init();
        tdata.toolno = 0;
        tdata.pocketno = 0;
    }
    return tdata;
}

CANON_TOOL_TABLE ToolTable::spindle() const
{
    if (!random) return spindle_entry;
    for (const auto &tool : tools) {
        if (tool.second.pocketno == 0) return tool.second;
    }
    return tooldata_entry_init();
}

bool ToolTable::set_offset(int toolno, int loaded_toolno, const EmcPose &offset,
                           double diameter, double frontangle,
                           double backangle, int orientation,
                           CANON_TOOL_TABLE *entry)
{
    // Tool 0 on a non-random changer is no tool. It is found in the spindle,
    // whose entry is the loaded tool's copy and is not set on its own.
    if (!random && toolno == 0) return false;

    auto it = tools.find(toolno);
    if (it == tools.end()) return false;

    CANON_TOOL_TABLE &tdata = it->second;
    tdata.offset = offset;
    tdata.diameter = diameter;
    tdata.frontangle = frontangle;
    tdata.backangle = backangle;
    tdata.orientation = orientation;
    if (entry) *entry = tdata;

    // the copy in the spindle is what is in effect
    if (!random && loaded_toolno == toolno) {
        spindle_entry.toolno = toolno;
        spindle_entry.offset = offset;
        spindle_entry.diameter = diameter;
        spindle_entry.frontangle = frontangle;
        spindle_entry.backangle = backangle;
        spindle_entry.orientation = orientation;
    }
    publish();
    return true;
}

bool ToolTable::load(int toolno)
{
    CANON_TOOL_TABLE tdata;
    Where where = locate(toolno, &tdata);
    if (where == NOT_FOUND) return false;

    if (!random) {
        if (where == SPINDLE) {
            // tool 0: the spindle is empty
            spindle_entry = tooldata_entry_init();
            spindle_entry.toolno = 0;
            spindle_entry.pocketno = 0;
        } else {
            spindle_entry = tdata;
        }
    } else if (where == POCKET) {
        // the tool in the spindle goes where the new tool comes from; if
        // the spindle was empty, the pocket is empty now
        auto in_spindle = find_pocket(0);
        if (in_spindle != tools.end()) {
            in_spindle->second.pocketno = tdata.pocketno;
        }
        tools[toolno].pocketno = 0;
    }
    publish();
    return true;
}

bool ToolTable::reload(int toolno)
{
    if (random) return false;
    auto it = tools.find(toolno);
    if (it == tools.end()) return false;
    spindle_entry = it->second;
    publish();
    return true;
}

void ToolTable::clear()
{
    tools.clear();
    spindle_entry = tooldata_entry_init();
}

// Take one line of a tool table; false if it is not understood. `source` is
// the tool table file, NULL for a line of the tool database program.
bool ToolTable::add_line(const char *line, const char *source)
{
    CANON_TOOL_TABLE tdata;
    bool has_pocket;
    int parsed = tooldata_parse_line(line, &tdata, &has_pocket);
    if (parsed == 0) return true;
    if (parsed < 0) {
        if (source) {
            printf("File: %s Unrecognized line skipped:\n    %s", source, line);
        }
        return false;
    }

    const char *from = source ? source : "tool database";
    if (tdata.toolno < 0) {
        fprintf(stderr, "%s: line without a tool number skipped:\n    %s",
                from, line);
        return true;
    }
    if (tools.count(tdata.toolno)) {
        fprintf(stderr, "WARNING: %s: tool %d is in the tool table more than "
                "once, the first entry is used. Skipped:\n    %s",
                from, tdata.toolno, line);
        return true;
    }

    if (!random) {
        // The export has room for CANON_POCKETS_MAX entries, the spindle's
        // included.
        if (tools.size() + 1 >= CANON_POCKETS_MAX) {
            printf("%s: more than %d tools. Skipping tool=%d\n",
                   from, CANON_POCKETS_MAX - 1, tdata.toolno);
            return true;
        }
    } else {
        if (tdata.pocketno < 0 || tdata.pocketno >= CANON_POCKETS_MAX) {
            printf("max pocket number is %d. skipping tool %d\n",
                   CANON_POCKETS_MAX - 1, tdata.toolno);
            return true;
        }
        // one tool to a pocket: the export has one entry for each pocket
        auto prior = find_pocket(tdata.pocketno);
        if (prior != tools.end()) {
            fprintf(stderr,"WARNING: Attempt to assign multiple toolno.s to pocket %d\n",tdata.pocketno);
            fprintf(stderr,"    WAS: pocket=%3d toolno=%3d\n",prior->second.pocketno,prior->second.toolno);
            fprintf(stderr,"     IS: pocket=%3d toolno=%3d\n",tdata.pocketno,tdata.toolno);
            tools.erase(prior);
        }
    }
    tools[tdata.toolno] = tdata;
    return true;
}

void ToolTable::db_reset(void *table)
{
    static_cast<ToolTable *>(table)->clear();
}

int ToolTable::db_line(const char *line, void *table)
{
    return static_cast<ToolTable *>(table)->add_line(line, NULL) ? 0 : -1;
}

int ToolTable::read(const char *filename)
{
    if (db_active) {
        if (tooldata_db_getall(db_reset, db_line, this)) {
            fprintf(stderr,
              "\ntooldata_load: Failed to load tooldata from database\n\n");
            db_active = false;
            publish();
            return -1;
        }
        // update the single-entry tbl file (typ: db_spindle.tbl)
        write(NULL);
        publish();
        return 0;
    }

    if (!filename) return -1;

    clear();

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "%s, %d: Failed to open tool table file '%s': %s\n",
                __FILE__, __LINE__, filename, strerror(errno));
        publish();
        return -1;
    }

    char line[CANON_TOOL_ENTRY_LEN];
    while (fgets(line, sizeof(line), fp)) {
        add_line(line, filename);
    }
    fclose(fp);
    publish();
    return 0;
}

int ToolTable::write(const char *filename) const
{
    if (db_active) {
        if (!random) return 0;
        filename = DB_SPINDLE_SAVE; // one entry tbl
    } else if (!filename) {
        // happens without ini entry [EMCIO]TOOL_TABLE
        fprintf(stderr, "%s: No filename. Are you missing INI-entry [EMCIO]TOOL_TABLE?\n",
                __PRETTY_FUNCTION__);
        return -1;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "%s, %d: Failed to open tool table file '%s': %s\n",
                __FILE__, __LINE__, filename, strerror(errno));
        return -1;
    }

    // by tool number; a random changer's tools by pocket
    std::vector<CANON_TOOL_TABLE> lines;
    if (db_active) {
        lines.push_back(spindle());
    } else {
        for (const auto &tool : tools) lines.push_back(tool.second);
        if (random) {
            std::stable_sort(lines.begin(), lines.end(),
                [](const CANON_TOOL_TABLE &a, const CANON_TOOL_TABLE &b) {
                    return a.pocketno < b.pocketno;
                });
        }
    }
    for (const CANON_TOOL_TABLE &tdata : lines) {
        if (tdata.toolno == -1) continue;
        char theline[CANON_TOOL_ENTRY_LEN] = {0};
        tooldata_format_toolline(tdata.pocketno,
                                 1, // ignore_zero_values
                                 tdata, theline);
        fprintf(fp, "%s", theline);
    }
    fclose(fp);
    return 0;
}

// Export the table to the tooldata storage. Only the entries that differ
// from what is there are stored, so a reader sees a single change as one.
void ToolTable::publish()
{
    const CANON_TOOL_TABLE empty = tooldata_entry_init();
    std::vector<CANON_TOOL_TABLE> want;
    if (!random) {
        want.push_back(spindle_entry);
        for (const auto &tool : tools) {
            if (want.size() >= CANON_POCKETS_MAX) break;
            want.push_back(tool.second);
        }
    } else {
        want.push_back(empty);
        for (const auto &tool : tools) {
            const CANON_TOOL_TABLE &tdata = tool.second;
            if (tdata.pocketno < 0 || tdata.pocketno >= CANON_POCKETS_MAX) continue;
            if ((size_t)tdata.pocketno >= want.size()) {
                want.resize(tdata.pocketno + 1, empty);
            }
            want[tdata.pocketno] = tdata;
        }
    }

    bool changed = want.size() != exported.size();
    size_t count = std::max(want.size(), exported.size());
    for (size_t idx = 0; idx < count; idx++) {
        const CANON_TOOL_TABLE &now = idx < want.size() ? want[idx] : empty;
        const CANON_TOOL_TABLE &was = idx < exported.size() ? exported[idx] : empty;
        if (same_entry(now, was)) continue;
        if (tooldata_put(now, idx) == IDX_FAIL) {
            fprintf(stderr,"UNEXPECTED %s %d\n",__FILE__,__LINE__);
        }
        changed = true;
    }
    if (want.size() != exported.size()) {
        tooldata_last_index_set(want.size() - 1);
    }
    exported = want;
    if (changed) table_serial++;
}
