import os,sys
from PyQt4 import QtCore,QtGui, uic
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
        if(event.type() == QtCore.QEvent.KeyPress):
            handled = False
            if (self.has_key_p_handler):
                handled = self.w.handler_instance.keypress_event__(receiver,event)
            elif self.has_process_key_handler:
                if event.isAutoRepeat():return True
                if event.key() in(16777249,16777248): return True
                p,k,c,s,ctrl = self.process_event(event,True)
                handled = self.w.handler_instance.processed_key_event__(receiver,event,p,k,c,s,ctrl)
            if handled: return True
        elif (event.type() == QtCore.QEvent.KeyRelease):
            if (self.has_key_r_handler):
                handled = self.w.handler_instance.keyrelease_event__(event)
            elif self.has_process_key_handler:
                if event.isAutoRepeat():return True
                if event.key() in(16777249,16777248): return True
                p,k,c,s,ctrl = self.process_event(event,False)
                handled = self.w.handler_instance.processed_key_event__(receiver,event,p,k,c,s,ctrl)
            if handled: return True
        #Call Base Class Method to Continue Normal Event Processing
        return super(MyEventFilter,self).eventFilter(receiver, event)

class MyWindow(QtGui.QMainWindow):
    def __init__(self,filename,halcomp):
        super(MyWindow, self).__init__()

        self.filename = filename
        self.halcomp = halcomp
        self.has_closing_handler = None
        self.setFocus(True)

    def closeEvent(self, event):
        if self.has_closing_handler:
            self.handler_instance.closing_cleanup__()

    def instance(self):
        instance = uic.loadUi(self.filename, self)

        log.debug('QTVCP top instance: {}'.format(self))
        for widget in instance.findChildren(QtCore.QObject):
            log.debug('QTVCP Widget: {}'.format(widget))


    def load_extension(self,handlerpath,paths=None):
        methods,self.handler_module,self.handler_instance = self._load_handlers([handlerpath],self.halcomp,self,paths)
        for i in methods:
            self[i] = methods[i]
        # See if the handler file has a closing function to call first
        self.has_closing_handler = 'closing_cleanup__' in dir(self.handler_instance)

    def _load_handlers(self,usermod,halcomp,widgets,paths):
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
                    objlist = h(halcomp,widgets,paths) # this sets the handler class signature
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
#                traceback.print_exc()

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
