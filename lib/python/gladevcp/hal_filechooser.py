#!/usr/bin/env python
# vim: sts=4 sw=4 et
# GladeVcp FileChooser related widgets
#
# Copyright (c) 2010  Pavel Shramov <shramov@mexmat.net>
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

import os, sys, time, select, re
import tempfile, atexit, shutil

import gtk, gobject

from hal_widgets import _HalWidgetBase
import linuxcnc
from hal_glib import GStat

_ = lambda x: x

from hal_actions import _EMC_ActionBase, _EMC_Action

progress_re = re.compile("^FILTER_PROGRESS=(\\d*)$")
class FilterProgram:
    def __init__(self, program_filter, infilename, outfilename, callback=None):
        import subprocess
        outfile = open(outfilename, "w")
        infilename_q = infilename.replace("'", "'\\''")
        env = dict(os.environ)
        env['AXIS_PROGRESS_BAR'] = '1'
        p = subprocess.Popen(["sh", "-c", "%s '%s'" % (program_filter, infilename_q)],
                              stdin=subprocess.PIPE,
                              stdout=outfile,
                              stderr=subprocess.PIPE,
                              env=env)
        p.stdin.close()  # No input for you
        self.p = p
        self.stderr_text = []
        self.program_filter = program_filter
        self.callback = callback
        gobject.timeout_add(100, self.update)
        #progress = Progress(1, 100)
        #progress.set_text(_("Filtering..."))

    def update(self):
        if self.p.poll() is not None:
            self.finish()
            return False

        r,w,x = select.select([self.p.stderr], [], [], 0)
        if not r:
            return True
        stderr_line = self.p.stderr.readline()
        m = progress_re.match(stderr_line)
        if m:
            pass #progress.update(int(m.group(1)), 1)
        else:
            self.stderr_text.append(stderr_line)
            sys.stderr.write(stderr_line)
        return True

    def finish(self):
        # .. might be something left on stderr
        for line in self.p.stderr:
            m = progress_re.match(line)
            if not m:
                self.stderr_text.append(line)
                sys.stderr.write(line)
        r = self.p.returncode
        if r:
            self.error(r, "".join(self.stderr_text))
        if self.callback:
            self.callback(r)

    def error(self, exitcode, stderr):
        dialog = gtk.MessageDialog(None, 0, gtk.MESSAGE_ERROR, gtk.BUTTONS_CLOSE,
                _("The program %(program)r exited with code %(code)d.  "
                "Any error messages it produced are shown below:")
                    % {'program': self.program_filter, 'code': exitcode})
        dialog.format_secondary_text(stderr)
        dialog.run()
        dialog.destroy()

class _EMC_FileChooser(_EMC_ActionBase):
    def _hal_init(self):
        _EMC_ActionBase._hal_init(self)
        self.ini = None
        self.tmp = None
        self.load_filters()

    def mktemp(self):
        if self.tmp:
            return
        self.tmp = tempfile.mkdtemp(prefix='emcflt-', suffix='.d')
        atexit.register(lambda: shutil.rmtree(self.tmp))

    def load_file(self, filename):
        flt = self.get_filter_program(filename)
        if not flt:
            return self._load_file(filename)

        if not self.tmp:
            self.mktemp()

        tmp = os.path.join(self.tmp, os.path.basename(filename))
        flt = FilterProgram(flt, filename, tmp, lambda r: r or self._load_file(tmp))

    def _load_file(self, filename):
        if filename:
            self.linuxcnc.mode(linuxcnc.MODE_AUTO)
            old = self.gstat.stat.file
            self.linuxcnc.program_open(filename)
            if old == filename:
                self.gstat.emit('file-loaded', filename)

    def load_filters(self, inifile=None):
        inifile = inifile or os.environ.get('INI_FILE_NAME', '/dev/null')
        self.ini = linuxcnc.ini(inifile)

        self._load_filters(self.ini)

    def get_filter_program(self, filename):
        ext = os.path.splitext(filename)[1]
        if ext:
            return self.ini.find("FILTER", ext[1:])
        else:
            return None

    def _load_filters(self, inifile):
        def _e2p(n, el):
            #print "New filter %s: %s" % (n, el)
            p = gtk.FileFilter()
            p.set_name(n)
            map(lambda s: p.add_pattern('*' + s), el)
            #print p
            return p
        all_extensions = [".ngc"]
        extensions = inifile.findall("FILTER", "PROGRAM_EXTENSION")
        extensions = [e.split(None, 1) for e in extensions]
        extensions = tuple([(v, tuple(k.split(","))) for k, v in extensions])
        map(lambda t: all_extensions.extend(t[1]), extensions)
        self.add_filter(_e2p("All machinable files", all_extensions))
        self.add_filter(_e2p("rs274ngc files", ['.ngc']))
        for n,e in extensions:
            self.add_filter(_e2p(n, e))
        self.add_filter(_e2p("All files", ['']))

class EMC_FileChooserDialog(gtk.FileChooserDialog, _EMC_FileChooser):
    __gtype_name__ = 'EMC_FileChooserDialog'
    def __init__(self, *a, **kw):
        gtk.FileChooserDialog.__init__(self, *a, **kw)
        _EMC_FileChooser._hal_init(self)
        self.connect('response', self.on_response)

    def on_response(self, w, response):
        pass
        #print ">>>", w, response

class EMC_FileChooserButton(gtk.FileChooserButton, _EMC_FileChooser):
    __gtype_name__ = 'EMC_FileChooserButton'
    def __init__(self, *a, **kw):
        gtk.FileChooserButton.__init__(self, gtk.FileChooserDialog())

        self.connect('file-set', self.on_file_set)

    def on_file_set(self, w):
        self.load_file(w.get_filename())

class EMC_Action_Open(_EMC_Action, _EMC_FileChooser):
    __gtype_name__ = 'EMC_Action_Open'
    fixed_file = gobject.property(type=str, default='', nick='Fixed file name')

    def _hal_init(self):
        _EMC_FileChooser._hal_init(self)
        _EMC_Action._hal_init(self)
        self.currentfolder = os.path.expanduser("~/linuxcnc/nc_files")
        self.gstat.connect('interp-run', lambda w: self.set_sensitive(False))
        self.gstat.connect('interp-idle', lambda w: self.set_sensitive(True))

    def _load_filters(self, ini): pass

    def on_activate(self, w):
        if self.fixed_file:
            self.load_file(self.fixed_file)
            return
        dialog = EMC_FileChooserDialog(title="Open File",action=gtk.FILE_CHOOSER_ACTION_OPEN, 
                buttons=(gtk.STOCK_CANCEL,gtk.RESPONSE_CANCEL,gtk.STOCK_OPEN,gtk.RESPONSE_OK))
        dialog.set_current_folder(self.currentfolder)
        dialog.show()
        r = dialog.run()
        fn = dialog.get_filename()
        dialog.hide()
        if r == gtk.RESPONSE_OK:
            dialog.load_file(fn)
            self.currentfolder = os.path.dirname(fn)
        dialog.destroy()

class EMC_Action_Reload(_EMC_Action, _EMC_FileChooser):
    __gtype_name__ = 'EMC_Action_Reload'
    def _hal_init(self):
        _EMC_Action._hal_init(self)

    def on_activate(self, w):
        self._load_file(self.gstat.stat.file)
