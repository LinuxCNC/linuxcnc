import os
import sys
import subprocess

from PyQt5 import QtGui, QtCore, QtWidgets, uic
import traceback
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
# Set up logging
from . import logger

log = logger.getLogger(__name__)

# Force the log level for this module
#log.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class Trampoline(object):
    def __init__(self, methods):
        self.methods = methods

    def __call__(self, *a, **kw):
        for m in self.methods:
            m(*a, **kw)


class MyEventFilter(QtCore.QObject):
    def __init__(self, window):
        super(MyEventFilter, self).__init__()
        self.w = window
        self.has_key_p_handler = 'keypress_event__' in dir(self.w.handler_instance)
        self.has_key_r_handler = 'keyrelease_event__' in dir(self.w.handler_instance)
        self.has_process_key_handler = 'processed_key_event__' in dir(self.w.handler_instance)

    def process_event(self, event, pressed):
        shift = ctrl = False
        if (event.modifiers() & QtCore.Qt.ShiftModifier):
            shift = True
        if (event.modifiers() & QtCore.Qt.ControlModifier):
            ctrl = True
        code = event.key()
        key = event.text()
        return (pressed, key, code, shift, ctrl)

    def eventFilter(self, receiver, event):
        # in pyqt5 QWindow gets all events before the widgets inside it do
        # we need the widgets inside it to get their events first
        # this line line provides that.
        # (pyqt4 did not require this)
        if isinstance(receiver, QtGui.QWindow):
            return super(MyEventFilter, self).eventFilter(receiver, event)
        # Run in try statement so if we want to event to run through normal event routines,
        # we just raise an error and run the super class event handler.
        # This is necessary because if the widget (such as dialogs) are owned by c++ rather then python,
        # it causes an error and the keystrokes don't get to the widgets.
        try:
            if (event.type() == QtCore.QEvent.KeyPress):
                handled = False
                handled = self.w.keyPressTrap(event)
                if (self.has_key_p_handler):
                    handled = self.w.handler_instance.keypress_event__(receiver, event)
                elif self.has_process_key_handler:
                    p, k, c, s, ctrl = self.process_event(event, True)
                    handled = self.w.handler_instance.processed_key_event__(receiver, event, p, k, c, s, ctrl)
                if handled: return True
            elif (event.type() == QtCore.QEvent.KeyRelease):
                handled = False
                handled = self.w.keyReleaseTrap(event)
                if (self.has_key_r_handler):
                    handled = self.w.handler_instance.keyrelease_event__(event)
                elif self.has_process_key_handler:
                    p, k, c, s, ctrl = self.process_event(event, False)
                    handled = self.w.handler_instance.processed_key_event__(receiver, event, p, k, c, s, ctrl)
                if handled: return True
            elif event.type() in (QtCore.QEvent.FocusIn, QtCore.QEvent.FocusOut):
                self.w.handler_instance.processed_focus_event__(receiver, event)
            # Call Base Class Method to Continue Normal Event Processing
            return super(MyEventFilter, self).eventFilter(receiver, event)
        except:
            return super(MyEventFilter, self).eventFilter(receiver, event)


class _VCPWindow(QtWidgets.QMainWindow):
    def __init__(self, halcomp=None, path=None):
        super(_VCPWindow, self).__init__()
        # only initialize once for all instances
        if self.__class__._instanceNum >= 1:
            return
        self.__class__._instanceNum += 1

        self._idName = "TopObject" # initial name, will be replaced
        self.halcomp = halcomp
        self.has_closing_handler = False
        self.setFocus(True)
        self.PATHS = path
        self.PREFS_ = None
        self.originalCloseEvent_ = self.closeEvent
        self._halWidgetList = []
        self._VCPWindowList = []
        self._DialogList = []
        self.settings = QtCore.QSettings('QtVcp', path.BASENAME)
        log.info('Qsettings file path: yellow<{}>'.format(self.settings.fileName()))
        # make an instance with embedded variables so they
        # are available to all subclassed objects
        _HalWidgetBase(halcomp, path, self)

    def registerHalWidget(self, widget):
        self._halWidgetList.append(widget)

    def getRegisteredHalWidgetList(self):
        return self._halWidgetList

    def registerDialog(self, widget):
        self._DialogList.append(widget)

    def getRegisteredDialogList(self):
        return self._DialogList

    # These catch events if using a plain VCP panel and there is no handler file
    def keyPressEvent(self, e):
        self.keyPressTrap(e)

    def keyReleaseEvent(self, e):
        self.keyReleaseTrap(e)

    # These can get class patched by xembed library to catch events
    def keyPressTrap(self, e):
        return False

    def keyReleaseTrap(self, e):
        return False

    # call closing function on main and embedded windows.
    def shutdown(self):
        for i in (self._VCPWindowList):
            if 'closing_cleanup__' in dir(i):
                log.debug('Calling handler file closing_cleanup__ function of {}.'.format(i._idName))
                i.closing_cleanup__()

    def sync_qsettings(self):
        try:
            self.settings.sync()
            log.debug('Qsettings sync called:')
        except Exception as e:
            log.debug('Error with Qsettings sync function:\n {}'.format(e))

    # resource files are compiled from the qrs file
    # with an installed version of linuxcnc we can't save the resource
    # file in the installed directory without being root,
    # so we always make a directory in the config folder to put it in
    def load_resources(self):
        def qrccompile(qrcname, qrcpy):
            log.info('Compiling qrc: {} to \n {}'.format(qrcname, qrcpy))
            try:
                subprocess.call(["pyrcc5", "-o", "{}".format(qrcpy), "{}".format(qrcname)])
            except OSError as e:
                log.error(
                    '{}, pyrcc5 error. try in terminal: sudo apt install pyqt5-dev-tools to install dev tools'.format(
                        e))
                msg = QtWidgets.QMessageBox()
                msg.setIcon(QtWidgets.QMessageBox.Critical)
                msg.setText("QTvcp qrc compiling ERROR! ")
                msg.setInformativeText(
                    'Qrc Compile error, try: "sudo apt install pyqt5-dev-tools" to install dev tools')
                msg.setWindowTitle("Error")
                msg.setDetailedText('You can continue but some images may be missing')
                msg.setStandardButtons(QtWidgets.QMessageBox.Retry | QtWidgets.QMessageBox.Abort)
                msg.show()
                retval = msg.exec_()
                if retval == QtWidgets.QMessageBox.Abort:  # cancel button
                    log.critical("Canceled from qrc compiling Error Dialog.\n")
                    raise SystemError('pyrcc5 compiling error: try: "sudo apt install pyqt5-dev-tools"')

        qrcname = self.PATHS.QRC
        qrcpy = self.PATHS.QRCPY

        # Is there a qrc file in directory?
        if qrcname is not None:
            qrcTime = os.stat(qrcname).st_mtime
            if qrcpy is not None and os.path.isfile(qrcpy):
                pyTime = os.stat(qrcpy).st_mtime
                # is py older then qrc file?
                if pyTime < qrcTime:
                    qrccompile(qrcname, qrcpy)
            # there is a qrc file but no resources.py file...compile it
            else:
                if not os.path.isfile(qrcpy):
                    # make the missing directory
                    try:
                        os.makedirs(os.path.split(qrcpy)[0])
                    except Exception as e:
                        log.warning('Could not make directory {} resource file: {}'.format(os.path.split(qrcpy)[0], e))
                qrccompile(qrcname, qrcpy)

        # is there a resource.py in the directory?
        # if so add a path to it so we can import it.
        if qrcpy is not None and os.path.isfile(qrcpy):
            try:
                sys.path.insert(0, os.path.split(qrcpy)[0])
                import importlib
                importlib.import_module('resources', os.path.split(qrcpy)[0])
                log.info('Imported resources.py file: yellow<{}>'.format(qrcpy))
            except Exception as e:
                log.warning('Could not load {} resource file: yellow<{}>'.format(qrcpy, e))
        else:
            log.info('No resource file to load: yellow<{}>'.format(qrcpy))

    def instance(self, filename):
        self.load_resources()
        try:
            instance = uic.loadUi(filename, self)
        except AttributeError as e:
            formatted_lines = traceback.format_exc().splitlines()
            if 'slotname' in formatted_lines[-2]:
                log.critical('Missing slot name in handler file: {}'.format(e))
                message = '''A widget in the ui file, was assigned a signal \
call to a missing function name in the handler file?\n
There may be more. Some functions might not work.\n
Python Error:\n {}'''.format(str(e))
                rtn = QtWidgets.QMessageBox.critical(None, "QTVCP Error", message)
            else:
                log.critical(e)
                raise

    def apply_styles(self, fname=None):
        if self.PATHS.IS_SCREEN:
            DIR = self.PATHS.SCREENDIR
            BNAME = self.PATHS.BASENAME
        else:
            DIR = self.PATHS.PANELDIR
            BNAME = self.PATHS.BASENAME
        # apply one word system theme
        if fname in (list(QtWidgets.QStyleFactory.keys())):
            QtWidgets.qApp.setStyle(fname)
            log.info('Applied System Style name: yellow<{}>'.format(fname))
            return

        # Check for Preference file specified qss
        if fname is None:
            if not self.PREFS_ is None:
                path = self.PREFS_.getpref('style_QSS_Path', 'DEFAULT', str, 'BOOK_KEEPING')
                if path.lower() == 'none':
                    return
                if not path.lower() == 'default':
                    fname = path

        # check for default base named qss file
        if fname is None:
            if self.PATHS.QSS is not None:
                fname = self.PATHS.QSS

        # if qss file is not a file, try expanding/adding a leading path
        if fname is not None:
            if not os.path.isfile(fname):
                temp = os.path.join(os.path.expanduser(fname))
                qssname = os.path.join(DIR, BNAME, fname + '.qss')
                if not os.path.isfile(fname):
                    qssname = temp
            else:
                qssname = fname
        else:
            return
        try:
            qss_file = open(qssname).read()
            # qss files aren't friendly about changing image paths
            qss_file = qss_file.replace('url(:/newPrefix/images', 'url({}'.format(self.PATHS.IMAGEDIR))
            self.setStyleSheet(qss_file)
            log.info('Applied Style Sheet path: yellow<{}>'.format(qssname))
            return
        except:
            if fname:
                themes = ''
                log.error('QSS Filepath Error: {}'.format(qssname))
                log.error("{} theme not available.".format(fname))
                current_theme = str(QtWidgets.qApp.style().objectName())
                for i in (list(QtWidgets.QStyleFactory.keys())):
                    themes += (', {}'.format(i))
                log.error('QTvcp Available system themes: green<{}> {}'.format(current_theme, themes))

    def load_extension(self, handlerpath, obj = ''):
        methods, self[obj].handler_module, self[obj].handler_instance = self._load_handlers([handlerpath], self.halcomp, self[obj])
        for i in methods:
            self[obj][i] = methods[i]
        # See if the handler file has a closing function to call first
        self[obj].has_closing_handler = 'closing_cleanup__' in dir(self[obj].handler_instance)

    def _load_handlers(self, usermod, halcomp, widgets):
        hdl_func = 'get_handlers'
        mod = None
        object = None

        def add_handler(method, f):
            if method in handlers:
                handlers[method].append(f)
            else:
                handlers[method] = [f]

        handlers = {}
        for u in usermod:
            (directory, filename) = os.path.split(u)
            (basename, extension) = os.path.splitext(filename)
            if directory == '':
                directory = '.'
            if directory not in sys.path:
                sys.path.insert(0, directory)
                log.debug('Adding import dir: yellow<{}>'.format(directory))

            try:
                mod = __import__(basename)
                # inject/class patch function to read an override file
                # this will be called from qtvcp.py later
                mod.HandlerClass.call_user_command_ = self.call_user_command_
            except ImportError as e:
                log.critical("Module '{}' skipped - import error: ".format(basename), exc_info=e)
                raise
                continue
            log.debug("Module '{}' imported green<OK>".format(mod.__name__))

            try:
                # look for 'get_handlers' function
                h = getattr(mod, hdl_func, None)
                if h and callable(h):
                    log.debug("Module '{}' : '{}' function found.".format(mod.__name__, hdl_func))
                    objlist = h(halcomp, widgets, self.PATHS)  # this sets the handler class signature
                else:
                    # the module has no get_handlers() callable.
                    # in this case we permit any callable except class Objects in the module to register as handler
                    log.debug("Module '{}': no '{}' function - registering only functions as callbacks."
                              .format(mod.__name__, hdl_func))
                    objlist = [mod]
                # extract callback candidates
                for object in objlist:
                    log.debug("Registering handlers in module {} object {}".format(mod.__name__, object))
                    if isinstance(object, dict):
                        methods = list(dict.items())
                    else:
                        methods = [(n, getattr(object, n, None)) for n in dir(object)]
                    for method, f in methods:
                        if method.startswith('_'):
                            continue
                        if callable(f):
                            log.verbose("Register callback '{}'".format(method))
                            add_handler(method, f)

            except Exception as e:
                log.exception("Trouble looking for handlers in '{}':".format(basename), exc_info=e)
                # we require a working handler file!
                raise

        # Wrap lists in Trampoline, unwrap single functions
        for n, v in list(handlers.items()):
            if len(v) == 1:
                handlers[n] = v[0]
            else:
                handlers[n] = Trampoline(v)

        return handlers, mod, object

    # this is the function that is injected into the handler file to read an override file
    # this will be called from qtvcp.py later
    def call_user_command_(self, klass, rcfile = "~/.qtvcprc"):
        # substitute for any keywords
        rcfile.replace('CONFIGFOLDER',self.PATHS.CONFIGPATH)
        rcfile.replace('WORKINGFOLDER',self.PATHS.WORKINGDIR)
        rcfile = os.path.expanduser(rcfile)

        if os.path.exists(rcfile):
            log.info('Handler Override file found at: yellow<{}>'.format(rcfile))
            try:
                local = {'self': klass, 'rcfile': rcfile}
                exec(compile(open(rcfile, "rb").read(), rcfile, 'exec'),local)
            except Exception as e:
                log.exception(e)
        else:
            log.info('No Handler Override file at: yellow<{}>'.format(rcfile))

    # facilitate the main screen to create another window panel
    def makeMainPage(self, name):
        return MainPage(main=self,name=name )

# This is used to build a window that a qtvcp panel can be built from.
# The idea is to embed the panel into the main screen
# This allow the completely separate panels to be incorporated in
# as the user sees fit.
# the big deal is to still allow the panel to have a functioning handler file
class MainPage(QtWidgets.QMainWindow):
    def __init__(self, *args, **kwargs):
        super().__init__()
        for key, value in kwargs.items():
            if key =='main':
                self.halcomp = value.halcomp
                self.PATHS = value.PATHS
                self.MAIN = value
            elif key == 'name':
                self._idName = value

    def instance(self, filename):
        try:
            instance = uic.loadUi(filename, self)
        except AttributeError as e:
            formatted_lines = traceback.format_exc().splitlines()
            if 'slotname' in formatted_lines[-2]:
                log.critical('{}: Missing slot name in handler file: {}'.format(self._idName, e))
                message = '''A widget in the ui file, {} was assigned a signal \
call to a missing function name in the handler file?\n
There may be more. Some functions might not work.\n
Python Error:\n {}'''.format(self._idName, str(e))
                rtn = QtWidgets.QMessageBox.critical(None, "QTVCP Error", message)
            else:
                log.critical(e)
                raise

    def load_extension(self, handlerpath):
        methods, self.handler_module, self.handler_instance = self._load_handlers([handlerpath], self.halcomp, self)
        for i in methods:
            self[i] = methods[i]
        # See if the handler file has a closing function to call first
        self.has_closing_handler = 'closing_cleanup__' in dir(self.handler_instance)

    def _load_handlers(self, usermod, halcomp, widgets):
        hdl_func = 'get_handlers'
        mod = None
        object = None

        def add_handler(method, f):
            if method in handlers:
                handlers[method].append(f)
            else:
                handlers[method] = [f]

        handlers = {}
        for u in usermod:
            (directory, filename) = os.path.split(u)
            (basename, extension) = os.path.splitext(filename)
            if directory == '':
                directory = '.'
            if directory not in sys.path:
                sys.path.insert(0, directory)
                log.debug('{}: adding import dir: yellow<{}>'.format(self._idName, directory))

            try:
                mod = __import__(basename)
            except ImportError as e:
                log.critical("{}: module '{}' skipped - import error: "
                        .format(self._idName, basename), exc_info=e)
                raise
                continue
            log.debug("{}: module '{}' imported green<OK>".format(self._idName, mod.__name__))

            try:
                # look for 'get_handlers' function
                h = getattr(mod, hdl_func, None)
                if h and callable(h):
                    log.debug("{}: module '{}' : '{}' function found".format(self._idName, mod.__name__, hdl_func))
                    objlist = h(halcomp, widgets, self.PATHS)  # this sets the handler class signature
                else:
                    # the module has no get_handlers() callable.
                    # in this case we permit any callable except class Objects in the module to register as handler
                    log.debug("{}: module '{}': no '{}' function - registering only functions as callbacks"
                              .format(self._idName, mod.__name__, hdl_func))
                    objlist = [mod]
                # extract callback candidates
                for object in objlist:
                    log.debug("{}: Registering handlers in module {} object {}"
                            .format(self._idName, mod.__name__, object))
                    if isinstance(object, dict):
                        methods = list(dict.items())
                    else:
                        methods = [(n, getattr(object, n, None)) for n in dir(object)]
                    for method, f in methods:
                        if method.startswith('_'):
                            continue
                        if callable(f):
                            log.debug("{}: Register callback '{}'".format(self._idName, method))
                            add_handler(method, f)

            except Exception as e:
                log.exception("{}: Trouble looking for handlers in '{}':"
                        .format(self._idName, basename), exc_info=e)
                # we require a working handler file!
                raise

        # Wrap lists in Trampoline, unwrap single functions
        for n, v in list(handlers.items()):
            if len(v) == 1:
                handlers[n] = v[0]
            else:
                handlers[n] = Trampoline(v)

        return handlers, mod, object

    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)

class VCPWindow(_VCPWindow):
    _instance = None
    _instanceNum = 0

    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = _VCPWindow.__new__(cls, *args, **kwargs)
        return cls._instance

    def __getitem__(self, item):
        if item == '':
            return self
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)
