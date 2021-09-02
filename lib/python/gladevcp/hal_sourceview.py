#!/usr/bin/env python3
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


# Norberts comments
# self.gstat ist nirgends festgelegt, auch wenn GStat importiert wurde



import os, time

import gi
gi.require_version("Gtk","3.0")
gi.require_version("Gdk","3.0")
gi.require_version("GtkSource","3.0")
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import GObject
from gi.repository import GtkSource
from gi.repository import GLib

from .hal_widgets import _HalWidgetBase
import linuxcnc
from hal_glib import GStat
from .hal_actions import _EMC_ActionBase, _EMC_Action
from .hal_filechooser import _EMC_FileChooser

class EMC_SourceView(GtkSource.View, _EMC_ActionBase):
    __gtype_name__ = 'EMC_SourceView'
    __gsignals__ = {
        'changed': (GObject.SIGNAL_RUN_FIRST, GObject.TYPE_NONE, ()),
    }

    __gproperties__ = {
        'idle_line_reset' : ( GObject.TYPE_BOOLEAN, 'Reset Line Number when idle', 'Sets line number back to 0 when code is not running or paused',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT)
    }
    def __init__(self, *a, **kw):
        GtkSource.View.__init__(self, *a, **kw)
        self.filename = None
        self.mark = None
        self.offset = 0
        self.program_length = 0
        self.idle_line_reset = True
        self.buf = self.get_buffer()
        self.buf.set_max_undo_levels(20)
        self.buf.connect('changed', self.update_iter)
        self.lm = GtkSource.LanguageManager()
        self.sm = GtkSource.StyleSchemeManager()
        if 'EMC2_HOME' in os.environ:
            path = os.path.join(os.environ['EMC2_HOME'], 'share/gtksourceview-2.0/language-specs/')
            self.lm.set_search_path(self.lm.get_search_path() + [path])

        self.buf.set_language(self.lm.get_language('.ngc'))
        self.set_show_line_numbers(True)
        self.set_show_line_marks(True)
        self.set_highlight_current_line(True)

        self.add_mark_category('error', '#ff7373')
        self.add_mark_category('motion', '#c5c5c5')
        self.add_mark_category('selected', '#96fef6')

        #TODO: how to set property background? but seems to work ok regardless.
        self.found_text_tag = Gtk.TextTag()

        self.connect('button-release-event', self.button_pressed)

    def add_mark_category(self, category, bg_color):
        att = GtkSource.MarkAttributes()
        color = Gdk.RGBA()
        color.parse(bg_color)
        att.set_background(color)
        self.set_mark_attributes(category, att, 1)

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in ['idle_line_reset']:
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name in ['idle_line_reset']:
            return setattr(self, name, value)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def _hal_init(self):
        _EMC_ActionBase._hal_init(self)
        self.gstat.connect('file-loaded', lambda w, f: GLib.timeout_add(1, self.load_file, f))
        self.gstat.connect('line-changed', self.highlight_line)
        if self.idle_line_reset:
            self.gstat.connect('interp_idle', lambda w: self.set_line_number(0))

    def set_language(self, lang, path = None):
        # path = the search path for the language file
        # if none, set to default
        # lang = the lang file to set
        if path == None:
            if 'EMC2_HOME' in os.environ:
                path = os.path.join(os.environ['EMC2_HOME'], 'share/gtksourceview-2.0/language-specs/')
        if path:
            self.lm.set_search_path(path)
        self.buf.set_language(self.lm.get_language(lang))
        
    def set_style_scheme(self, style, path = None):
        if path:
            self.sm.set_search_path(path)
        self.buf.set_style_scheme(self.sm.get_scheme(style))

    def get_filename(self):
        return self.filename

    # This load the file while not allowing undo buttons to unload the program.
    # It updates the iter because iters become invalid when anything changes.
    # We set the buffer-unmodified flag false after loading the file.
    # Set the highlight line to the line linuxcnc is looking at.
    # if one calls load_file without a filename, We reload the existing file.
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
        f = open(fn, 'r')
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

    def button_pressed(self,widget,event):
        self.update_iter()

    # iters are invalid (and will cause a complete crash) after any changes.
    # so we have to update them after a change or the user clicks on view with mouse
    # re-establish start and end of text
    # current_iter is the cursor position
    # cancel the last search match
    def update_iter(self,widget=None):
        self.start_iter =  self.buf.get_start_iter()
        self.end_iter = self.buf.get_end_iter()
        self.current_iter = self.buf.get_iter_at_mark(self.buf.get_insert())
        self.match_start = self.match_end = None
        start, end = self.buf.get_bounds()
        self.buf.remove_tag(self.found_text_tag, start, end)
        self.emit("changed")

    # This will search the buffer for a specified text string.
    # You can search forward or back, with mixed case or exact text.
    # if it searches to either end, if search is pressed again, it will start at the other end.
    # This will grab focus and set the cursor active, while highlighting the line.
    # It automatically scrolls if it must.
    # it primes self.match_start for replacing text 
    def text_search(self,direction=True,mixed_case=True,text="t"):
        CASEFLAG = 0
        if mixed_case:
            CASEFLAG = Gtk.TextSearchFlags.CASE_INSENSITIVE
        if direction:
            if self.current_iter.is_end():
                self.current_iter = self.start_iter.copy()
            found = Gtk.TextIter.forward_search(self.current_iter,text,CASEFLAG, None)
        else:
            if self.current_iter.is_start():
                self.current_iter = self.end_iter.copy()
            found = Gtk.TextIter.backward_search(self.current_iter,text,CASEFLAG, None)
        if found:
            # erase any existing highlighting tags
            try:
                self.buf.remove_tag(self.found_text_tag, self.match_start, self.match_end)
            except:
                pass
            self.match_start,self.match_end = found
            self.buf.apply_tag(self.found_text_tag, self.match_start, self.match_end)
            self.buf.select_range(self.match_start,self.match_end)
            self.grab_focus()
            if direction:
                self.current_iter = self.match_end.copy()
            else:
                self.current_iter = self.match_start.copy()
            self.scroll_to_iter(self.match_start, 0, True, 0, 0.5)

        else:
            self.current_iter = self.start_iter.copy()

            self.match_start = self.match_end = None

    # check if we already have a match
    # if so and we are replacing-all, delete and insert without individular undo moves
    # if so but not replace-all, delete and insert with individulat undo moves
    # do a search to prime self.match_start
    # if we have gone to the end, stop searching
    # if not replace-all stop searching, otherwise start again
    def replace_text_search(self,direction=True,mixed_case=True,text="t",re_text="T",replace_all=False):
        while True:
            if self.match_start:
                if replace_all:
                    self.buf.delete(self.match_start, self.match_end)
                    self.buf.insert_at_cursor(re_text)
                else:
                    self.buf.delete_interactive(self.match_start, self.match_end,True)
                    self.buf.insert_interactive_at_cursor(re_text,-1,True)
            self.text_search(direction,mixed_case,text)
            if self.current_iter.is_start(): break
            if not replace_all: break

    # undo one level of changes
    def undo(self):
        if self.buf.can_undo():
            self.buf.undo()

    # redo one level of changes
    def redo(self):
        if self.buf.can_redo():
            self.buf.redo()

def safe_write(filename, data, mode=0o644):
    import os, tempfile
    fd, fn = tempfile.mkstemp(dir=os.path.dirname(filename), prefix=os.path.basename(filename))
    try:
        os.write(fd, data)
        os.close(fd)
        fd = None
        os.rename(fn, filename)
        os.chmod(filename, mode)
    finally:
        if fd is not None:
            os.close(fd)
        if os.path.isfile(fn):
            os.unlink(fn)

class EMC_Action_Save(_EMC_Action, _EMC_FileChooser):
    __gtype_name__ = 'EMC_Action_Save'
    __gproperties__ = { 'textview' : (EMC_SourceView.__gtype__, 'Textview',
                    "Corresponding textview widget", GObject.ParamFlags.READWRITE),
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
        dialog = Gtk.FileChooserDialog(title="Save As",action=Gtk.FILE_CHOOSER_ACTION_SAVE,
                    buttons=(Gtk.STOCK_CANCEL,Gtk.RESPONSE_CANCEL,Gtk.STOCK_SAVE,Gtk.ResponseType.OK))
        dialog.set_do_overwrite_confirmation(True)
        dialog.set_current_folder(self.currentfolder)
        if self.textview.filename:
            dialog.set_current_name(os.path.basename(self.textview.filename))
        dialog.show()
        r = dialog.run()
        fn = dialog.get_filename()
        dialog.destroy()
        if r == Gtk.ResponseType.OK:
            self.save(fn)
            self.currentfolder = os.path.dirname(fn)
