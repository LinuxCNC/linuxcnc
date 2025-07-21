import hal
import _hal
import traceback
from PyQt5.QtCore import QObject, QTimer, pyqtSignal

# Set up logging
from common import logger
log = logger.getLogger(__name__)
# log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL, VERBOSE

class QPin(hal.Pin, QObject):

    value_changed = pyqtSignal('PyQt_PyObject')# depreciated
    pinValueChanged = pyqtSignal('PyQt_PyObject','PyQt_PyObject')
    isDrivenChanged = pyqtSignal('PyQt_PyObject','PyQt_PyObject')
    REGISTRY = []
    UPDATE = False

    def __init__(self, *a, **kw):
        super(QPin, self).__init__(*a, **kw)
        QObject.__init__(self, None)
        self.hal = hal
        self._item_wrap(self._item)
        self._prev = None
        self._prevDriven = None
        self.REGISTRY.append(self)
        self.prefix = None

    def update(self):
        tmp = self.get()
        if tmp != self._prev:
            self.value_changed.emit(tmp)
            self.pinValueChanged.emit(self, tmp)
        self._prev = tmp

    def isDriven(self):
        return hal.pin_has_writer('{}.{}'.format(self.prefix, self.get_name()))

    def updateDriven(self):
        tmp = self.isDriven()
        if tmp != self._prevDriven:
            self.isDrivenChanged.emit(self, tmp)
        self._prevDriven = tmp

    def text(self):
        return self.get_name()

    # always returns False because
    # there was no error when making pin
    # see class DUMMY
    def error(self):
        return False

    @classmethod
    def update_all(cls):
        if not cls.UPDATE:
            return
        kill = []
        for p in cls.REGISTRY:
            try:
                p.update()
            except Exception as e:
                kill.append(p)
                log.error("Error updating pin {}; Removing".format(p))
                log.exception(e)
            if p.receivers(p.isDrivenChanged) > 0:
                p.updateDriven()

        for p in kill:
            cls.REGISTRY.remove(p)
        return cls.UPDATE

    @classmethod
    def update_start(cls, cycletime=None):
        if QPin.UPDATE:
            return
        QPin.UPDATE = True
        cls.timer = QTimer()
        cls.timer.timeout.connect(cls.update_all)
        cls.timer.start(cycletime)

    @classmethod
    def update_stop(cls):
        QPin.UPDATE = False


# so errors when making QPins aren't fatal
class DummyPin(QObject):
    value_changed = pyqtSignal('PyQt_PyObject')

    def __init__(self, *a, **kw):
        super(DummyPin, self).__init__(None)
        self._a = a
        self._kw = kw

    # always returns True because
    # there was an error when making HAL pin
    # see class QPin
    def error(self):
        return True

    def getError(self):
        print('{}'.format(self._kw.get('ERROR')))

    # send a safe value
    def get(self):
        return 0

    # accept but don't set anything
    def set(self, *a, **kw):
        pass

    def get_name(self):
        return self._a[0]

class _QHal(object):
    HAL_BIT = hal.HAL_BIT
    HAL_FLOAT = hal.HAL_FLOAT
    HAL_S32 = hal.HAL_S32
    HAL_U32 = hal.HAL_U32

    HAL_IN = hal.HAL_IN
    HAL_OUT = hal.HAL_OUT
    HAL_IO = hal.HAL_IO
    HAL_RO = hal.HAL_RO
    HAL_RW = hal.HAL_RW

    def __new__(cls, *a, **kw):
        instance = super(_QHal, cls).__new__(cls)
        instance.__init__(*a, **kw)
        return instance

    def __init__(self, comp=None, hal=None):
        # only initialize once for all instances
        if not self.__class__._instance is None:
            return

        if isinstance(comp, _QHal):
            comp = comp.comp
        self.comp = comp
        self.hal = hal

    def setUpdateRate(self, cyclerate):
        QPin.update_start(cyclerate)

    def newpin(self, *a, **kw):
        return self.newPin(*a,**kw)
    def newPin(self, *a, **kw):
        try:
            p = QPin(_hal.component.newpin(self.comp, *a, **kw))
        except ValueError as e:
            # if pin is already made, find a new name
            if 'Duplicate pin name' in '{}'.format(e):
                try:
                    # tuples are immutable, convert to list
                    y = list(a)
                    y[0] = self.makeUniqueName(y[0])
                    a = tuple(y)
                    # this late in the game, component is probably already 'ready'
                    if self.hal.component_is_ready(self.comp.getprefix()):
                        self.comp.unready()
                        p = QPin(_hal.component.newpin(self.comp, *a, **kw))
                        self.comp.ready()
                    else:
                        p = QPin(_hal.component.newpin(self.comp, *a, **kw))
                except Exception as e:
                    raise
        except Exception as e:
            if log.getEffectiveLevel() == logger.VERBOSE:
                raise
            t = inspect.getframeinfo(inspect.currentframe().f_back)
            log.error("QHal: Error making new HAL pin: {}\n    {}\n    Line {}\n    Function: {}".
                format(e, t[0], t[1], t[2]))
            log.error("QHal: {}".format(traceback.format_exc()))
            p = DummyPin(*a, ERROR=e)
        p.prefix = self.comp.getprefix()
        return p

    # accept either, first is depreciated
    def getpin(self, *a, **kw):
        return self.getPinObject(*a, **kw)
    def getPinObject(self, *a, **kw):
        return QPin(_hal.component.getpin(self.comp, *a, **kw))

    # accept either, first is depreciated
    def getvalue(self, name):
        return self.getValue(name)
    def getValue(self, name):
        try:
            return hal.get_value(name)
        except Exception as e:
            raise("QHal: Error getting value of {}\n {}".format(name, e))

    # accept either, first is depreciated
    def setp(self,name, value):
        return self.setPin(name, value)
    def setPin(self,name, value):
        try:
            return hal.set_p(name,value)
        except Exception as e:
            raise("QHal: Error setting pin {} to {}\n {}".format(name,value, e))

    # accept either, first is depreciated
    def sets(self,name, value):
        return self.setSignal(name, value)
    def setSignal(self,name, value):
        try:
            return hal.set_s(name,value)
        except Exception as e:
            raise("QHal: Error setting signal {} to {}\n {}".format(name,value, e))

    def exit(self, *a, **kw):
        return self.comp.exit(*a, **kw)

    # find a unique HAL pin name by adding '-x' to the base name
    # x being an ever increasing number till name is unique
    def makeUniqueName(self, name):
        num = 2
        base = self.comp.getprefix()
        while True:
            trial = ('{}.{}-{}').format(base,name,num)
            for i in hal.get_info_pins():
                if i['NAME'] == trial:
                    num +=1
                    trial = ('{}.{}-{}').format(base,name,num)
                    break
            else:
                break
        return ('{}-{}').format(name,num)

    def __getitem__(self, k): return self.comp[k]
    def __setitem__(self, k, v): self.comp[k] = v

class Qhal(_QHal):
    _instance = None

    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            cls._instance = _QHal.__new__(cls, *args, **kwargs)
        return cls._instance
