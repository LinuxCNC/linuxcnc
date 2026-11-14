/*
** The tool table, as task keeps it.
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
#ifndef TOOL_TABLE_HH
#define TOOL_TABLE_HH

#include <map>
#include <vector>
#include "tooldata.hh"

/*
** The tools of a machine, looked up by tool number. Task owns the one
** instance; nothing else writes it.
**
** A tool number is in the table once: of several tool table lines with the
** same tool number the first is used.
**
** A non-random tool changer puts a tool back where it came from. The
** spindle has an entry of its own, a copy of the loaded tool's.
**
** A random tool changer puts the tool it unloads into the pocket of the
** tool it loads. A tool's pocket number says where it is, and pocket 0 is
** the spindle. A pocket holds one tool.
**
** Every change is exported to the tooldata storage (the tool mmap), which
** is what other processes read. The export is laid out by index, as that
** storage always was: the spindle's entry at 0, then a non-random changer's
** tools in the order of their tool numbers, or a random changer's tools at
** their pocket number. Nothing is read back from it.
*/
class ToolTable {
public:
    explicit ToolTable(bool random_toolchanger);

    enum Where {
        NOT_FOUND,
        SPINDLE,    // in the spindle; on a non-random changer also tool 0
        POCKET,
    };
    // Where tool `toolno` is; with `entry`, also its entry.
    Where locate(int toolno, CANON_TOOL_TABLE *entry = nullptr) const;

    // The entry of tool `toolno`, or one with toolno -1 if it is not in the
    // table. Tool 0 on a non-random changer is no tool: toolno 0 in
    // pocket 0, with no offsets.
    CANON_TOOL_TABLE get_tool(int toolno) const;

    // The spindle's entry: the loaded tool's data, toolno 0 or -1 for none.
    CANON_TOOL_TABLE spindle() const;

    // Set the data of tool `toolno`, also of its copy in the spindle if
    // `loaded_toolno` says it is the loaded tool. False if it is not in
    // the table. `entry` gets the changed entry.
    bool set_offset(int toolno, int loaded_toolno, const EmcPose &offset,
                    double diameter, double frontangle, double backangle,
                    int orientation, CANON_TOOL_TABLE *entry = nullptr);

    // Put tool `toolno` into the spindle. On a non-random changer tool 0
    // empties it. False if the tool is not in the table.
    bool load(int toolno);

    // Non-random changer: make the spindle's entry a copy of tool
    // `toolno`'s again, after the table was read. False if the tool is not
    // in the table; the spindle's entry is left as reading left it.
    bool reload(int toolno);

    // Read the table from the tool table file, or from the tool database
    // program if one is in use. Returns 0 if it was read.
    int read(const char *filename);
    // Write the tool table file, the tools in the order of their tool
    // numbers, a random changer's in the order of their pockets.
    // With a tool database program only a random
    // changer writes one, DB_SPINDLE_SAVE with the spindle's entry.
    int write(const char *filename) const;

    void use_db(bool active) { db_active = active; }

    // Counts the changes to the table.
    int serial() const { return table_serial; }

private:
    bool random;
    bool db_active {false};
    std::map<int, CANON_TOOL_TABLE> tools; // by tool number
    // a non-random changer's spindle entry; a random changer's is the tool
    // in pocket 0
    CANON_TOOL_TABLE spindle_entry;
    std::vector<CANON_TOOL_TABLE> exported;
    int table_serial {0};

    void clear();
    bool add_line(const char *line, const char *source);
    static void db_reset(void *table);
    static int db_line(const char *line, void *table);
    std::map<int, CANON_TOOL_TABLE>::iterator find_pocket(int pocketno);
    void publish();
};

#endif
