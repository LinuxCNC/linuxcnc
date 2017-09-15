#!/usr/bin/env python
#    This is a component of AXIS, a front-end for emc
#    Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

import nf, os

# lib/tcltk/emc2 for installed emc
# tcl            for run-in-place emc
for candidate in 'lib/tcltk/linuxcnc', 'tcl':
    LINUXCNC_TCL = os.path.join(nf.PREFIX, candidate, 'linuxcnc.tcl')
    if os.path.exists(LINUXCNC_TCL): break

options = '''
. configure -bg #d9d9d9

set BASE_FONT [linuxcnc::standard_font]
set FIXED_FONT [linuxcnc::standard_fixed_font]

option add *highlightBackground #d9d9d9 $OPTIONLEVEL
option add *background #d9d9d9 $OPTIONLEVEL
option add *foreground black $OPTIONLEVEL

option add *Button.borderWidth 2 $OPTIONLEVEL
option add *Button.font $BASE_FONT $OPTIONLEVEL

option add *Checkbutton.borderWidth 1 $OPTIONLEVEL
option add *Checkbutton.font $BASE_FONT $OPTIONLEVEL

option add *Entry.background white $OPTIONLEVEL
option add *Entry.borderWidth 2 $OPTIONLEVEL
option add *Entry.font $BASE_FONT $OPTIONLEVEL
option add *Entry.selectBackground #08246b $OPTIONLEVEL
option add *Entry.selectForeground white $OPTIONLEVEL

option add *Frame.borderWidth 0 $OPTIONLEVEL

option add *Hierbox.borderWidth 2 $OPTIONLEVEL
option add *Hierbox.selectBackground #08246b $OPTIONLEVEL
option add *Hierbox.selectForeground white $OPTIONLEVEL

option add *Label.font $BASE_FONT $OPTIONLEVEL
option add *Label.borderWidth 1 $OPTIONLEVEL

option add *Listbox.background white $OPTIONLEVEL
option add *Listbox.font $BASE_FONT $OPTIONLEVEL
option add *Listbox.borderWidth 2 $OPTIONLEVEL
option add *Listbox.selectBackground #08246b $OPTIONLEVEL
option add *Listbox.selectForeground white $OPTIONLEVEL

option add *Menu.activeBorderWidth 0 $OPTIONLEVEL
option add *Menu.borderWidth 1 $OPTIONLEVEL
option add *Menu.font $BASE_FONT $OPTIONLEVEL
option add *Menu.activeBackground #08246b $OPTIONLEVEL
option add *Menu.activeForeground white $OPTIONLEVEL

option add *Menubutton.borderWidth 1 $OPTIONLEVEL
option add *Menubutton.font $BASE_FONT $OPTIONLEVEL
option add *Menubutton.indicatorOn 1 $OPTIONLEVEL
option add *Menubutton.relief raised $OPTIONLEVEL

option add *Message.borderWidth 1 $OPTIONLEVEL
option add *Message.font $BASE_FONT $OPTIONLEVEL

option add *Radiobutton.borderWidth 1 $OPTIONLEVEL
option add *Radiobutton.font $BASE_FONT $OPTIONLEVEL

option add *Scrollbar.borderWidth 2 $OPTIONLEVEL
option add *Scrollbar.takeFocus 0 $OPTIONLEVEL
option add *Scrollbar.troughColor #d9d9d9 $OPTIONLEVEL
option add *Scrollbar.elementBorderWidth 2 $OPTIONLEVEL

option add *Text.background white $OPTIONLEVEL
option add *Text.borderWidth 2 $OPTIONLEVEL
option add *Text.font $FIXED_FONT $OPTIONLEVEL
option add *Text.selectBackground #08246b $OPTIONLEVEL
option add *Text.selectForeground white $OPTIONLEVEL

option add *Labelframe.borderWidth 2 $OPTIONLEVEL
option add *Labelframe.relief groove $OPTIONLEVEL
option add *Labelframe.font $BASE_FONT $OPTIONLEVEL

option add *work.borderWidth 3 $OPTIONLEVEL

option add *buttons*Button.default normal $OPTIONLEVEL

option add *Vspace.height 6 $OPTIONLEVEL

option add *Hspace.width 20 $OPTIONLEVEL

option add *Vrule.borderWidth 1 $OPTIONLEVEL
option add *Vrule.relief sunken $OPTIONLEVEL
option add *Vrule.width 2 $OPTIONLEVEL

option add *Hrule.borderWidth 1 $OPTIONLEVEL
option add *Hrule.relief sunken $OPTIONLEVEL
option add *Hrule.height 2 $OPTIONLEVEL

option add *Togl.back #000 startupFile

option add *Togl.dwell #ff8080 startupFile
option add *Togl.m1xx #8080ff startupFile

option add *Togl.straight_feed #ffffff startupFile
option add *Togl.straight_feed_xy #40ff40 startupFile
option add *Togl.straight_feed_uv #4040ff startupFile
option add *Togl.arc_feed #ffffff startupFile
option add *Togl.arc_feed_xy #40ff40 startupFile
option add *Togl.arc_feed_uv #4040ff startupFile
option add *Togl.cone #ffffff startupFile
option add *Togl.cone_xy #00ff00 startupFile
option add *Togl.cone_uv #0000ff startupFile
option add *Togl.traverse #4c8080 startupFile
option add *Togl.traverse_xy #4c8080 startupFile
option add *Togl.traverse_uv #4c8080 startupFile
option add *Togl.backplotjog yellow startupFile
option add *Togl.backplotfeed #c04040 startupFile
option add *Togl.backplotarc #c04080 startupFile
option add *Togl.backplottraverse #4c8080 startupFile
option add *Togl.backplottoolchange orange startupFile
option add *Togl.backplotprobing purple startupFile
option add *Togl.backplotjog_alpha .75 startupFile
option add *Togl.backplotfeed_alpha .75 startupFile
option add *Togl.backplotarc_alpha .75 startupFile
option add *Togl.backplottraverse_alpha .25 startupFile
option add *Togl.backplottoolchange_alpha .25 startupFile
option add *Togl.backplotprobing_alpha .75 startupFile
option add *Togl.selected #00ffff startupFile

option add *Togl.overlay_foreground #ffffff startupFile
option add *Togl.overlay_alpha .75 startupFile
option add *Togl.overlay_background #000000 startupFile

option add *Togl.label_limit #ff353a startupFile
option add *Togl.label_ok #ff8287 startupFile

option add *Togl.small_origin #00ffff startupFile

option add *Togl.axis_x #33ff33 startupFile
option add *Togl.axis_y #ff3333 startupFile
option add *Togl.axis_z #3333ff startupFile

option add *Togl.tool_diffuse #999999 startupFile
option add *Togl.tool_ambient #666666 startupFile
option add *Togl.tool_light_x 1 startupFile
option add *Togl.tool_light_y -1 startupFile
option add *Togl.tool_light_z 1 startupFile
option add *Togl.tool_alpha .2 startupFile

option add *Togl.lathetool #cccccc startupFile
option add *Togl.lathetool_alpha .1 startupFile
'''

import commands, sys

def install(root = None):
    if root is None: root = Tkinter._default_root
    o = root.option_get("optionLevel", "Level") or "interactive"
    if hasattr(root, 'tk'): root = root.tk
    root.call('source', LINUXCNC_TCL)
    root.call('set', 'OPTIONLEVEL', o)
    root.call('eval', options)
# vim:sw=4:sts=4:et:ts=8:
