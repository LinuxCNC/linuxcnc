from PyQt5 import QtWidgets
from PyQt5.QtGui import QColor, QBrush, QPainter, QLinearGradient
from PyQt5.QtCore import (Qt, pyqtSlot, pyqtProperty, pyqtSignal, QVariant, QRectF,
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
    valueChanged = pyqtSignal([int],[float])

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
        self._padding = 2.0  # n-pixel gap around edge.
        self._split = .2 # percent of full scale indicator 
        self._useMultiColor = True
        self._singleIndicatorColor = QColor('blue')

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
            for n in range(self.n_steps):
                brush.setColor(QColor(self._step_color_list[n]))
                if self._opposite:
                    top = self._padding + (( + n) * step_size) + bar_spacer
                else:
                    top = self._padding + d_height - ((1 + n) * step_size) + bar_spacer

                # full scale indicator:
                # QRextF(left, top, width and height )
                rect = QRectF(
                    self._padding,
                    top,
                    d_width * self._split,
                    bar_height
                    )
                painter.fillRect(rect, brush)

                # value indicator:
                if n < n_steps_to_draw:
                    rect2 = QRectF(
                        self._padding + (d_width * self._split),
                        top ,
                        d_width  * (1-self._split),
                        bar_height
                        )
                    if self._useMultiColor:
                        color = QColor(self._step_color_list[n])
                    else:
                        color = self._singleIndicatorColor
                    gradient = QLinearGradient(0, 0, 1, 0)
                    gradient.setColorAt(0.0, color)
                    gradient.setColorAt(0.5, color.lighter())
                    gradient.setColorAt(1.0, color)
                    gradient.setCoordinateMode(gradient.ObjectMode)
                    painter.fillRect(rect2, QBrush(gradient))

            # draw partial step bar for in-between values
            if partial > 0:
                n = n_steps_to_draw
                brush.setColor(QColor(self._step_color_list[n]))
                # right to left
                height = (bar_height* partial)
                if self._opposite:
                    top = self._padding  + (n * step_size) + bar_spacer
                else:
                    top = self._padding - (n * step_size) + d_height - height- bar_spacer

                # full scale indicator
                rect = QRectF(
                    self._padding,
                    top,
                    d_width * self._split,
                    height
                    )
                painter.fillRect(rect, brush)

                # value indicator
                rect2 = QRectF(
                    self._padding + d_width * self._split,
                    top ,
                    d_width *(1- self._split),
                    height
                    )

                if self._useMultiColor:
                    color = QColor(self._step_color_list[n])
                else:
                    color = self._singleIndicatorColor

                gradient = QLinearGradient(0, 0, 1, 0)
                gradient.setColorAt(0.0, color)
                gradient.setColorAt(0.5, color.lighter())
                gradient.setColorAt(1.0, color)
                gradient.setCoordinateMode(gradient.ObjectMode)
                painter.fillRect(rect2, QBrush(gradient))


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
            for n in range(self.n_steps):
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
                    d_height * self._split
                )
                painter.fillRect(rect, brush)

                if n < n_steps_to_draw:
                    rect2 = QRectF(
                        left ,
                        self._padding + d_height * self._split,
                        bar_width,
                        d_height * (1-self._split)
                    )

                    if self._useMultiColor:
                        color = QColor(self._step_color_list[n])
                    else:
                        color = self._singleIndicatorColor

                    gradient = QLinearGradient(0, 1, 0, 0)
                    gradient.setColorAt(0.0, color)
                    gradient.setColorAt(0.5, color.lighter())
                    gradient.setColorAt(1.0, color)
                    gradient.setCoordinateMode(gradient.ObjectMode)
                    painter.fillRect(rect2, QBrush(gradient))

            # draw partial step bar for in-between values
            if partial > 0:
                n = n_steps_to_draw
                brush.setColor(QColor(self._step_color_list[n]))

                # right to left
                width = (bar_width* partial)
                if self._opposite:
                    left = self._padding - (n * step_size) + d_width - width - bar_spacer
                # left to right
                else:
                    left = self._padding + bar_spacer + (n * step_size)

                # full scale indicator
                rect = QRectF(
                            left,
                            self._padding ,
                            width,
                            d_height * self._split
                            )
                painter.fillRect(rect, brush)

                # value indicator
                rect2 = QRectF(
                    left ,
                    self._padding + d_height * self._split,
                    width,
                    d_height * (1-self._split)
                )
                if self._useMultiColor:
                    color = QColor(self._step_color_list[n])
                else:
                    color = self._singleIndicatorColor
                gradient = QLinearGradient(0, 1, 0, 0)
                gradient.setColorAt(0.0, color)
                gradient.setColorAt(0.5, color.lighter())
                gradient.setColorAt(1.0, color)
                gradient.setCoordinateMode(gradient.ObjectMode)
                painter.fillRect(rect2, QBrush(gradient))

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
            self.valueChanged.emit(int(data))
            self.valueChanged[float].emit(float(data))
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

    def invertBool(self, value):
        if value:
            return False
        return True

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

    def get_indicatorColor(self):
        return self._singleIndicatorColor
    @pyqtSlot(QColor)
    def set_indicatorColor(self, value):
        self._singleIndicatorColor = value
        self.update()
    def reset_indicatorColor(self, value):
        self._singleIndicatorColor = QColor('blue')
        self.update()

    def getSplit(self):
        return int(self._split * 100)
    def setSplit(self, data):
        if data <0: data == 0
        elif data >100: data == 100
        self._split = data/100
        self.update()
    def resetSplit(self):
        self._split = .2
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

    def getMultiColor(self):
        return self._useMultiColor
    def setMultiColor(self, data):
        self._useMultiColor = data
        self.update()
    def resetMultiColor(self):
        self._useMultiColor = False
        self.update()

    def getVert(self):
        return self._vertical
    def setVert(self, data):
        self._vertical = data
        self.update()
    def resetVert(self):
        self._vertical = False
        self.update()

    stepColorList = pyqtProperty(
                        QVariant.typeToName(QVariant.StringList),
                         get_step_color_l, set_step_color_l, reset_step_color_l)

    backgroundColor = pyqtProperty(QColor, getBackgroundColor, setBackgroundColor)
    indicatorColor = pyqtProperty(QColor, get_indicatorColor, set_indicatorColor, reset_indicatorColor)
    useMultiColorIndicator = pyqtProperty(bool, getMultiColor, setMultiColor, resetMultiColor)
    split = pyqtProperty(int, getSplit, setSplit, resetSplit)
    setMaximum = pyqtProperty(int, getMax, setMax, resetMax)
    setMinimum = pyqtProperty(int, getMin, setMin, resetMin)
    setVertical = pyqtProperty(bool, getVert, setVert, resetVert)
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
        self._invert_negative = False
        self._superOpposite = self._opposite

    def _hal_init(self):
        if self._pin_name == '':
            pname = self.HAL_NAME_
        else:
            pname = self._pin_name

        self._superOpposite = self._opposite

        if self._pin_type == HALPinType.FLOAT:
            self.hal_pin = self.HAL_GCOMP_.newpin(pname, hal.HAL_FLOAT, hal.HAL_IN)
            self.hal_pin.value_changed.connect(lambda data: self.updateDisplay(data))
        elif self._pin_type == HALPinType.S32:
            self.hal_pin = self.HAL_GCOMP_.newpin(pname, hal.HAL_S32, hal.HAL_IN)
            self.hal_pin.value_changed.connect(lambda data: self.updateDisplay(data))

    def updateDisplay(self, data):
        if data < 0 and self._invert_negative:
            self._opposite = self.invertBool(self._superOpposite)
            data = data * -1
        else:
            self._opposite = self._superOpposite
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

    def set_invert_negative(self, value):
        self._invert_negative = value
    def get_invert_negative(self):
        return self._invert_negative
    def reset_invert_negative(self):
        self._invert_negative = False

    pinType = pyqtProperty(HALPinType, get_pin_type, set_pin_type, reset_pin_type)
    pinName = pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)
    invertOnNegative = pyqtProperty(bool, get_invert_negative, set_invert_negative, reset_invert_negative)

if __name__ == '__main__':
    from PyQt5.QtWidgets import (QLabel, QSlider, QWidget, QVBoxLayout,
        QHBoxLayout, QPushButton, QCheckBox)

    app = QtWidgets.QApplication([])
    w = QWidget()
    w.setGeometry(100, 100, 200, 100)
    w.setWindowTitle('Bar widget')
    layout = QVBoxLayout(w)

    label = QLabel()

    bar = HalBar()
    bar.setProperty('setInverted',True)
    bar.setProperty('setVertical',False)

    #bar.setValue(51)

    slider = QSlider(Qt.Horizontal)
    slider.setMinimum(0)
    slider.setMaximum(200)
    slider.setSingleStep(10)
    slider.setPageStep(100)
    slider.valueChanged.connect(bar.setValue)
    slider.valueChanged.connect(label.setNum)
    slider.setValue(51)

    layout.addWidget(label)
    layout.addWidget(bar)
    layout.addWidget(slider)

    button = QPushButton('Vertical')
    button.setCheckable(True)
    button.setChecked(bar.getVert())
    button.toggled.connect(bar.setVert)

    layout.addWidget(button)

    button = QPushButton('Use Multi Color')
    button.setCheckable(True)
    button.setChecked(bar.getMultiColor())
    button.toggled.connect(bar.setMultiColor)

    layout.addWidget(button)

    lyt = QHBoxLayout()
    check1 = QCheckBox('o%')
    check1.setAutoExclusive (True)
    check1.stateChanged.connect(lambda value: bar.setSplit(0))

    check2 = QCheckBox('20%')
    check2.setAutoExclusive (True)
    check2.stateChanged.connect(lambda value: bar.setSplit(20))

    check3 = QCheckBox('50%')
    check3.setAutoExclusive (True)
    check3.stateChanged.connect(lambda value: bar.setSplit(50))

    lyt.addWidget(check1)
    lyt.addWidget(check2)
    lyt.addWidget(check3)
    layout.addLayout(lyt)

    w.show()
    app.exec_()

