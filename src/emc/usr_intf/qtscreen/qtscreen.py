import os
import sys
import traceback
import hal
from optparse import Option, OptionParser
from PyQt4 import QtGui, QtCore
from qtvcp import qt_makepins, qt_makegui

options = [ Option( '-c', dest='component', metavar='NAME'
                  , help="Set component name to NAME. Default is basename of UI file")
          , Option( '-a', action='store_true', dest='always_top', default=False
                  , help="set the window to always be on top")
          , Option( '-d', action='store_true', dest='debug', default=False
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

# to help with debugging new screens
verbose_debug = False
# print debug messages if debug is true
qtscreen_debug = False
def dbg(str):
    global qtscreen_debug
    if not qtscreen_debug: return
    print str

class Paths():
    def __init__(self,filename):

        self.BASENAME = os.path.splitext(os.path.basename(filename))[0]
        print self.BASENAME
        self.VCP_UI = '%s.ui'% os.path.splitext(filename)[0]
        self.VCP_HANDLER = '%s_handler.py'% self.BASENAME
        if not os.path.exists(self.VCP_HANDLER):
            self.VCP_HANDLER = None

    def add_screen_paths(self):
        # BASE is the absolute path to linuxcnc base
        # libdir is the path to qtscreen python files
        # DATADIR is where the standarad UI files are
        # IMAGEDIR is for icons
        self.BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
        self.libdir = os.path.join(self.BASE, "lib", "python")
        sys.path.insert(0, self.libdir)
        self.IMAGEDIR = os.path.join(self.BASE, "share","qtscreen","images")
        self.SKINDIR = os.path.join(self.BASE, "share","qtscreen","skins")
        #self.themedir = "/usr/share/themes"
        self.userthemedir = os.path.join(os.path.expanduser("~"), ".themes")
        # path to the configuration the user requested
        self.CONFIGPATH = os.environ['CONFIG_DIR']
        sys.path.insert(0, self.CONFIGPATH)

        # check for a local translation folder
        locallocale = os.path.join(self.CONFIGPATH,"locale")
        if os.path.exists(locallocale):
            self.LOCALEDIR = locallocale
            self.DOMAIN = self.BASENAME
            print ("**** GSCREEN INFO: CUSTOM locale name =",self.LOCALEDIR,self.BASENAME)
        else:
            locallocale = os.path.join(self.SKINDIR,"%s/locale"% self.BASENAME)
            if os.path.exists(locallocale):
                self.LOCALEDIR = locallocale
                self.DOMAIN = self.BASENAME
                print ("**** GSCREEN INFO: SKIN locale name =",self.LOCALEDIR,self.BASENAME)
            else:
                self.LOCALEDIR = os.path.join(self.BASE, "share", "locale")
                self.DOMAIN = "linuxcnc"
        # check for local XML file
        # look in config folder:
        localui = os.path.join(self.CONFIGPATH,"%s.ui"% self.BASENAME)
        dbg("**** QTSCREEN DEBUG: checking for .ui in: %s"% localui)
        if os.path.exists(localui):
            print ("\n**** qtscreen INFO:  Using CUSTOM ui file from %s ****"% localui)
            self.XML = localui
        else:
            # look in stock skin folder
            localui = os.path.join(self.SKINDIR,"%s/%s.ui"%(self.BASENAME,self.BASENAME))
            dbg("**** QTSCREEN DEBUG: checking for .ui in: %s"% localui)
            if os.path.exists(localui):
                print ("\n**** qtscreen INFO:  Using SKIN ui file from %s ****"% localui)
                self.XML = localui
            else:
                # error
                self.XML = None
                print ("\n**** QTSCREEN ERROR:  No UI file found ****")
                sys.exit(0)

        # look for custom handler files:
        handler_fn = "%s_handler.py"% self.BASENAME
        local_handler_path = os.path.join(self.CONFIGPATH,handler_fn)
        skin_handler_path = os.path.join(self.SKINDIR,"%s/%s"%(self.BASENAME,handler_fn))
        dbg("**** QTSCREEN DEBUG: checking for handler file in: %s"% local_handler_path)
        dbg("**** QTSCREEN DEBUG: checking for handler file in: %s"% skin_handler_path)
        if os.path.exists(local_handler_path):
            self.HANDLER = local_handler_path
            print ("**** QTSCREEN INFO: Local handler file path: %s"%self.HANDLER)
        elif os.path.exists(skin_handler_path):
            self.HANDLER = skin_handler_path
            dbg("**** QTSCREEN INFO: Skin handler file path: %s"%self.HANDLER)
        else:
            self.HANDLER = None
            dbg("**** QTSCREEN WARNING: No handler file found")
            sys.exit(0)

class QTscreen: 
    def __init__(self):
        INIPATH = None

        usage = "usage: %prog [options] myfile.ui"
        parser = OptionParser(usage=usage)
        parser.disable_interspersed_args()
        parser.add_options(options)
        # remove [-ini filepath] that linuxcnc adds if being launched as a screen
        # keep a reference of that path
        for i in range(len(sys.argv)):
            if sys.argv[i] =='-ini':
                # delete -ini
                del sys.argv[i]
                # pop out the ini path
                INIPATH = sys.argv.pop(i)
                break

        (opts, args) = parser.parse_args()
        global qtscreen_debug
        qtscreen_debug = opts.debug
        # a specific path has been set to load from or...
        # no path set but -ini is present: default qtscreen screen...or
        # oops error
        if args:
            basepath=args[0]
        elif INIPATH:
            basepath = "qt_cnc"
        else:
            print '**** QTscreen Error in path'
            sys.exit()

        # set paths using basename
        PATH = Paths(basepath)

        #################
        # Screen specific
        #################
        if INIPATH:
            dbg('Linuxcnc Main Screen')
            import linuxcnc
            # internationalization and localization
            import locale, gettext
            # pull info from the INI file
            self.inifile = linuxcnc.ini(INIPATH)
            self.inipath = INIPATH
            # screens require more path info
            PATH.add_screen_paths()

            # International translation
            locale.setlocale(locale.LC_ALL, '')
            locale.bindtextdomain(PATH.DOMAIN, PATH.LOCALEDIR)
            gettext.install(PATH.DOMAIN, localedir=PATH.LOCALEDIR, unicode=True)
            gettext.bindtextdomain(PATH.DOMAIN, PATH.LOCALEDIR)

            # if no handler file specified, use stock test one
            if not opts.usermod:
                opts.usermod = PATH.HANDLER

            # specify the HAL component name if missing
            if opts.component is None:
                opts.component = PATH.BASENAME
            # find screen xml file
            xmlpath = PATH.XML

        #################
        # VCP specific
        #################
        else:
            dbg('VCP screen')
            xmlpath = PATH.VCP_UI
            # If no handler file was specified, check for one using basename
            if not opts.usermod:
                if PATH.VCP_HANDLER:
                    dbg('found VCP handler')
                    opts.usermod = PATH.VCP_HANDLER

        ##############
        # Build ui
        ##############

        #if there was no component name specified use the xml file name
        if opts.component is None:
            opts.component = PATH.BASENAME

        # initialize HAL
        try:
            self.halcomp = hal.component(opts.component)
        except:
            print >> sys.stderr, "**** QTscreen ERROR:    Asking for a HAL component using a name that already exists?"
            sys.exit(0)

        # initialize the window
        self.app = QtGui.QApplication(sys.argv)
        window = qt_makegui.MyWindow(xmlpath,self.halcomp,opts.debug)
 
        # load optional user handler file
        if opts.usermod:
            dbg('Loading the handler file')
            window.load_extension(opts.usermod,PATH)
            # add filter to catch keyboard events
            dbg('Adding the key events filter')
            myFilter = qt_makegui.MyEventFilter(window)
            self.app.installEventFilter(myFilter)

        # actually build the widgets
        window.instance()

        # make QT widget HAL pins
        panel = qt_makepins.QTPanel(self.halcomp,xmlpath,window,opts.debug)

        # call handler file's initialized function
        if opts.usermod:
            if "initialized__" in dir(window.handler_instance):
                dbg('''Calling the handler file's initialized__ function''')
                window.handler_instance.initialized__()

        # User components are set up so report that we are ready
        dbg('Set HAL ready')
        self.halcomp.ready()

        # embed window in another program
        if opts.parent:
            print 'Xembed Option not available yet'
            sys.exit(1)
            window = xembed.reparent(window, opts.parent)
            forward = os.environ.get('AXIS_FORWARD_EVENTS_TO', None)
            if forward:
                xembed.keyboard_forward(window, forward)

        # for window resize and or position options
        if "+" in opts.geometry:
            try:
                j =  opts.geometry.partition("+")
                pos = j[2].partition("+")
                window.move( int(pos[0]), int(pos[2]) )
            except:
                print >> sys.stderr, "**** QTscreen ERROR:    With window position data"
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
                print >> sys.stderr, "**** QTscreen ERROR:    With window resize data"
                parser.print_usage()
                sys.exit(1)

        # always on top
        if opts.always_top:
            window.setWindowFlags(QtCore.Qt.WindowStaysOnTopHint)

        # maximize
        if opts.maximum:
            window.showMaximized()

        # fullscreen
        if opts.fullscreen:
            window.showFullScreen()

        # theme (styles in QT speak)
        if opts.theme:
            if not opts.theme in (QtGui.QStyleFactory.keys()):
                print "**** QTscreen WARNING: %s theme not avaialbe"% opts.theme
                if opts.debug:
                    print 'QTscreen Available themes:'
                    theme=[]
                    for i in (QtGui.QStyleFactory.keys()):
                        theme.append('%s'%i)
                    print theme
            else:
                QtGui.qApp.setStyle(opts.theme)
        # windows theme is default for screens
        elif INIPATH:
            QtGui.qApp.setStyle('Windows')

        # title
        if INIPATH:
            title = 'QTscreen-%s'% opts.component
        else:
            title = 'QTvcp-%s'% opts.component
        window.setWindowTitle(title)

        dbg('Show window')
        window.show()
        if INIPATH:
            self.postgui()

        # start loop
        self.app.exec_()
        self.halcomp.exit()
        sys.exit(0)

    # finds the postgui file name and INI file path
    def postgui(self):
        postgui_halfile = self.inifile.find("HAL", "POSTGUI_HALFILE")
        print "**** QTscreen INFO: postgui filename:",postgui_halfile
        if postgui_halfile:
            if postgui_halfile.lower().endswith('.tcl'):
                res = os.spawnvp(os.P_WAIT, "haltcl", ["haltcl", "-i",self.inipath, postgui_halfile])
            else:
                res = os.spawnvp(os.P_WAIT, "halcmd", ["halcmd", "-i",self.inipath,"-f", postgui_halfile])
            if res: raise SystemExit, res

# starts qtscreen
if __name__ == "__main__":
    try:
        APP = QTscreen()
    except KeyboardInterrupt:
        sys.exit(0)

