import sys
import os
import traceback
import hal
from optparse import Option, OptionParser
from PyQt4 import QtGui
from qtvcp import qt_makepins, qt_makegui

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
    window = qt_makegui.MyWindow(xmlname,halcomp,opts.debug)
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


