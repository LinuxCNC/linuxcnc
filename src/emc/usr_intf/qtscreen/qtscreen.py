import os
import sys
import traceback
import hal
from optparse import Option, OptionParser
from PyQt4 import QtGui
from qtvcp import qt_makepins, qt_makegui

options = [ Option( '-c', dest='component', metavar='NAME'
                  , help="Set component name to NAME. Default is basename of UI file")
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
    def __init__(self):
        # BASE is the absolute path to linuxcnc base
        # libdir is the path to qtscreen python files
        # datadir is where the standarad GLADE files are
        # imagedir is for icons
        # themedir is path to system's GTK2 theme folder
        self.BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
        self.libdir = os.path.join(self.BASE, "lib", "python")
        self.datadir = os.path.join(self.BASE, "share", "linuxcnc")
        self.imagedir = os.path.join(self.BASE, "share","qtscreen","images")
        self.SKINPATH = os.path.join(self.BASE, "share","qtscreen","skins")
        sys.path.insert(0, self.libdir)
        self.themedir = "/usr/share/themes"
        self.userthemedir = os.path.join(os.path.expanduser("~"), ".themes")
        self.xmlname = os.path.join(self.datadir,"qtscreen.ui")
        self.xmlname2 = os.path.join(self.datadir,"qtscreen2.ui")
        self.handlername = os.path.join(self.datadir,"qtscreen_handler.py")

class QTscreen: 

    def __init__(self):
        INIPATH = None
        PATH = Paths()
        xmlname = PATH.xmlname
        self.skinname = "qtscreen"

        (progdir, progname) = os.path.split(sys.argv[0])

        usage = "usage: %prog [options] myfile.ui"
        parser = OptionParser(usage=usage)
        parser.disable_interspersed_args()
        parser.add_options(options)
        # remove [-ini filepath] that linuxcnc adds if being launched as a screen
        for i in range(len(sys.argv)):
            if sys.argv[i] =='-ini':
                # delete -ini
                del sys.argv[i]
                # pop out the ini path
                INIPATH = sys.argv.pop(i)
                break

        (opts, args) = parser.parse_args()

        if args:
            xmlname=args[0]

        #################
        # Screen specific
        #################
        if INIPATH:
            import linuxcnc
            # internationalization and localization
            import locale, gettext
            # pull info from the INI file
            self.inifile = linuxcnc.ini(INIPATH)
            self.inipath = INIPATH
            # path to the configuration the user requested
            # used to see if the is local GLADE files to use
            CONFIGPATH = os.environ['CONFIG_DIR']

            # check for a local translation folder
            locallocale = os.path.join(CONFIGPATH,"locale")
            if os.path.exists(locallocale):
                LOCALEDIR = locallocale
                domain = self.skinname
                print ("**** GSCREEN INFO: CUSTOM locale name =",LOCALEDIR,self.skinname)
            else:
                locallocale = os.path.join(PATH.SKINPATH,"%s/locale"%self.skinname)
                if os.path.exists(locallocale):
                    LOCALEDIR = locallocale
                    domain = self.skinname
                    print ("**** GSCREEN INFO: SKIN locale name =",LOCALEDIR,self.skinname)
                else:
                    LOCALEDIR = os.path.join(PATH.BASE, "share", "locale")
                    domain = "linuxcnc"
            locale.setlocale(locale.LC_ALL, '')
            locale.bindtextdomain(domain, LOCALEDIR)
            gettext.install(domain, localedir=LOCALEDIR, unicode=True)
            gettext.bindtextdomain(domain, LOCALEDIR)

            # main screen
            # look in config folder:
            localui = os.path.join(CONFIGPATH,"%s.ui"%self.skinname)
            if os.path.exists(localui):
                print _("\n**** qtscreen INFO:  Using CUSTOM ui file from %s ****"% localui)
                xmlname = localui
            else:
                # look in stock skin folder
                localui = os.path.join(PATH.SKINPATH,"%s/%s.ui"%(self.skinname,self.skinname))
                if os.path.exists(localui):
                    print _("\n**** qtscreen INFO:  Using SKIN ui file from %s ****"% localui)
                    xmlname = localui
                else:
                    # use a stock test case
                    print _("\n**** qtscreen INFO:  using STOCK ui file from: %s ****"% xmlname)
            # if no handler file specified, use stock test one
            if not opts.usermod:
                opts.usermod = PATH.handlername
            if opts.component is None:
                opts.component = 'qtscreen' 


        ##############
        # Build ui
        ##############

        #if there was no component name specified use the xml file name
        if opts.component is None:
            opts.component = os.path.splitext(os.path.basename(xmlname))[0]

        # initialize HAL
        try:
            self.halcomp = hal.component(opts.component)
        except:
            print >> sys.stderr, "**** QTscreen ERROR:    Asking for a HAL component using a name that already exists?"
            sys.exit(0)

        # initialize the window
        self.app = QtGui.QApplication(sys.argv)
        window = qt_makegui.MyWindow(xmlname,self.halcomp,opts.debug)

        # load optional user handler file
        if opts.usermod:
            window.load_extension(opts.usermod)

        # actually build the widgets
        window.instance()

        # make QT widget HAL pins
        panel = qt_makepins.QTPanel(self.halcomp,xmlname,window,opts.debug)

        # call handler file's initialized function
        if opts.usermod:
            if "initialized__" in dir(window.handler_instance):
                window.handler_instance.initialized__()

        # User components are set up so report that we are ready
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
                print 'Available themes:',
                for i in (QtGui.QStyleFactory.keys()):
                    print i,
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

        window.show()
        if INIPATH:
            self.postgui()

        # start loop
        self.app.exec_()
        self.halcomp.exit()
        sys.exit()

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

