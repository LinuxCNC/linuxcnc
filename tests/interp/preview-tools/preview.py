#!/usr/bin/env python3
# The preview interpreter asks its canon for tools by number. StatMixin
# answers from a copy of linuxcnc.stat's tool table, follows the program's
# tool changes and takes its G10 L1 edits.
#   preview.py random|nonrandom test.ngc
import collections
import gcode
import sys
import tempfile
from rs274.interpret import StatMixin

Tool = collections.namedtuple("Tool", "id xoffset yoffset zoffset aoffset boffset"
        " coffset uoffset voffset woffset diameter frontangle backangle"
        " orientation pocketno")

def tool(toolno, pocket, z, diameter=0.0):
    return Tool(toolno, 0.0, 0.0, z, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                diameter, 0.0, 0.0, 0, pocket)

random = sys.argv[1] == "random"

class Stat:
    if random:
        # the index is the pocket; tool 1 is in the spindle
        tool_in_spindle = 1
        tool_table = [tool(-1, 0, 0.0)] * 11
        tool_table[0] = tool(1, 0, 1.0, 0.5)
        tool_table[3] = tool(10, 3, 10.0)
        tool_table[10] = tool(2, 10, 2.0)
    else:
        # in file order; entry 0 is the (empty) spindle
        tool_in_spindle = 0
        tool_table = [tool(0, 0, 0.0), tool(1, 5, 1.0, 0.5),
                      tool(2, 10, 2.0), tool(10, 3, 10.0)]
    angular_units = linear_units = 1.0
    axis_mask = 7
    block_delete = False

class Canon(StatMixin):
    def __getattr__(self, attr):
        return lambda *args: None
    def next_line(self, linecode): pass
    def message(self, text): print(text.strip())
    def tool_offset(self, *offset): print("  offset z=%.4f" % offset[2])
    def change_tool(self, toolno):
        print("  change_tool(%d)" % toolno)
        StatMixin.change_tool(self, toolno)

parameter = tempfile.NamedTemporaryFile()
canon = Canon(Stat(), random)
canon.parameter_file = parameter.name
result, seq = gcode.parse(sys.argv[2], canon, '', '', '')
if result > gcode.MIN_ERROR: raise SystemExit(gcode.strerror(result))
