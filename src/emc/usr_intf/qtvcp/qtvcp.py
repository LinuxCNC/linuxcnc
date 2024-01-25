#!/usr/bin/env python3

import os
import sys
import shutil
import traceback
import hal
import signal
import subprocess

from optparse import Option, OptionParser
from PyQt5 import QtWidgets, QtCore, QtGui

try:
    from PyQt5.QtWebEngineWidgets import QWebEngineView as QWebView
except:
    try:
        from PyQt5.QtWebKitWidgets import QWebView
    except:
        print('Qtvcp Error with loading webView - is python3-pyqt5.qtwebengine installed?')

# keep track of the number of traceback errors
ERROR_COUNT = 0

options = [ Option( '-c', dest='component', metavar='NAME'
                  , help="Set component name to NAME. Default is basename of UI file")
          , Option( '-a', action='store_true', dest='always_top', default=False
                  , help="set the window to always be on top")
          , Option( '-d', action='store_true', dest='debug', default=False
                  , help="Enable debug output")
          , Option( '-v', action='store_true', dest='verbose', default=False
                  , help="Enable verbose debug output")
          , Option( '-q', action='store_true', dest='quiet', default=False
                  , help="Enable only error debug output")
          , Option( '-g', dest='geometry', default="", help="""Set geometry WIDTHxHEIGHT+XOFFSET+YOFFSET.
example: -g 200x400+0+100. Values are in pixel units, XOFFSET/YOFFSET is referenced from top left of screen
use -g WIDTHxHEIGHT for just setting size or -g +XOFFSET+YOFFSET for just position.""")
          , Option( '-H', dest='halfile', metavar='FILE'
                  , help="execute HAL statements from FILE with halcmd after the component is set up and ready")
          , Option( '-i', action='store_true', dest='info', default=False
                  , help="Enable info output")
          , Option( '-m', action='store_true', dest='maximum', help="Force panel window to maximize")
          , Option( '-f', action='store_true', dest='fullscreen', help="Force panel window to fullscreen")
          , Option( '-t', dest='theme', default="", help="Set QT style. Default is system theme")
          , Option( '-x', dest='parent', type=int, metavar='XID'
                  , help="Reparent Qtvcp into an existing window XID instead of creating a new top level window")
          , Option( '--push_xid', action='store_true', dest='push_XID'
                  , help="reparent window into a plug add push the plug xid number to standardout")
          , Option( '-u', dest='usermod', default="", help='file path of user defined handler file')
          , Option( '-o', dest='useropts', action='append', metavar='USEROPTS', default=[]
                  , help='pass USEROPTS strings to handler under self.w.USEROPTIONS_ list variable')
          ]

from PyQt5.QtCore import QObject, QEvent, pyqtSignal

class inputFocusFilter(QObject):
    focusIn = pyqtSignal(object)

    def eventFilter(self, widget, event):
        if event.type() == QEvent.FocusIn and not isinstance(widget,QtWidgets.QCommonStyle):
            # emit a `focusIn` signal, with the widget as its argument:
            self.focusIn.emit(widget)
        return super(inputFocusFilter, self).eventFilter(widget, event)

class MyApplication(QtWidgets.QApplication):
    def __init__(self, *arg, **kwarg):
        super(MyApplication, self).__init__(*arg, **kwarg)

        self._input_focus_widget = None

        self.event_filter = inputFocusFilter()
        self.event_filter.focusIn.connect(self.setInputFocusWidget)
        self.installEventFilter(self.event_filter)

    def setInputFocusWidget(self, widget):
        self._input_focus_widget = widget

    def inputFocusWidget(self):
        return self._input_focus_widget


class QTVCP:
    def __init__(self):
        sys.excepthook = self.excepthook
        self.STATUS = Status()
        self.INFO = Info()
        self.PATH = Path()

        INIPATH = None
        INITITLE = self.INFO.TITLE
        INIICON = self.INFO.ICON
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
                # pop out the INI path
                INIPATH = sys.argv.pop(i)
                break
        (opts, args) = parser.parse_args()

        # so web engine can load local images
        sys.argv.append("--disable-web-security")

        # initialize QApp so we can pop up dialogs now.
        global APP
        APP = MyApplication(sys.argv)

        # we import here so that the QApp is initialized before
        # the Notify library is loaded because it uses DBusQtMainLoop
        # DBusQtMainLoop must be initialized after to work properly
        from qtvcp import qt_makepins, qt_makegui

        # a specific path has been set to load from or...
        # no path set but -ini is present: default qtvcp screen...or
        # oops error
        if args:
            basepath=args[0]
        elif INIPATH:
            basepath = "qt_cnc"
        else:
            parser.print_help()
            print("")

            LOG.critical('Available built-in Machine Control Screens:')
            print(self.PATH.find_screen_dirs())
            print("")

            LOG.critical('Available built-in VCP Panels:')
            print(self.PATH.find_panel_dirs())
            sys.exit(0)

        # set paths using basename
        error = self.PATH.set_paths(basepath, bool(INIPATH))
        self.INFO.IS_SCREEN = bool(INIPATH)
        if error:
            sys.exit(0)

        # keep track of python version during this transition
        ver = 'Python 3'

        #################
        # Screen specific
        #################
        if INIPATH:
            LOG.info('green<Building A LinuxCNC Main Screen with: {}>'.format(ver))
            import linuxcnc
            # pull info from the INI file
            self.inifile = linuxcnc.ini(INIPATH)
            self.inipath = INIPATH

            # if no handler file specified, use stock test one
            if not opts.usermod:
                LOG.info('No handler file specified on command line.')
                target =  os.path.join(self.PATH.CONFIGPATH, '%s_handler.py' % self.PATH.BASENAME)
                source =  os.path.join(self.PATH.SCREENDIR, 'tester/tester_handler.py')
                if self.PATH.HANDLER is None:
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
                        opts.usermod = self.PATH.HANDLER = target
                    else:
                        LOG.critical('No handler file found or specified. User requested stopping.')
                else:
                    opts.usermod = self.PATH.HANDLER

            # specify the HAL component name if missing
            if opts.component is None:
                LOG.info('No HAL component base name specified on command line using: yellow<{}>'.format(self.PATH.BASENAME))
                opts.component = self.PATH.BASENAME

        #################
        # VCP specific
        #################
        else:
            LOG.info('green<Building A VCP Panel with: {}>'.format(ver))
            # if no handler file specified, use stock test one
            if not opts.usermod:
                LOG.info('No handler file specified - using: yellow<{}>'.format(self.PATH.HANDLER))
                opts.usermod = self.PATH.HANDLER

            # specify the HAL component name if missing
            if opts.component is None:
                LOG.info('No HAL component base name specified - using: yellow<{}>'.format(self.PATH.BASENAME))
                opts.component = self.PATH.BASENAME

        ############################
        # International translation
        ############################
        if self.PATH.LOCALEDIR is not None:
            translator = QtCore.QTranslator()
            translator.load(self.PATH.LOCALEDIR)
            APP.installTranslator(translator)
            #QtCore.QCoreApplication.installTranslator(translator)
            #print(self.app.translate("MainWindow", 'Machine Log'))

        ##############
        # Build ui
        ##############

        #if there was no component name specified use the xml file name
        if opts.component is None:
            opts.component = self.PATH.BASENAME

        # initialize HAL
        # if component fails (already exists) -> create a new name
        # and try again.
        try:
            try:
                self.halcomp = hal.component(opts.component)
            except hal.error:
                n=2
                while True:
                    try:
                        self.halcomp = hal.component('{}_{}'.format(opts.component,n))
                    except:
                        n+=1
                        if n == 25: break
                    else:
                        break
            self.hal = Qhal(self.halcomp, hal)
        except:
            LOG.critical("Asking for a HAL component using a name that already exists?")
            raise Exception('"Asking for a HAL component using a name that already exists?')

        global HAL
        HAL = self.halcomp
        # initialize the window
        self.w = window = qt_makegui.VCPWindow(self.hal, self.PATH)

        # give reference to user command line options
        if opts.useropts:
            window.USEROPTIONS_ = opts.useropts
        else:
            window.USEROPTIONS_ = None

        # load optional user handler file
        if opts.usermod:
            LOG.debug('Loading the handler file.')
            window.load_extension(opts.usermod)
            try:
                window.web_view = QWebView()
            except:
                window.web_view = None
            # do any class patching now
            if "class_patch__" in dir(window.handler_instance):
                window.handler_instance.class_patch__()
            # add filter to catch keyboard events
            LOG.debug('Adding the key events filter.')
            myFilter = qt_makegui.MyEventFilter(window)
            APP.installEventFilter(myFilter)

        # actually build the widgets
        window.instance(filename=self.PATH.XML)

        # add a default program icon - this might be overridden later
        window.setWindowIcon(QtGui.QIcon(os.path.join(self.PATH.IMAGEDIR, 'linuxcncicon.png')))

        # title
        if INIPATH:
            title ='QTvcp-Screen-%s'% opts.component
        else:
            title = 'QTvcp-Panel-%s'% opts.component
        window.setWindowTitle(title)

        # make QT widget HAL pins
        self.panel = qt_makepins.QTPanel(self.hal, self.PATH, window, opts.debug)

        # call handler file's initialized function
        if opts.usermod:
            if "initialized__" in dir(window.handler_instance):
                LOG.debug('''Calling the handler file's initialized__ function''')
                window.handler_instance.initialized__()
            # add any external handler override user commands
            if self.INFO.USER_COMMAND_FILE is None:
                self.INFO.USER_COMMAND_FILE = os.path.join(self.PATH.CONFIGPATH,'.{}rc'.format(self.PATH.BASEPATH))
            self.INFO.USER_COMMAND_FILE = self.INFO.USER_COMMAND_FILE.replace('CONFIGFOLDER',self.PATH.CONFIGPATH)
            self.INFO.USER_COMMAND_FILE = self.INFO.USER_COMMAND_FILE.replace('WORKINGFOLDER',self.PATH.WORKINGDIR)

            # TODO: what about embedded panels override files?
            # TODO: could listed them like this: for i in reversed(window._VCPWindowList):
            window.handler_instance.call_user_command_(window.handler_instance, self.INFO.USER_COMMAND_FILE)
            if "after_override__" in dir(window.handler_instance):
                LOG.debug('''Calling the handler file's after_override__ function''')
                window.handler_instance.after_override__()

        # All Widgets should be added now - sync them to linuxcnc
        self.STATUS.forced_update()

        # call a HAL file after widgets built
        if opts.halfile:
            if opts.halfile[-4:] == ".tcl":
                cmd = ["haltcl", opts.halfile]
            else:
                cmd = ["halcmd", "-f", opts.halfile]
            res = subprocess.call(cmd, stdout=sys.stdout, stderr=sys.stderr)
            if res:
                print("'%s' exited with %d" %(' '.join(cmd), res), file=sys.stderr)
                self.shutdown()

        # User components are set up so report that we are ready
        LOG.debug('Set HAL ready.')
        self.halcomp.ready()

        # embed us into an X11 window (such as AXIS)
        if opts.parent:
            try:
                from qtvcp.lib import xembed
                window = xembed.reparent_qt_to_x11(window, opts.parent)
                forward = os.environ.get('AXIS_FORWARD_EVENTS_TO', None)
                LOG.critical('Forwarding events to AXIS is not well tested yet.')
                if forward:
                    xembed.XEmbedForwarding(window, forward)
            except Exception as e:
                LOG.critical('Embedding error:{}'.format(e))

        # push the window id for embedment into an external program
        if opts.push_XID:
            wid = int(window.winId())
            print(wid, file=sys.stdout)
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
        # apply qss file or default theme
        else:
            window.apply_styles()

        LOG.debug('Show window.')
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
            self.postgui_cmd()
            # if there is a valid INI based icon path, override the default icon.
            if INIICON !='' and os.path.exists(os.path.join(self.PATH.CONFIGPATH, INIICON)):
                window.setWindowIcon(QtGui.QIcon(os.path.join(self.PATH.CONFIGPATH, INIICON)))
            if (INITITLE !=''):
                window.setWindowTitle(INITITLE)

        # catch control c and terminate signals
        signal.signal(signal.SIGTERM, self.shutdown)
        signal.signal(signal.SIGINT, self.shutdown)

        # check for handler file and if it has 'before_loop' function in
        # ineach screen/embedded panel. (screen should be last)
        # last chance to change anything before event loop.
        for i in reversed(window._VCPWindowList):
            try:
                if "before_loop__" in dir(i.handler_instance):
                    LOG.debug('''Calling handler file's before_loop__ function in object'''+str(i))
                    i.handler_instance.before_loop__()
            # probably panel with no handler file
            except:
                pass

        LOG.info('Preference path: yellow<{}>'.format(self.PATH.PREFS_FILENAME))

        # start loop
        global _app
        _app = APP.exec()

        self.shutdown()

    # finds the postgui file name and INI file path
    def postgui(self):
        postgui_halfile = self.INFO.POSTGUI_HALFILE_PATH
        LOG.info("Postgui filename: yellow<{}>".format(postgui_halfile))
        if postgui_halfile is not None:
            for f in postgui_halfile:
                f = os.path.expanduser(f)
                if f.lower().endswith('.tcl'):
                    res = os.spawnvp(os.P_WAIT, "haltcl", ["haltcl", "-i",self.inipath, f])
                else:
                    res = os.spawnvp(os.P_WAIT, "halcmd", ["halcmd", "-i",self.inipath,"-f", f])
                if res: raise SystemExit(res)

    def postgui_cmd(self):
        postgui_commands = self.INFO.POSTGUI_HAL_COMMANDS
        LOG.info("Postgui commands: yellow<{}>".format(postgui_commands))
        if postgui_commands is not None:
            for f in postgui_commands:
                f = os.path.expanduser(f)
                res = os.spawnvp(os.P_WAIT, "halcmd", ["halcmd"] + f.split())
                if res: raise SystemExit(res)

    # This can be called by control c or an early error.
    # close out HAL pins
    def shutdown(self,signum=None,stack_frame=None):

        from qtvcp.core import Status
        s = Status()
        LOG.debug('Status shutdown')
        s.shutdown()

        # call pyqt qsetting saving process
        try:
            self.w.sync_qsettings()
        except:
            pass

        # call handler file shutdown functions
        try:
            self.w.shutdown()
        except:
            pass

        # call HAL widget 'hal_cleanuo_' functions
        try:
            self.w.panel_.shutdown()
        except Exception as e:
            print(e)
            pass

        LOG.debug('Exiting HAL')
        HAL.exit()

        # Throws up a dialog with debug info when an error is encountered
    def excepthook(self, exc_type, exc_obj, exc_tb):
        global ERROR_COUNT
        ERROR_COUNT +=1

        # we count errors because often there are multiple and the first is the
        # only important one.
        if ERROR_COUNT == 1:
            lines = traceback.format_exception(exc_type, exc_obj, exc_tb)
            self._message = ("Qtvcp encountered an error.  The following "
                    + "information may be useful in troubleshooting:\n"
                    + 'LinuxCNC Version  : %s\n'% self.INFO.LINUXCNC_VERSION)

            msg = QtWidgets.QMessageBox()
            msg.setIcon(QtWidgets.QMessageBox.Critical)
            msg.setText(self._message)
            msg.setInformativeText("QTvcp ERROR! Message # %d"%ERROR_COUNT)
            msg.setWindowTitle("Error")
            msg.setDetailedText(''.join(lines))
            msg.setStandardButtons(QtWidgets.QMessageBox.Retry | QtWidgets.QMessageBox.Abort)
            msg.show()

            # hack to scroll details to bottom;
            for i in msg.children():
                for j in i.children():
                    if isinstance(j, QtWidgets.QTextEdit):
                            j.moveCursor(QtGui.QTextCursor.End)
            # hack to auto open details box
            for i in msg.buttons():
                if msg.buttonRole(i) == QtWidgets.QMessageBox.ActionRole:
                    i.click()

            retval = msg.exec_()
            if retval == QtWidgets.QMessageBox.Abort: #cancel button
                LOG.critical("Aborted from Error Dialog\n {}\n{}\n".format(self._message,''.join(lines)))
                self.shutdown()
            else:
                ERROR_COUNT = 0
                LOG.critical("Retry from Error Dialog\n {}\n{}\n".format(self._message,''.join(lines)))

# starts Qtvcp
if __name__ == "__main__":
        HAL = None
        _qtvcp = None
        _app = None
        # Set up the base logger
        #   We have do do this before importing other modules because on import
        #   they set up their own loggers as children of the base logger.

        # If log_file is none, logger.py will attempt to find the log file specified in
        # INI [DISPLAY] LOG_FILE, failing that it will log to $HOME/<base_log_name>.log

        # Note: In all other modules it is best to use the `__name__` attribute
        #   to ensure we get a logger with the correct hierarchy.
        #   Ex: LOG = logger.getLogger(__name__)

        from qtvcp import logger
        LOG = logger.initBaseLogger('QTvcp', log_file=None, log_level=logger.WARNING)

        # we set the log level early so the imported modules get the right level
        if '-d' in sys.argv:
            # Log level defaults to WARNING, so set lower if in debug mode
            logger.setGlobalLevel(logger.DEBUG)
            LOG.debug('DEBUGGING logging on')
        elif '-i' in sys.argv:
            # Log level defaults to WARNING, so set lower if in info mode
            logger.setGlobalLevel(logger.INFO)
            LOG.info('INFO logging on')
        elif '-v' in sys.argv:
            # Log level defaults to WARNING, so set lowest if in verbose mode
            logger.setGlobalLevel(logger.VERBOSE)
            LOG.verbose('VERBOSE logging on')
        elif '-q' in sys.argv:
            logger.setGlobalLevel(logger.ERROR)

        # these libraries log when imported so logging level must already be set. 
        from qtvcp.core import Status, Info, Qhal, Path

        _qtvcp = QTVCP()
        os._exit(_app)
