#!/usr/bin/python3

import os
import sys
import shutil
import traceback
import hal
import signal
import subprocess

from optparse import Option, OptionParser
from PyQt5 import QtWidgets, QtCore

# Set up the base logger
#   We have do do this before importing other modules because on import
#   they set up their own loggers as children of the base logger.
from qtvcp import logger
LOG = logger.initBaseLogger('QTvcp', log_file=None, log_level=logger.INFO)


from qtvcp.core import Status, Info, QComponent, Path
from qtvcp.lib import xembed

try:
    from PyQt5.QtWebEngineWidgets import QWebEngineView as QWebView
except:
    try:
        from PyQt5.QtWebKitWidgets import QWebView
    except:
        if sys.version_info.major > 2:
            LOG.error('Qtvcp Error with loading webView - is python3-pyqt5.qtwebengine installed?')
        else:
            LOG.error('Qtvcp Error with loading webView - is python-pyqt5.qtwebkit or python-pyqt5.qtwebengine installed?')
# If log_file is none, logger.py will attempt to find the log file specified in
# INI [DISPLAY] LOG_FILE, failing that it will log to $HOME/<base_log_name>.log

# Note: In all other modules it is best to use the `__name__` attribute
#   to ensure we get a logger with the correct hierarchy.
#   Ex: LOG = logger.getLogger(__name__)

STATUS = Status()
INFO = Info()
PATH = Path()
ERROR_COUNT = 0

options = [ Option( '-c', dest='component', metavar='NAME'
                  , help="Set component name to NAME. Default is basename of UI file")
          , Option( '-a', action='store_true', dest='always_top', default=False
                  , help="set the window to always be on top")
          , Option( '-d', action='store_true', dest='debug', default=False
                  , help="Enable debug output")
          , Option( '-v', action='store_true', dest='verbose', default=False
                  , help="Enable verbose debug output")
          , Option( '-g', dest='geometry', default="", help="""Set geometry WIDTHxHEIGHT+XOFFSET+YOFFSET.
example: -g 200x400+0+100. Values are in pixel units, XOFFSET/YOFFSET is referenced from top left of screen
use -g WIDTHxHEIGHT for just setting size or -g +XOFFSET+YOFFSET for just position.""")
          , Option( '-H', dest='halfile', metavar='FILE'
                  , help="execute hal statements from FILE with halcmd after the component is set up and ready")
          , Option( '-m', action='store_true', dest='maximum', help="Force panel window to maximize")
          , Option( '-f', action='store_true', dest='fullscreen', help="Force panel window to fullscreen")
          , Option( '-t', dest='theme', default="", help="Set QT style. Default is system theme")
          , Option( '-x', dest='parent', type=int, metavar='XID'
                  , help="Reparent Qtvcp into an existing window XID instead of creating a new top level window")
          , Option( '--push_xid', action='store_true', dest='push_XID'
                  , help="reparent window into a plug add push the plug xid number to standardout")
          , Option( '-u', dest='usermod', default="", help='file path of user defined handler file')
          , Option( '-o', dest='useropts', action='append', metavar='USEROPTS', default=[]
                  , help='pass USEROPTS strings to handler under self.w.USEROPTIONS_ list varible')
          ]

class QTVCP: 
    def __init__(self):
        sys.excepthook = self.excepthook
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

        if sys.version_info.major > 2:
            # so web engine can load local images
            sys.argv.append("--disable-web-security")

        # initialize QApp so we can pop up dialogs now. 
        self.app = QtWidgets.QApplication(sys.argv)

        # we import here so that the QApp is initialized before
        # the Notify library is loaded because it uses DBusQtMainLoop
        # DBusQtMainLoop must be initialized after to work properly
        from qtvcp import qt_makepins, qt_makegui

        # ToDo: pass specific log levels as an argument, or use an INI setting
        if opts.debug:
            # Log level defaults to INFO, so set lower if in debug mode
            logger.setGlobalLevel(logger.DEBUG)
        if opts.verbose:
            # Log level defaults to INFO, so set lowest if in verbose mode
            logger.setGlobalLevel(logger.VERBOSE)

        # a specific path has been set to load from or...
        # no path set but -ini is present: default qtvcp screen...or
        # oops error
        if args:
            basepath=args[0]
        elif INIPATH:
            basepath = "qt_cnc"
        else:
            PATH.set_paths()

        # set paths using basename
        PATH.set_paths(basepath, bool(INIPATH))

        # keep track of python version during this transition
        if sys.version_info.major > 2:
            ver = 'Python 3'
        else:
            ver = 'Python 2'

        #################
        # Screen specific
        #################
        if INIPATH:
            LOG.info('green<Building A Linuxcnc Main Screen with {}>'.format(ver))
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
            gettext.install(PATH.DOMAIN, localedir=PATH.LOCALEDIR)
            gettext.bindtextdomain(PATH.DOMAIN, PATH.LOCALEDIR)

            # if no handler file specified, use stock test one
            if not opts.usermod:
                LOG.info('No handler file specified on command line')
                target =  os.path.join(PATH.CONFIGPATH, '%s_handler.py' % PATH.BASENAME)
                source =  os.path.join(PATH.SCREENDIR, 'tester/tester_handler.py')
                if PATH.HANDLER is None:
                    message = ("""
Qtvcp encountered an error; No handler file was found.
Would you like to copy a basic handler file into your config folder?
This handler file will allow display of your screen and basic keyboard jogging.

The new handlerfile's path will be:
%s

Pressing cancel will close linuxcnc.""" % target)
                    rtn = QtWidgets.QMessageBox.critical(None, "QTVCP Error", message,QtWidgets.QMessageBox.Ok | QtWidgets.QMessageBox.Cancel)
                    if rtn == QtWidgets.QMessageBox.Ok:
                        try:
                            shutil.copy(source, target)
                        except IOError as e:
                            LOG.critical("Unable to copy handler file. %s" % e)
                            sys.exit(0)
                        except:
                            LOG.critical("Unexpected error copying handler file:", sys.exc_info())
                            sys.exit(0)
                        opts.usermod = PATH.HANDLER = target
                    else:
                        LOG.critical('No handler file found or specified. User requested stopping')
                else:
                    opts.usermod = PATH.HANDLER

            # specify the HAL component name if missing
            if opts.component is None:
                LOG.info('No HAL component base name specified on command line using: {}'.format(PATH.BASENAME))
                opts.component = PATH.BASENAME

        #################
        # VCP specific
        #################
        else:
            LOG.info('green<Building A VCP Panel with {}>'.format(ver))
            # if no handler file specified, use stock test one
            if not opts.usermod:
                LOG.info('No handler file specified - using {}'.format(PATH.HANDLER))
                opts.usermod = PATH.HANDLER

            # specify the HAL component name if missing
            if opts.component is None:
                LOG.info('No HAL component base name specified - using: {}'.format(PATH.BASENAME))
                opts.component = PATH.BASENAME

        ##############
        # Build ui
        ##############

        #if there was no component name specified use the xml file name
        if opts.component is None:
            opts.component = PATH.BASENAME

        # initialize HAL
        try:
            self.halcomp = hal.component(opts.component)
            self.hal = QComponent(self.halcomp)
        except:
            LOG.critical("Asking for a HAL component using a name that already exists?")
            raise Exception('"Asking for a HAL component using a name that already exists?')

        # initialize the window
        window = qt_makegui.VCPWindow(self.hal, PATH)
 
        if opts.useropts:
            window.USEROPTIONS_ = opts.useropts
        else:
            window.USEROPTIONS_ = None

        # load optional user handler file
        if opts.usermod:
            LOG.debug('Loading the handler file')
            window.load_extension(opts.usermod)
            try:
                window.web_view = QWebView()
            except:
                window.web_view = None
            # do any class patching now
            if "class_patch__" in dir(window.handler_instance):
                window.handler_instance.class_patch__()
            # add filter to catch keyboard events
            LOG.debug('Adding the key events filter')
            myFilter = qt_makegui.MyEventFilter(window)
            self.app.installEventFilter(myFilter)

        # actually build the widgets
        window.instance()

        # title
        if INIPATH:
            title = 'QTvcp-Screen-%s'% opts.component
        else:
            title = 'QTvcp-Panel-%s'% opts.component
        window.setWindowTitle(title)

        # make QT widget HAL pins
        self.panel = qt_makepins.QTPanel(self.hal, PATH, window, opts.debug)

        # call handler file's initialized function
        if opts.usermod:
            if "initialized__" in dir(window.handler_instance):
                LOG.debug('''Calling the handler file's initialized__ function''')
                window.handler_instance.initialized__()
        # All Widgets should be added now - synch them to linuxcnc
        STATUS.forced_update()

        # call a HAL file after widgets built
        if opts.halfile:
            if opts.halfile[-4:] == ".tcl":
                cmd = ["haltcl", opts.halfile]
            else:
                cmd = ["halcmd", "-f", opts.halfile]
            res = subprocess.call(cmd, stdout=sys.stdout, stderr=sys.stderr)
            if res:
                print >> sys.stderr, "'%s' exited with %d" %(' '.join(cmd), res)
                self.shutdown()

        # User components are set up so report that we are ready
        LOG.debug('Set HAL ready')
        self.halcomp.ready()

        # embed us into an X11 window (such as AXIS)
        if opts.parent:
            window = xembed.reparent_qt_to_x11(window, opts.parent)
            forward = os.environ.get('AXIS_FORWARD_EVENTS_TO', None)
            LOG.critical('Forwarding events to AXIS is not well tested yet')
            if forward:
                xembed.XEmbedFowarding(window, forward)

        # push the window id for embedment into an external program
        if opts.push_XID:
            wid = int(window.winId())
            print >> sys.stdout,wid
            sys.stdout.flush()

        # for window resize and or position options
        if "+" in opts.geometry:
            LOG.debug('-g option: moving window')
            try:
                j =  opts.geometry.partition("+")
                pos = j[2].partition("+")
                window.move( int(pos[0]), int(pos[2]) )
            except Exception as e:
                LOG.critical("With -g window position data:\n {}".format(e))
                parser.print_help()
                self.shutdown()
        if "x" in opts.geometry:
            LOG.debug('-g option: resizing')
            try:
                if "+" in opts.geometry:
                    j =  opts.geometry.partition("+")
                    t = j[0].partition("x")
                else:
                    t = opts.geometry.partition("x")
                window.resize( int(t[0]), int(t[2]) )
            except Exception as e:
                LOG.critical("With -g window resize data:\n {}".format(e))
                parser.print_help()
                self.shutdown()

        # always on top
        if opts.always_top:
            window.setWindowFlags(QtCore.Qt.WindowStaysOnTopHint)

        # theme (styles in QT speak) specify a qss file
        if opts.theme:
            window.apply_styles(opts.theme)
        # appy qss file or default theme
        else:
            window.apply_styles()

        LOG.debug('Show window')
        # maximize
        if opts.maximum:
            window.showMaximized()
        # fullscreen
        elif opts.fullscreen:
            window.showFullScreen()
        else:
            self.panel.set_preference_geometry()
        window.show()
        if INIPATH:
            self.postgui()

        # catch control c and terminate signals
        signal.signal(signal.SIGTERM, self.shutdown)
        signal.signal(signal.SIGINT, self.shutdown)

        if opts.usermod and "before_loop__" in dir(window.handler_instance):
            LOG.debug('''Calling the handler file's before_loop__ function''')
            window.handler_instance.before_loop__()

        LOG.info('Preference path: {}'.format(PATH.PREFS_FILENAME))
        # start loop
        self.app.exec_()

        # now shut it all down
        self.shutdown()

    # finds the postgui file name and INI file path
    def postgui(self):
        postgui_halfile = INFO.POSTGUI_HALFILE_PATH
        LOG.info("postgui filename: yellow<{}>".format(postgui_halfile))
        if postgui_halfile:
            if postgui_halfile.lower().endswith('.tcl'):
                res = os.spawnvp(os.P_WAIT, "haltcl", ["haltcl", "-i",self.inipath, postgui_halfile])
            else:
                res = os.spawnvp(os.P_WAIT, "halcmd", ["halcmd", "-i",self.inipath,"-f", postgui_halfile])
            if res: raise SystemExit(res)

    # This can be called normally or by control c
    # call optional handlerfile cleanup function
    # call optional widget cleanup functions
    # shut down STATUS so no error is called
    # close out HAL pins
    def shutdown(self,signum=None,stack_frame=None):
        try:
            self.panel.window.shutdown()
            self.panel.shutdown()
        except AttributeError:
            pass
        STATUS.shutdown()
        try:
            self.halcomp.exit()
        except:
            pass
        sys.exit(0)

        # Throws up a dialog with debug info when an error is encountered 
    def excepthook(self, exc_type, exc_obj, exc_tb):
        global ERROR_COUNT
        ERROR_COUNT +=1

        lines = traceback.format_exception(exc_type, exc_obj, exc_tb)
        message = ("Qtvcp encountered an error.  The following "
                    + "information may be useful in troubleshooting:\n"
                    + 'LinuxCNC Version  : %s\n'% INFO.LINUXCNC_VERSION)
        if ERROR_COUNT > 5:
            LOG.critical("Too Manu Errors \n {}\n{}\n".format(message,''.join(lines)))
            self.shutdown()
        msg = QtWidgets.QMessageBox()
        msg.setIcon(QtWidgets.QMessageBox.Critical)
        msg.setText(message)
        msg.setInformativeText("QTvcp ERROR! Message # %d"%ERROR_COUNT)
        msg.setWindowTitle("Error")
        msg.setDetailedText(''.join(lines))
        msg.setStandardButtons(QtWidgets.QMessageBox.Retry | QtWidgets.QMessageBox.Abort)
        msg.show()
        retval = msg.exec_()
        if retval == QtWidgets.QMessageBox.Abort: #cancel button
            LOG.critical("Aborted from Error Dialog\n {}\n{}\n".format(message,''.join(lines)))
            self.shutdown()
        if ERROR_COUNT == 1:
            self.shutdown()

# starts Qtvcp
if __name__ == "__main__":
        APP = QTVCP()

