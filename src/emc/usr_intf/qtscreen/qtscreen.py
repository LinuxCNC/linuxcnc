import os
import sys
import traceback
import hal
from PyQt4 import QtGui, uic
from qtvcp import qt_makepins, qt_makegui
import linuxcnc
# internationalization and localization
import locale, gettext

# BASE is the absolute path to linuxcnc base
# libdir is the path to qtscreen python files
# datadir is where the standarad GLADE files are
# imagedir is for icons
# themedir is path to system's GTK2 theme folder
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
libdir = os.path.join(BASE, "lib", "python")
datadir = os.path.join(BASE, "share", "linuxcnc")
imagedir = os.path.join(BASE, "share","qtscreen","images")
SKINPATH = os.path.join(BASE, "share","qtscreen","skins")
sys.path.insert(0, libdir)
themedir = "/usr/share/themes"
userthemedir = os.path.join(os.path.expanduser("~"), ".themes")
xmlname = os.path.join(datadir,"qtscreen.ui")
xmlname2 = os.path.join(datadir,"qtscreen2.ui")
handlername = os.path.join(datadir,"qtscreen_handler.py")
# path to the configuration the user requested
# used to see if the is local GLADE files to use
CONFIGPATH = os.environ['CONFIG_DIR']

# to help with debugging new screens
verbose_debug = False
# print debug messages if debug is true
qtscreen_debug = False
def dbg(str):
    global qtscreen_debug
    if not qtscreen_debug: return
    print str

class QTscreen: 

    def __init__(self):
        global xmlname
        global xmlname2
        global qtscreen_debug
        global verbose_debug
        self.skinname = "qtscreen"
        self.inipath = sys.argv[2]
        (progdir, progname) = os.path.split(sys.argv[0])

        # linuxcnc adds -ini to display name and optparse
        # can't understand that, so we do it manually 
        for num,temp in enumerate(sys.argv):
            if temp == '-c':
                try:
                    print ("**** qtscreen INFO: Skin name ="),sys.argv[num+1]
                    self.skinname = sys.argv[num+1]
                except:
                    pass
            if temp == '-d': qtscreen_debug = True
            if temp == '-v': verbose_debug = True

        # check for a local translation folder
        locallocale = os.path.join(CONFIGPATH,"locale")
        if os.path.exists(locallocale):
            LOCALEDIR = locallocale
            domain = self.skinname
            print ("**** GSCREEN INFO: CUSTOM locale name =",LOCALEDIR,self.skinname)
        else:
            locallocale = os.path.join(SKINPATH,"%s/locale"%self.skinname)
            if os.path.exists(locallocale):
                LOCALEDIR = locallocale
                domain = self.skinname
                print ("**** GSCREEN INFO: SKIN locale name =",LOCALEDIR,self.skinname)
            else:
                LOCALEDIR = os.path.join(BASE, "share", "locale")
                domain = "linuxcnc"
        locale.setlocale(locale.LC_ALL, '')
        locale.bindtextdomain(domain, LOCALEDIR)
        gettext.install(domain, localedir=LOCALEDIR, unicode=True)
        gettext.bindtextdomain(domain, LOCALEDIR)

        # main screen
        localglade = os.path.join(CONFIGPATH,"%s.ui"%self.skinname)
        if os.path.exists(localglade):
            print _("\n**** qtscreen INFO:  Using CUSTOM ui file from %s ****"% localglade)
            xmlname = localglade
        else:
            localglade = os.path.join(SKINPATH,"%s/%s.glade"%(self.skinname,self.skinname))
            if os.path.exists(localglade):
                print _("\n**** qtscreen INFO:  Using SKIN glade file from %s ****"% localglade)
                xmlname = localglade
            else:
                print _("\n**** qtscreen INFO:  using STOCK glade file from: %s ****"% xmlname2)

        # initialize HAL
        try:
            self.halcomp = hal.component('qtscreen')
        except:
            print >> sys.stderr, "**** QTvcp ERROR:    Asking for a HAL component using a name that already exists?"
            sys.exit(0)

        # build the ui
        self.app = QtGui.QApplication(sys.argv)
        window = qt_makegui.MyWindow(xmlname,self.halcomp,qtscreen_debug)
        # load optional user handler file
        window.load_extension(handlername)
        # actually build the widgets
        window.instance()
        # make QT widget HAL pins
        panel = qt_makepins.QTPanel(self.halcomp,xmlname,window)

        if "initialized__" in dir(window.handler_instance):
            window.handler_instance.initialized__()

        # User components are set up so report that we are ready
        self.halcomp.ready()

        # pull info from the INI file
        self.inifile = linuxcnc.ini(self.inipath)
        # add a theme
        theme = 'Plastique'
        if not theme in (QtGui.QStyleFactory.keys()):
            print "**** QTvcp WARNING: %s theme not avaialbe"% theme
            for i in (QtGui.QStyleFactory.keys()):
                print i,
        else:
            QtGui.qApp.setStyle(theme)

        window.setWindowTitle('QTscreen')
        window.show()
        print dir(window.handler_instance)

    def loop(self):
        # set up is complete, loop for user inputs
        self.app.exec_()
        # Ok panel has closed, exit halcomponent
        self.halcomp.exit()
        sys.exit()

    # finds the postgui file name and INI file path
    def postgui(self):
        postgui_halfile = self.inifile.find("HAL", "POSTGUI_HALFILE")
        return postgui_halfile,sys.argv[2]

# calls a postgui file if there is one.
# then starts qtscreen
if __name__ == "__main__":
    try:
        print "**** qtscreen INFO ini:", sys.argv[2]
        app = QTscreen()
    except KeyboardInterrupt:
        sys.exit(0)
    postgui_halfile,inifile = QTscreen.postgui(app)
    print "**** qtscreen INFO: postgui filename:",postgui_halfile
    if postgui_halfile:
        if postgui_halfile.lower().endswith('.tcl'):
            res = os.spawnvp(os.P_WAIT, "haltcl", ["haltcl", "-i",inifile, postgui_halfile])
        else:
            res = os.spawnvp(os.P_WAIT, "halcmd", ["halcmd", "-i",inifile,"-f", postgui_halfile])
        if res: raise SystemExit, res
    app.loop()
