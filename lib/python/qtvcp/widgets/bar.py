from PyQt5 import QtWidgets
from PyQt5.QtGui import QColor, QBrush, QPainter
from PyQt5.QtCore import (Qt, pyqtSlot, pyqtProperty, QVariant, QRectF,
    QSize)

try:
    from PyQt5.QtCore import Q_ENUM
except:
    # before Qt 5.10
    from PyQt5.QtCore import Q_ENUMS as Q_ENUM

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
import hal

# Set up logging
from qtvcp import logger
LOG = logger.getLogger(__name__)

# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class Bar(QtWidgets.QWidget):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self._vertical = False
        self._opposite = False
        self._minimum = 0
        self._maximum = 200
        self._value = 100

        self.setSizePolicy(
            QtWidgets.QSizePolicy.MinimumExpanding,
            QtWidgets.QSizePolicy.MinimumExpanding
        )

        # set default number of colored steps
        self.setSteps(None)

        self._bar_solid_percent = 0.9
        self._background_color = QColor('black')
        self._padding = 4.0  # n-pixel gap around edge.

    def paintEvent(self, e):
        painter = QPainter(self)

        brush = QBrush()
        brush.setColor(self._background_color)
        brush.setStyle(Qt.SolidPattern)
        rect = QRectF(0, 0, painter.device().width(), painter.device().height())
        painter.fillRect(rect, brush)

        # Get current state.
        vmin, vmax = self.minimum(), self.maximum()
        value = self.value()

        # Define our canvas.
        d_height = painter.device().height() - (self._padding * 2)
        d_width = painter.device().width() - (self._padding * 2)

        if self._vertical:
            # Draw the vertical bars.
            step_size = d_height / self.n_steps
            bar_height = step_size * self._bar_solid_percent
            bar_spacer = step_size * (1 - self._bar_solid_percent) / 2

            # Calculate the y-stop position, from the value in range.
            pc = (value - vmin) / (vmax - vmin)
            n_steps_to_draw = int(pc * self.n_steps)
            partial = (pc * self.n_steps) % 1
            #print('pre',value,pc, n_steps_to_draw, partial)

            n =-1 # default fall through value if n_steps_to_draw = 0
            # draw complete step bars
            for n in range(n_steps_to_draw):
                brush.setColor(QColor(self._step_color_list[n]))
                if self._opposite:
                    rect = QRectF(
                        self._padding,
                        self._padding + (( + n) * step_size) + bar_spacer,
                        d_width,
                        bar_height
                        )
                else:
                    rect = QRectF(
                        self._padding,
                        self._padding + d_height - ((1 + n) * step_size) + bar_spacer,
                        d_width,
                        bar_height
                        )
                painter.fillRect(rect, brush)

            # draw partial step bar for in-between values
            if partial > 0:
                n +=1
                brush.setColor(QColor(self._step_color_list[n]))
                if self._opposite:
                    rect = QRectF(
                        self._padding,
                        self._padding  + (n * step_size) + bar_spacer,
                        d_width,
                        bar_height * partial
                        )
                else:
                    height = (bar_height* partial)
                    # left,top,width,height
                    rect = QRectF(
                        self._padding,
                        self._padding + d_height - (n * step_size) + bar_spacer - height,
                        d_width,
                        height- bar_spacer
                        )

                painter.fillRect(rect, brush)

        else:
            # Draw the horizontal bars.
            step_size = d_width / self.n_steps
            bar_width = step_size * self._bar_solid_percent
            bar_spacer = step_size * (1 - self._bar_solid_percent) / 2

            # Calculate the y-stop position, from the value in range.
            pc = (value - vmin) / (vmax - vmin)
            n_steps_to_draw = int(pc * self.n_steps)
            partial = (pc * self.n_steps) % 1
            #print('pre',value,pc, n_steps_to_draw, partial)

            n=-1 # default fall through value if n_steps_to_draw = 0
            for n in range(n_steps_to_draw):
                brush.setColor(QColor(self._step_color_list[n]))
                # left, top, width and height floats
                if self._opposite:
                    left = self._padding + d_width - ((1 + n) * step_size) + bar_spacer
                else:
                    left = self._padding + bar_spacer + ((0 + n) * step_size)
                rect = QRectF(
                    left ,
                    self._padding ,
                    bar_width,
                    d_height
                )
                painter.fillRect(rect, brush)

            # draw partial step bar for in-between values
            if partial > 0:
                n+=1
                brush.setColor(QColor(self._step_color_list[n]))
                # right to left
                if self._opposite:
                    width = (bar_width* partial)
                    left = self._padding - (n * step_size) + d_width - width
                    rect = QRectF(
                                left ,
                                self._padding ,
                                width - bar_spacer,
                                d_height
                                )
                # left to right
                else:
                    left = self._padding + bar_spacer + (n * step_size)
                    rect = QRectF(
                                left,
                                self._padding ,
                                bar_width * partial,
                                d_height
                                )
                painter.fillRect(rect, brush)

        painter.end()

    # list of color strings or an integer of steps
    def setSteps(self, steps):
        if steps is None:
            steps = ['green', 'green', '#00b600', '#00b600', '#00d600',
                     '#00d600', 'yellow', 'yellow', 'red', 'red']
            self.n_steps = len(steps)
            self._step_color_list = steps

        if isinstance(steps, list):
            # list of colors.
            self.n_steps = len(steps)
            self._step_color_list = steps

        elif isinstance(steps, int):
            # int number of bars, defaults to red.
            self.n_steps = steps
            self._step_color_list = ['red'] * steps
        else:
            raise TypeError('steps must be a list or int')

    def value(self):
        return self._value

    @pyqtSlot(float)
    @pyqtSlot(int)
    def setValue(self, data):
        if data != self._value:
            if data >  self._maximum: data = self._maximum
            if data <  self._minimum : data =  self._minimum
            self._value = data
            self.update()

    def sizeHint(self):
        if self._vertical:
            return QSize(40, 120)
        else:
            return QSize(40, 20)

    def minimum(self):
        return self._minimum

    def maximum(self):
        return self._maximum

    def setRange(self, low, hi):
        self._minimum = low
        self._maximum = hi

    def setFormat(self, data):
        pass

    def getInvertedAppearance(self):
        return self._opposite
    def setInvertedAppearance(self, data):
        self._opposite = bool(data)
        self.update()
    def resetInvertedAppearance(self, data):
        self._opposite = False
        self.update()

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    ########################################################################

    def set_step_color_l(self, data):
        # convert stylesheet list of single string to list of multiple
        if ',' in data[0]:
            self.setSteps(data[0].split(','))
        else:
            self.setSteps(data)
    def get_step_color_l(self):
        return self._step_color_list
    def reset_step_color_l(self):
        self.setSteps( ['green', 'green', '#00b600', '#00b600',
                 '#00d600', '#00d600', 'yellow', 'yellow', 'red', 'red'])

    def getBackgroundColor(self):
        return self._background_color

    @pyqtSlot(QColor)
    def setBackgroundColor(self, value):
        self._background_color = value
        self.update()

    def getMax(self):
        return self._maximum
    def setMax(self, data):
        self._maximum = data
    def resetMax(self):
        self._maximum = 200

    def getMin(self):
        return self._minimum
    def setMin(self, data):
        self._minimum = data
    def resetMin(self):
        self._minimum = 0

    def getVertical(self):
        return self._vertical
    def setVertical(self, data):
        self._vertical = data
        self.update()
    def resetVertical(self):
        self._vertical = False
        self.update()

    stepColorList = pyqtProperty(
                        QVariant.typeToName(QVariant.StringList),
                         get_step_color_l, set_step_color_l, reset_step_color_l)

    backgroundColor = pyqtProperty(QColor, getBackgroundColor, setBackgroundColor)
    setMaximum = pyqtProperty(int, getMax, setMax, resetMax)
    setMinimum = pyqtProperty(int, getMin, setMin, resetMin)
    setVertical = pyqtProperty(bool, getVertical, setVertical, resetVertical)
    setInverted = pyqtProperty(bool, getInvertedAppearance, setInvertedAppearance, resetInvertedAppearance)

class HALPinType:
    NONE = 0
    S32 = hal.HAL_S32
    FLOAT = hal.HAL_FLOAT


class  HalBar(Bar, _HalWidgetBase):
    HALPinType = HALPinType
    Q_ENUM(HALPinType)

    # older version of pyqt5 need this as well as Q_ENUM
    NONE = 0
    S32 = hal.HAL_S32
    FLOAT = hal.HAL_FLOAT

    def __init__(self, *args, **kwargs):
        super(). __init__( *args, **kwargs)
        self._pin_type = HALPinType.S32
        self._pin_name = ''

    def _hal_init(self):
        if self._pin_name == '':
            pname = self.HAL_NAME_
        else:
            pname = self._pin_name

        if self._pin_type == HALPinType.FLOAT:
            self.hal_pin = self.HAL_GCOMP_.newpin(pname, hal.HAL_FLOAT, hal.HAL_IN)
            self.hal_pin.value_changed.connect(lambda data: self.updateDisplay(data))
        elif self._pin_type == HALPinType.S32:
            self.hal_pin = self.HAL_GCOMP_.newpin(pname, hal.HAL_S32, hal.HAL_IN)
            self.hal_pin.value_changed.connect(lambda data: self.updateDisplay(data))

    def updateDisplay(self, data):
        self.setValue(data)

    def set_pin_type(self, value):
        self._pin_type = value
    def get_pin_type(self):
        return self._pin_type
    def reset_pin_type(self):
        self._pin_type = HALPinType.S32

    def set_pin_name(self, value):
        self._pin_name = value
    def get_pin_name(self):
        return self._pin_name
    def reset_pin_name(self):
        self._pin_name = ''

    pinType = pyqtProperty(HALPinType, get_pin_type, set_pin_type, reset_pin_type)
    pinName = pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)

if __name__ == '__main__':
    app = QtWidgets.QApplication([])
    bar = HalBar()
    bar.setProperty('setInverted',True)
    bar.setProperty('setVertical',True)
    bar.setValue(51)
    bar.show()
    app.exec_()

