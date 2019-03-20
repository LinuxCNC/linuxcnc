import os,sys
from PyQt5 import QtGui, QtCore, QtWidgets, uic
import traceback

# Set up logging
import logger
log = logger.getLogger(__name__)
# Set the log level for this module
#log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class Trampoline(object):
    def __init__(self,methods):
        self.methods = methods

    def __call__(self, *a, **kw):
        for m in self.methods:
            m(*a, **kw)

class MyEventFilter(QtCore.QObject):
    def __init__(self,window):
        super(MyEventFilter, self).__init__()
        self.w = window
        self.has_key_p_handler = 'keypress_event__' in dir(self.w.handler_instance)
        self.has_key_r_handler = 'keyrelease_event__' in dir(self.w.handler_instance)
        self.has_process_key_handler = 'processed_key_event__' in dir(self.w.handler_instance)

    def process_event(self,event,pressed):
        shift = ctrl = False
        if (event.modifiers() & QtCore.Qt.ShiftModifier):
            shift = True
        if (event.modifiers() & QtCore.Qt.ControlModifier):
            ctrl = True
        code = event.key()
        key = event.text()
        return (pressed,key,code,shift,ctrl)

    def eventFilter(self, receiver, event):
        # in pyqt5 QWindow gets all events before the widgets inside it do
        # we need the widgets inside it to get their events first
        # this line line provides that.
        # (pyqt4 did not require this)
        if isinstance(receiver, QtGui.QWindow):
            return super(MyEventFilter,self).eventFilter(receiver, event)
        if(event.type() == QtCore.QEvent.KeyPress):
            handled = False
            handled = self.w.keyPressTrap(event)
            if (self.has_key_p_handler):
                handled = self.w.handler_instance.keypress_event__(receiver,event)
            elif self.has_process_key_handler:
                if event.isAutoRepeat():return True
                p,k,c,s,ctrl = self.process_event(event,True)
                handled = self.w.handler_instance.processed_key_event__(receiver,event,p,k,c,s,ctrl)
            if handled: return True
        elif (event.type() == QtCore.QEvent.KeyRelease):
            handled = False
            handled = self.w.keyReleaseTrap(event)
            if (self.has_key_r_handler):
                handled = self.w.handler_instance.keyrelease_event__(event)
            elif self.has_process_key_handler:
                if event.isAutoRepeat():return True
                p,k,c,s,ctrl = self.process_event(event,False)
                handled = self.w.handler_instance.processed_key_event__(receiver,event,p,k,c,s,ctrl)
            if handled: return True
        #Call Base Class Method to Continue Normal Event Processing
        return super(MyEventFilter,self).eventFilter(receiver, event)

class MyWindow(QtWidgets.QMainWindow):
    def __init__(self, halcomp, path):
        super(MyWindow, self).__init__()

        self.filename = path.XML
        self.halcomp = halcomp
        self.has_closing_handler = None
        self.setFocus(True)
        self.PATHS = path
        self.PREFS_ = None

    # These catch events if using a plain VCP panel and there is no handler file
    def keyPressEvent(self, e):
        self.keyPressTrap(e)
    def keyreleaseEvent(self, e):
        self.keyReleaseTrap(e)

    # These can get class patched by xembed library to catch events
    def keyPressTrap(self, e):
        return False
    def keyReleaseTrap(self, e):
        return False

    def shutdown(self):
        if self.has_closing_handler:
            log.debug('Calling handler file Closing_cleanup__ function.')
            self.handler_instance.closing_cleanup__()

    def instance(self):
        try:
            instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            log.critical(e)
            log.critical('Did a widget signal call a missing function name in the handler file?')
            message = 'Did a widget signal call a missing function name in the handler file?\nPython Error:\n'+ str(e)
            rtn = QtWidgets.QMessageBox.critical(None, "QTVCP Error", message)
            sys.exit(0)

        log.debug('QTVCP top instance: {}'.format(self))
        for widget in instance.findChildren(QtCore.QObject):
            log.debug('QTVCP Widget: {}'.format(widget))

    def apply_styles(self, fname = None):
        if self.PATHS.IS_SCREEN:
            DIR = self.PATHS.SCREENDIR
            BNAME = self.PATHS.BASENAME
        else:
            DIR =self.PATHS.PANELDIR
            BNAME = self.PATHS.BASENAME
        # apply one word system theme
        if fname in (QtWidgets.QStyleFactory.keys()):
            QtWidgets.qApp.setStyle(fname)
            return
        
        # apply default qss file or specified file
        if fname is None:
            if self.PATHS.QSS is not None:
                qssname = self.PATHS.QSS
            else:
                return
        elif not os.path.isfile(fname):
            temp = os.path.join(os.path.expanduser(fname))
            qssname = os.path.join(DIR, BNAME,fname+'.qss')
            if not os.path.isfile(fname):
                qssname = temp
        else:
            qssname = fname
        try:
            qss_file = open(qssname).read()
            self.setStyleSheet(qss_file)
            return
        except:
            if fname:
                themes = ''
                log.error('QSS Filepath Error: {}'.format(qssname))
                log.error("{} theme not available".format(fname))
                current_theme = str(QtWidgets.qApp.style().objectName())
                for i in (QtWidgets.QStyleFactory.keys()):
                    themes += (', {}'.format(i))
                log.error('QTvcp Available system themes: green<{}> {}'.format(current_theme, themes))

    def load_extension(self,handlerpath):
        methods,self.handler_module,self.handler_instance = self._load_handlers([handlerpath], self.halcomp,self)
        for i in methods:
            self[i] = methods[i]
        # See if the handler file has a closing function to call first
        self.has_closing_handler = 'closing_cleanup__' in dir(self.handler_instance)

    def _load_handlers(self,usermod,halcomp,widgets):
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
            (directory,filename) = os.path.split(u)
            (basename,extension) = os.path.splitext(filename)
            if directory == '':
                directory = '.'
            if directory not in sys.path:
                sys.path.insert(0,directory)
                log.debug('adding import dir yellow<{}>'.format(directory))

            try:
                mod = __import__(basename)
            except ImportError, e:
                log.critical("module '{}' skipped - import error: ".format(basename), exc_info=e)
                sys.exit(0)
                continue
            log.debug("module '{}' imported green<OK>".format(mod.__name__))

            try:
                # look for 'get_handlers' function
                h = getattr(mod,hdl_func,None)

                if h and callable(h):
                    log.debug("module '{}' : '{}' function found".format(mod.__name__,hdl_func))
                    objlist = h(halcomp,widgets,self.PATHS) # this sets the handler class signature
                else:
                    # the module has no get_handlers() callable.
                    # in this case we permit any callable except class Objects in the module to register as handler
                    log.debug("module '{}': no '{}' function - registering only functions as callbacks"
                        .format(mod.__name__, hdl_func))
                    objlist =  [mod]
                # extract callback candidates
                for object in objlist:
                    log.debug("Registering handlers in module {} object {}".format(mod.__name__, object))
                    if isinstance(object, dict):
                        methods = dict.items()
                    else:
                        methods = map(lambda n: (n, getattr(object, n, None)), dir(object))
                    for method,f in methods:
                        if method.startswith('_'):
                            continue
                        if callable(f):
                            log.debug("Register callback '{}' in {}".format(method, object))
                            add_handler(method, f)
            except Exception as e:
                log.exception("Trouble looking for handlers in '{}':".format(basename), exc_info=e)
                # we require a working handler file!
                sys.exit()

        # Wrap lists in Trampoline, unwrap single functions
        for n,v in list(handlers.items()):
            if len(v) == 1:
                handlers[n] = v[0]
            else:
                handlers[n] = Trampoline(v)

        return handlers,mod,object

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)
