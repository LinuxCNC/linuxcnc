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
import emc
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
        self.buf = gtksourceview.Buffer()
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
        self.set_mark_category_background('motion', gtk.gdk.Color('#f44'))

    def _hal_init(self):
        _EMC_ActionBase._hal_init(self)
        self.gstat.connect('file-loaded', lambda w, f: gobject.timeout_add(1, self.load_file, f))
        self.gstat.connect('line-changed', self.set_line)

    def load_file(self, fn):
        self.filename = fn
        if not fn:
            self.buf.set_text('')
            return
        self.buf.set_text(open(fn).read())
        self.set_line(self.gstat, self.gstat.stat.motion_line)

    def set_line(self, w, l):
        if not l:
            if self.mark:
                self.buf.delete_mark(self.mark)
                self.mark = None
            return
        line = self.buf.get_iter_at_line(l)
        if not self.mark:
            self.mark = self.buf.create_source_mark('motion', 'motion', line)
            self.mark.set_visible(True)
        else:
            self.buf.move_mark(self.mark, line)
        self.scroll_to_mark(self.mark, 0, True, 0, 0.5)

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

    def on_activate(self, w):
        if not self.textview:
            return
        dialog = gtk.FileChooserDialog(buttons=(gtk.STOCK_CANCEL,gtk.RESPONSE_CANCEL,gtk.STOCK_SAVE,gtk.RESPONSE_OK))
        dialog.set_do_overwrite_confirmation(True)
        if self.textview.filename:
            dialog.set_filename(self.textview.filename)
        dialog.show()
        r = dialog.run()
        fn = dialog.get_filename()
        dialog.destroy()
        if r == gtk.RESPONSE_OK:
            self.save(fn)
