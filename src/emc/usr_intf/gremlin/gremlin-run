#!/usr/bin/env python3
import gi
gi.require_version("Gtk","3.0")
from gi.repository import Gtk

import sys
import os

import linuxcnc
from gremlin import Gremlin

def W(p, k, *args, **kw):
    w = k(*args, **kw)
    w.show()
    p.add(w)
    return w

class GremlinApp(Gtk.Window):
    def __init__(self, inifile):
        if not inifile:
            inifile = os.environ.get('INI_FILE_NAME', None)
            if not inifile:
                usage()

        try:
            inifile = linuxcnc.ini(inifile)
        except linuxcnc.error as detail:
            usage('Using filename = %s\n %s' % (inifile,detail))

        Gtk.Window.__init__(self)

        self.vbox = W(self, Gtk.VBox)
        self.gremlin = W(self.vbox, Gremlin, inifile)
        self.gremlin.set_size_request(400, 400)

        self.connect("destroy", self.quit)

        self.show()
    def quit(self, event):
        Gtk.main_quit()


def usage(msg=None):
    print('Usage: %s inifilename' % sys.argv[0])
    print('   or: %s              (requires environmental variable INI_FILE_NAME)'
         % sys.argv[0])
    if msg:
        print('\n%s' % msg)


    sys.exit(1)

def main():
    if len(sys.argv) == 1:
        inifilename = None
    elif len(sys.argv) == 2:
        inifilename = sys.argv[1]
    else:
        usage()

    g = GremlinApp(inifilename)
    Gtk.main()

if __name__ == '__main__': raise SystemExit(main())
