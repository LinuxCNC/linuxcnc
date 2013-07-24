#!/usr/bin/env python
# vim: sts=4 sw=4 et
# GladeVcp actions
#
# Copyright (c) 2011  Pavel Shramov <shramov@mexmat.net>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import os, time

import gobject, gtk

from hal_widgets import _HalWidgetBase
import linuxcnc
from hal_glib import GStat
from hal_actions import _EMC_ActionBase, _EMC_Action
from hal_filechooser import _EMC_FileChooser

import gtksourceview2 as gtksourceview

class EMC_SourceView(gtksourceview.View, _EMC_ActionBase):
    __gtype_name__ = 'EMC_SourceView'
    def __init__(self, *a, **kw):
        gtksourceview.View.__init__(self, *a, **kw)
        self.filename = None
        self.mark = None
        self.offset = 0
        self.program_length = 0
        self.buf = gtksourceview.Buffer()
        self.buf.set_max_undo_levels(10)
        self.update_iter()
        self.buf.connect('changed', self.update_iter)
        self.set_buffer(self.buf)
        lm = gtksourceview.LanguageManager()
        if 'EMC2_HOME' in os.environ:
            path = os.path.join(os.environ['EMC2_HOME'], 'share/gtksourceview-2.0/language-specs/')
            lm.set_search_path(lm.get_search_path() + [path])

        self.buf.set_language(lm.get_language('.ngc'))
        self.set_show_line_numbers(True)
        self.set_show_line_marks(True)
        self.set_highlight_current_line(True)
        self.set_mark_category_icon_from_icon_name('motion', 'gtk-forward')
        self.set_mark_category_background('motion', gtk.gdk.Color('#ff0'))

    def _hal_init(self):
        _EMC_ActionBase._hal_init(self)
        self.gstat.connect('file-loaded', lambda w, f: gobject.timeout_add(1, self.load_file, f))
        self.gstat.connect('line-changed', self.highlight_line)

    def get_filename(self):
        return self.filename

    # This load the file while not allowing undo buttons to unload the program.
    # It updates the iter because iters become invalid when anything changes.
    # We set the buffer-unmodified flag false after loading the file.
    # Set the hilight line to the line linuxcnc is looking at.
    # if one calls load_file without a filenname, We reload the exisiting file.
    def load_file(self, fn=None):
        self.buf.begin_not_undoable_action()
        if fn == None:
            fn = self.filename
        self.filename = fn
        if not fn:
            self.buf.set_text('')
            return 
        self.buf.set_text(open(fn).read())
        self.buf.end_not_undoable_action()
        self.buf.set_modified(False)
        self.update_iter()
        self.highlight_line(self.gstat, self.gstat.stat.motion_line)
        self.offset = self.gstat.stat.motion_line
        f = file(fn, 'r')
        p = f.readlines()
        f.close()
        self.program_length = len(p)

    # This moves the highlight line to a lower numbered line.
    # useful for run-at-line selection
    def line_down(self):
        self.offset +=1
        self.check_offset()
        self.highlight_line(self.gstat, self.offset)

    # This moves the highlight line to a higher numbered line.
    # useful for run-at-line selection
    def line_up(self):
        self.offset -=1
        self.check_offset()
        self.highlight_line(self.gstat, self.offset)

    def get_line_number(self):
        return self.offset

    # sets the highlight line to a specified line.
    def set_line_number(self,linenum):
        self.offset = linenum
        self.check_offset()
        self.highlight_line(self.gstat, self.offset)

    def check_offset(self):
        if self.offset < 0:
            self.offset = 0
        elif  self.offset > self.program_length:
            self.offset = self.program_length

    def highlight_line(self, w, l):
        self.offset = l
        if not l:
            if self.mark:
                self.buf.delete_mark(self.mark)
                self.mark = None
            return
        line = self.buf.get_iter_at_line(l-1)
        if not self.mark:
            self.mark = self.buf.create_source_mark('motion', 'motion', line)
            self.mark.set_visible(True)
        else:
            self.buf.move_mark(self.mark, line)
        self.scroll_to_mark(self.mark, 0, True, 0, 0.5)

    # iters are invalid (and will cause a complete crash) after any changes.
    # so we have to update them after a change
    def update_iter(self,widget=None):
        self.start_iter =  self.buf.get_start_iter()
        self.end_iter = self.buf.get_end_iter()
        self.current_iter = self.start_iter.copy()

    # This will search the buffer for a specified text string.
    # You can search forward or back, with mixed case or exact text.
    # if it searches to either end, if search is pressed again, it will start at the other end.
    # This will grab focus and set the cursor active, while highlighting the line.
    # It automatically scrolls if it must.
    def text_search(self,direction=True,mixed_case=True,text="t"):
        caseflag = 0
        if mixed_case:
            caseflag = gtksourceview.SEARCH_CASE_INSENSITIVE
        if direction:
            if self.current_iter.is_end():
                self.current_iter = self.start_iter.copy()
            found = gtksourceview.iter_forward_search(self.current_iter,text,caseflag, None)
        else:
            if self.current_iter.is_start():
                self.current_iter = self.end_iter.copy()
            found = gtksourceview.iter_backward_search(self.current_iter,text,caseflag, None)
        if found:
            match_start,match_end = found
            self.buf.select_range(match_start,match_end)
            if direction:
                self.buf.place_cursor(match_start)
                self.grab_focus()
                self.current_iter = match_end.copy()
            else:
                self.buf.place_cursor(match_start)
                self.grab_focus()
                self.current_iter = match_start.copy()
            self.scroll_to_iter(match_start, 0, True, 0, 0.5)
            self.set_highlight_current_line(True)
        else:
            self.current_iter = self.start_iter.copy()
            self.set_highlight_current_line(False)

    # unndo one level of changes
    def undo(self):
        self.buf.undo()

    # redo one level of changes
    def redo(self):
        self.buf.redo()

def safe_write(filename, data, mode=0644):
    import os, tempfile
    fd, fn = tempfile.mkstemp(dir=os.path.dirname(filename), prefix=os.path.basename(filename))
    try:
        os.write(fd, data)
        os.close(fd)
        fd = None
        os.rename(fn, filename)
    finally:
        if fd is not None:
            os.close(fd)
        if os.path.isfile(fn):
            os.unlink(fn)

class EMC_Action_Save(_EMC_Action, _EMC_FileChooser):
    __gtype_name__ = 'EMC_Action_Save'
    __gproperties__ = { 'textview' : (EMC_SourceView.__gtype__, 'Textview',
                    "Corresponding textview widget", gobject.PARAM_READWRITE),
    }
    def __init__(self, *a, **kw):
        _EMC_Action.__init__(self, *a, **kw)
        self.textview = None

    def _hal_init(self):
        _EMC_Action._hal_init(self)

    def on_activate(self, w):
        if not self.textview or not self.textview.filename:
            return
        self.save(self.textview.filename)

    def save(self, fn):
        b = self.textview.get_buffer()
        b.set_modified(False)
        safe_write(fn, b.get_text(b.get_start_iter(), b.get_end_iter()))
        self._load_file(fn)

    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name == 'textview':
            self.textview = value
        else:
            return _EMC_Action.do_set_property(self, property, value)

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name == 'textview':
            return self.textview
        else:
            return _EMC_Action.do_get_property(self, property)


class EMC_Action_SaveAs(EMC_Action_Save):
    __gtype_name__ = 'EMC_Action_SaveAs'

    def __init__(self, *a, **kw):
        _EMC_Action.__init__(self, *a, **kw)
        self.textview = None
        self.currentfolder = os.path.expanduser("~/linuxcnc/nc_files")

    def on_activate(self, w):
        if not self.textview:
            return
        dialog = gtk.FileChooserDialog(title="Save As",action=gtk.FILE_CHOOSER_ACTION_SAVE,
                    buttons=(gtk.STOCK_CANCEL,gtk.RESPONSE_CANCEL,gtk.STOCK_SAVE,gtk.RESPONSE_OK))
        dialog.set_do_overwrite_confirmation(True)
        dialog.set_current_folder(self.currentfolder)
        if self.textview.filename:
            dialog.set_current_name(os.path.basename(self.textview.filename))
        dialog.show()
        r = dialog.run()
        fn = dialog.get_filename()
        dialog.destroy()
        if r == gtk.RESPONSE_OK:
            self.save(fn)
            self.currentfolder = os.path.dirname(fn)
