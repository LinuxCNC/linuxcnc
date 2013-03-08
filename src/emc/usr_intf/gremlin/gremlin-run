#!/usr/bin/python

import sys
import os
import gtk

import linuxcnc
from gremlin import Gremlin

def W(p, k, *args, **kw):
    w = k(*args, **kw)
    w.show()
    p.add(w)
    return w

class GremlinApp(gtk.Window):
    def __init__(self, inifile):
        if not inifile:
            inifile = os.environ.get('INI_FILE_NAME', None)
            if not inifile:
                usage()

        try:
            inifile = linuxcnc.ini(inifile)
        except linuxcnc.error,detail:
            usage('Using filename = %s\n %s' % (inifile,detail))

        gtk.Window.__init__(self)

        self.vbox = W(self, gtk.VBox)
        self.gremlin = W(self.vbox, Gremlin, inifile)
        self.gremlin.set_size_request(400, 400)

        self.connect("destroy", self.quit)

        self.show()
    def quit(self, event):
        gtk.main_quit()


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
    gtk.main()

if __name__ == '__main__': raise SystemExit, main()
