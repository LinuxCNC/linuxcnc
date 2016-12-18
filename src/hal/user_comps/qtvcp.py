import sys
import os
import traceback
import hal
from optparse import Option, OptionParser
from PyQt4 import QtGui, uic
from qtvcp import qt_makepins

options = [ Option( '-c', dest='component', metavar='NAME'
                  , help="Set component name to NAME. Default is basename of UI file")
          , Option( '-d', action='store_true', dest='debug'
                  , help="Enable debug output")
          , Option( '-g', dest='geometry', default="", help="""Set geometry WIDTHxHEIGHT+XOFFSET+YOFFSET.
Values are in pixel units, XOFFSET/YOFFSET is referenced from top left of screen
use -g WIDTHxHEIGHT for just setting size or -g +XOFFSET+YOFFSET for just position""")
          , Option( '-H', dest='halfile', metavar='FILE'
                  , help="execute hal statements from FILE with halcmd after the component is set up and ready")
          , Option( '-m', action='store_true', dest='maximum', help="Force panel window to maxumize")
          , Option( '-f', action='store_true', dest='fullscreen', help="Force panel window to fullscreen")
          , Option( '-t', dest='theme', default="", help="Set QT style. Default is system theme")
          , Option( '-x', dest='parent', type=int, metavar='XID'
                  , help="Reparent gladevcp into an existing window XID instead of creating a new top level window")
          , Option( '-u', dest='usermod', default="", help='file path of user defined handler file')
          , Option( '-U', dest='useropts', action='append', metavar='USEROPT', default=[]
                  , help='pass USEROPTs to Python modules')
          ]

class Trampoline(object):
    def __init__(self,methods):
        self.methods = methods

    def __call__(self, *a, **kw):
        for m in self.methods:
            m(*a, **kw)

qtvcp_debug = 1
def dbg(str):
    global qtvcp_debug
    if not qtvcp_debug: return
    print str

def load_handlers(usermod,halcomp,widgets):
    hdl_func = 'get_handlers'

    def add_handler(method, f):
        if method in handlers:
            handlers[method].append(f)
        else:
            handlers[method] = [f]

    handlers = {}
    for u in usermod:
        (directory,filename) = os.path.split(u)
        (basename,extension) = os.path.splitext(filename)
        if directory == '':
            directory = '.'
        if directory not in sys.path:
            sys.path.insert(0,directory)
            dbg('adding import dir %s' % directory)

        try:
            mod = __import__(basename)
        except ImportError,msg:
            print "module '%s' skipped - import error: %s" %(basename,msg)
	    continue
        dbg("module '%s' imported OK" % mod.__name__)
        try:
            # look for 'get_handlers' function
            h = getattr(mod,hdl_func,None)

            if h and callable(h):
                dbg("module '%s' : '%s' function found" % (mod.__name__,hdl_func))
                objlist = h(halcomp,widgets)
            else:
                # the module has no get_handlers() callable.
                # in this case we permit any callable except class Objects in the module to register as handler
                dbg("module '%s': no '%s' function - registering only functions as callbacks" % (mod.__name__,hdl_func))
                objlist =  [mod]
            # extract callback candidates
            for object in objlist:
                dbg("Registering handlers in module %s object %s" % (mod.__name__, object))
                if isinstance(object, dict):
                    methods = dict.items()
                else:
                    methods = map(lambda n: (n, getattr(object, n, None)), dir(object))
                for method,f in methods:
                    if method.startswith('_'):
                        continue
                    if callable(f):
                        dbg("Register callback '%s' in %s" % (method, object))
                        add_handler(method, f)
        except Exception, e:
            print "QTvcp: trouble looking for handlers in '%s': %s" %(basename, e)
            traceback.print_exc()

    # Wrap lists in Trampoline, unwrap single functions
    for n,v in list(handlers.items()):
        if len(v) == 1:
            handlers[n] = v[0]
        else:
            handlers[n] = Trampoline(v)

    return handlers,mod,object

class MyWindow(QtGui.QMainWindow):
    def __init__(self,filename,halcomp):
        super(MyWindow, self).__init__()
        self.filename = filename
        self.halcomp = halcomp

    def instance(self):
        instance = uic.loadUi(self.filename, self)
        #for widget in instance.children():
        #    print widget

    def load_extension(self,filename):
        methods,self.handler_module,self.handler_instance = load_handlers([filename],self.halcomp,self)
        #print methods
        for i in methods:
            #print i, methods[i]
            self[i] = methods[i]

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

def main():
    (progdir, progname) = os.path.split(sys.argv[0])

    usage = "usage: %prog [options] myfile.ui"
    parser = OptionParser(usage=usage)
    parser.disable_interspersed_args()
    parser.add_options(options)

    (opts, args) = parser.parse_args()

    if not args:
        parser.print_help()
        sys.exit(1)
    else:
        xmlname=args[0]

    #if there was no component name specified use the xml file name
    if opts.component is None:
        opts.component = os.path.splitext(os.path.basename(xmlname))[0]

    # initialize HAL
    try:
        halcomp = hal.component(opts.component)
    except:
        print >> sys.stderr, "**** QTvcp ERROR:    Asking for a HAL component using a name that already exists?"
        sys.exit(0)

    # build the ui
    app = QtGui.QApplication(sys.argv)
    window = MyWindow(xmlname,halcomp)
    # load optional user handler file
    if opts.usermod:
        print opts.usermod
        window.load_extension(opts.usermod)
    # actually build the widgets
    window.instance()
    # make QT widget HAL pins
    panel = qt_makepins.QTPanel(halcomp,xmlname,window)
    # call handler file's initialized function
    if opts.usermod:
        if "initialized__" in dir(window.handler_instance):
            window.handler_instance.initialized__()
    # User components are set up so report that we are ready
    halcomp.ready()
    # for window resize and or position options
    if "+" in opts.geometry:
        try:
            j =  opts.geometry.partition("+")
            pos = j[2].partition("+")
            window.move( int(pos[0]), int(pos[2]) )
        except:
            print >> sys.stderr, "**** QTvcp ERROR:    With window position data"
            parser.print_usage()
            sys.exit(1)
    if "x" in opts.geometry:
        try:
            if "+" in opts.geometry:
                j =  opts.geometry.partition("+")
                t = j[0].partition("x")
            else:
                t = window_geometry.partition("x")
            window.resize( int(t[0]), int(t[2]) )
        except:
            print >> sys.stderr, "**** QTvcp ERROR:    With window resize data"
            parser.print_usage()
            sys.exit(1)
    # maximize
    if opts.maximum:
        window.showMaximized()
    # fullscreen
    if opts.fullscreen:
        window.showFullScreen()
    # theme (styles in QT speak)
    if opts.theme:
        if not opts.theme in (QtGui.QStyleFactory.keys()):
            print "**** QTvcp WARNING: %s theme not avaialbe"% opts.theme
            for i in (QtGui.QStyleFactory.keys()):
                print i,
        else:
            QtGui.qApp.setStyle(opts.theme)
    window.setWindowTitle('QTvcp')
    window.show()
    # set up is complete, loop for user inputs
    app.exec_()
    # Ok panel has closed, exit halcomponent
    halcomp.exit()
    sys.exit()

if __name__ == '__main__':
    main()    


