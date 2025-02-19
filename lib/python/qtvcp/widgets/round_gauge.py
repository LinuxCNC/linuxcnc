#!/usr/bin/env python3
import sys
import math
from PyQt5 import QtCore, QtWidgets
from PyQt5.QtCore import Qt, QPoint, QPointF, QLine, QRect, QSize, pyqtSlot, pyqtProperty
from PyQt5.QtGui import QPainter, QBrush, QPen, QFont, QColor, QRadialGradient
from qtvcp.widgets.widget_baseclass import _HalWidgetBase, hal

class Gauge(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Gauge, self).__init__(parent)
        self._threshold = 0
        self._setpoint = QPointF(0, 0)
        self._num_ticks = 11
        self._max_value = 100
        self._max_reading = 100
        self._gauge_label = "GAUGE"
        self._dial_font_size = 10
        self._value_font_size = 10
        self._label_font_size = 10
        self._base_gradient_color = QColor(180, 180, 180)
        self._base_color = QColor(40, 40, 40)
        self._center_gradient_color = QColor("gray")
        self._center_color = QColor("#404040")
        self._zone1_color = QColor("green")
        self._zone2_color = QColor("red")
        self._bezel_color = QColor("gray")
        self._bezel_width = 6
        self.qpa = list()
        self.canvas = QRect()
        self.canvas.setSize(QSize(self.width(), self.height()))
        self.tick_width = 2
        self.arc_width = 10
        self.value = 0
        self._halpin_option = True
        self.create_unit_array()

    def _hal_init(self):
        if (self._halpin_option):
            self.hal_pin = self.HAL_GCOMP_.newpin(self.HAL_NAME_ + "_value", hal.HAL_FLOAT, hal.HAL_IN)
            self.hal_pin.value_changed.connect(lambda value: self.update_value(value))
            self.hal_pin = self.HAL_GCOMP_.newpin(self.HAL_NAME_ + "_setpoint", hal.HAL_FLOAT, hal.HAL_IN)
            self.hal_pin.value_changed.connect(lambda value: self.set_setpoint(value))

    def create_unit_array(self):
        self.qpa = list(range(self._num_ticks))
        inc = 270.0 / (self._num_ticks - 1)
        for i in range(self._num_ticks):
            angle = (inc * i) - 225
            x = math.cos(math.radians(angle))
            y = math.sin(math.radians(angle))
            self.qpa[i] = QPointF(x, y)

    def paintEvent(self, event):
        w = int(min(event.rect().width(), event.rect().height()))
        painter = QPainter(self)
        painter.setRenderHint(painter.Antialiasing)
        self.draw_background(painter, event, w)
        self.draw_zones(painter, event, w)
        self.draw_gauge(painter, event, w)
        self.draw_digits(painter, event, w)
        self.draw_setpoint(painter, event, w)
        self.draw_needle(painter, event, w)
        self.draw_center(painter, event, w)
        self.draw_readout(painter, event, w)
        painter.end()

    def draw_background(self, qp, event, w):
        w -= 6
        center = event.rect().center()
        rect = QRect()
        rect.setSize(QSize(w, w))
        rect.moveCenter(center)
        fp = QPoint(int(center.x() - w/4), int(center.y() - w/4))
        bg = QRadialGradient(center, w/2, fp)
        bg.setColorAt(0, self._base_gradient_color)
        bg.setColorAt(1, self._base_color)
        qp.setPen(QPen(self._bezel_color, self._bezel_width))
        qp.setBrush(QBrush(bg))
        qp.drawEllipse(rect)
        qp.drawArc(rect, 0, 360 * 16)

    def draw_zones(self, qp, event, w):
        segment1 = -45
        span1 = int(((self._max_value - self._threshold) * 270) / self._max_value)
        segment2 = span1 - 45
        span2 = 270 - span1
        rect = QRect()
        w = int(w/2)
        rect.setSize(QSize(w, w))
        rect.moveCenter(event.rect().center())
        qp.setPen(QPen(self._zone1_color, self.arc_width, cap = Qt.FlatCap))
        qp.drawArc(rect, segment1*16, span1*16)
        qp.setPen(QPen(self._zone2_color, self.arc_width, cap = Qt.FlatCap))
        qp.drawArc(rect, segment2*16, span2*16)

    def draw_gauge(self, qp, event, w):
        w *= 0.6
        w =int(w)
        rect = QRect()
        rect.setSize(QSize(w, w))
        rect.moveCenter(event.rect().center())
        center = rect.center()
        qp.setPen(QPen(Qt.white, self.tick_width, cap = Qt.FlatCap))
        qp.drawArc(rect, (-45 * 16), (270 * 16))
        rad = rect.width()/2
        inc = 270.0 / (self._num_ticks - 1)
        for i in range(self._num_ticks):
            p1 = self.qpa[i] * rad
            p1 += center
            p2 = self.qpa[i] * (rad + 10)
            p2 += center
            line = QLine(p1.toPoint(), p2.toPoint())
            qp.drawLine(line)

    def draw_digits(self, qp, event, w):
        w *= 0.8
        center = event.rect().center()
        rect = QRect()
        rect.setSize(QSize(40,18))
        qp.setPen(QPen(Qt.white, self.tick_width))
        qp.setFont(QFont('Lato Heavy', self._dial_font_size))
        rad = w/2
        inc = 270.0 / (self._num_ticks - 1)
        for i in range(self._num_ticks):
            angle = (inc * i) - 225
            q, r = divmod(self._max_reading * i, self._num_ticks - 1)
            text = str(q) if r == 0 else ""
            if text == "": continue
            x = int(rad * math.cos(math.radians(angle)) + center.x())
            y = int(rad * math.sin(math.radians(angle)) + center.y())
            rect.moveCenter(QPoint(x, y))
            qp.drawText(rect, Qt.AlignCenter, text)

    def draw_center(self, qp, event, w):
        w *= 0.2
        w = int(w)
        rect = QRect()
        rect.setSize(QSize(w, w))
        rect.moveCenter(event.rect().center())
        rad = rect.width()/2
        cap = QRadialGradient(rect.center(), rad)
        cap.setColorAt(0, self._center_gradient_color)
        cap.setColorAt(1, self._center_color)
        qp.setPen(QPen(self._bezel_color, 1.5))
        qp.setBrush(QBrush(cap))
        qp.drawEllipse(rect)

    def draw_setpoint(self, qp, event, w):
        w *= 0.6
        center = event.rect().center()
        rad = w/2
        rect = QRect(0, 0, 8, 8)
        p = self._setpoint * rad
        p += center
        rect.moveCenter(p.toPoint())
        qp.fillRect(rect, QColor(Qt.yellow))

    def draw_needle(self, qp, event, w):
        w *= 0.6
        center = event.rect().center()
        angle = ((self.value * 270.0) / self._max_value) - 225
        rad = w/2
        x = int(rad * math.cos(math.radians(angle)) + center.x())
        y = int(rad * math.sin(math.radians(angle)) + center.y())
        line = QLine(center.x(), center.y(), x, y)
        qp.setPen(QPen(Qt.red, 4))
        qp.drawLine(line)

    def draw_readout(self, qp, event, w):
        center = event.rect().center()
        rect = QRect()
        rect.setSize(QSize(int(w/4), int(w/8)))
        rect.moveCenter(QPoint(center.x(), center.y() + int(w/4)))
        text = "{}".format(self.value)
        qp.setPen(QPen(Qt.white, 4))
        qp.setFont(QFont('Lato Heavy', self._value_font_size))
        qp.drawText(rect, Qt.AlignCenter, text)
        rect.moveCenter(QPoint(center.x(), center.y() + int(w/3)))
        text = self._gauge_label
        qp.setFont(QFont('Lato Heavy', self._label_font_size))
        qp.drawText(rect, Qt.AlignCenter, text)

    @pyqtSlot(float)
    @pyqtSlot(int)
    def update_value(self, value):
        if value != self.value:
            self.value = int(value)
            self.update()

    def set_setpoint(self, value):
        if value >= self._max_value:
            angle = 45
        elif value <= 0:
            angle = -225
        else:
            angle = ((value * 270) / self._max_value) - 225
        x = math.cos(math.radians(angle))
        y = math.sin(math.radians(angle))
        self._setpoint = QPointF(x, y)
        self.update()

    @pyqtSlot(int)
    def set_threshold(self, value):
        if value > self._max_value:
            self._threshold = self._max_value
        elif value < 0:
            self._threshold = 0
        else:
            self._threshold = value
        self.update()

    def get_threshold(self):
        return self._threshold

    def reset_threshold(self):
        self._threshold = 50

    @pyqtSlot(int)
    def set_max_value(self, value):
        self._max_value = value

    def get_max_value(self):
        return self._max_value

    def reset_max_value(self):
        self._max_value = 100

    @pyqtSlot(int)
    def set_max_reading(self, value):
        self._max_reading = value
        self.update()

    def get_max_reading(self):
        return self._max_reading

    def reset_max_reading(self):
        self._max_reading = 100

    @pyqtSlot(int)
    def set_num_ticks(self, value):
        # cannot allow value <= 1 or there will be division by 0 errors
        self._num_ticks = value if value > 1 else 2
        self.create_unit_array()
        self.update()

    def get_num_ticks(self):
        return self._num_ticks

    def reset_num_ticks(self):
        self._num_ticks = 11

    @pyqtSlot(str)
    def set_label(self, label):
        self._gauge_label = label
        self.update()

    def get_label(self):
        return self._gauge_label

    def reset_label(self):
        self._gauge_label = "GAUGE"
        
    @pyqtSlot(QColor)
    def set_base_gradient_color(self, color):
        self._base_gradient_color = color
        self.update()

    def get_base_gradient_color(self):
        return self._base_gradient_color

    def reset_base_gradient_color(self):
        self._base_gradient_color = QColor(180, 180, 180)
        self.update()

    @pyqtSlot(QColor)
    def set_base_color(self, color):
        self._base_color = color
        self.update()

    def get_base_color(self):
        return self._base_color

    def reset_base_color(self):
        self._base_color = QColor(40, 40, 40)        
        self.update()

    @pyqtSlot(QColor)
    def set_center_gradient_color(self, color):
        self._center_gradient_color = color
        self.update()

    def get_center_gradient_color(self):
        return self._center_gradient_color

    def reset_center_gradient_color(self):
        self._center_gradient_color = QColor("gray")
        self.update()

    @pyqtSlot(QColor)
    def set_center_color(self, color):
        self._center_color = color
        self.update()

    def get_center_color(self):
        return self._center_color

    def reset_center_color(self):
        self._center_color = QColor("#404040")        
        self.update()

    @pyqtSlot(QColor)
    def set_zone2_color(self, color):
        self._zone2_color = color
        self.update()

    def get_zone2_color(self):
        return self._zone2_color

    def reset_zone2_color(self):
        self._zone2_color = QColor("red")        

    @pyqtSlot(QColor)
    def set_zone1_color(self, color):
        self._zone1_color = color
        self.update()

    def get_zone1_color(self):
        return self._zone1_color

    def reset_zone1_color(self):
        self._zone1_color = QColor("green")

    @pyqtSlot(QColor)
    def set_zone2_color(self, color):
        self._zone2_color = color
        self.update()

    def get_zone2_color(self):
        return self._zone2_color

    def reset_zone2_color(self):
        self._zone2_color = QColor("red")

    @pyqtSlot(QColor)
    def set_bezel_color(self, color):
        self._bezel_color = color
        self.update()

    def get_bezel_color(self):
        return self._bezel_color

    def reset_bezel_color(self):
        self._bezel_color = QColor("gray")

    def sizeHint(self):
        return QtCore.QSize(200, 200)

    def set_halpin_option(self, value):
        self._halpin_option = value

    def get_halpin_option(self):
        return self._halpin_option

    def reset_halpin_option(self):
        self._halpin_option = True
        
    @pyqtSlot(int)
    def set_bezel_width(self, value):
        self._bezel_width = value
        self.update()

    def get_bezel_width(self):
        return self._bezel_width

    def reset_bezel_width(self):
        self._bezel_width = 6
        self.update()

    def sizeHint(self):
        return QtCore.QSize(200, 200)

    def set_halpin_option(self, value):
        self._halpin_option = value

    def get_halpin_option(self):
        return self._halpin_option

    def reset_halpin_option(self):
        self._halpin_option = True        

    halpin_option = pyqtProperty(bool, get_halpin_option, set_halpin_option, reset_halpin_option)
    threshold = pyqtProperty(int, get_threshold, set_threshold, reset_threshold)
    max_value = pyqtProperty(int, get_max_value, set_max_value, reset_max_value)
    max_reading = pyqtProperty(int, get_max_reading, set_max_reading, reset_max_reading)
    num_ticks = pyqtProperty(int, get_num_ticks, set_num_ticks, reset_num_ticks)
    gauge_label = pyqtProperty(str, get_label, set_label, reset_label)
    base_gradient_color = pyqtProperty(QColor, get_base_gradient_color, set_base_gradient_color, reset_base_gradient_color)
    base_color = pyqtProperty(QColor, get_base_color, set_base_color, reset_base_color)
    center_gradient_color = pyqtProperty(QColor, get_center_gradient_color, set_center_gradient_color, reset_center_gradient_color)
    center_color = pyqtProperty(QColor, get_center_color, set_center_color, reset_center_color)
    zone1_color = pyqtProperty(QColor, get_zone1_color, set_zone1_color, reset_zone1_color)
    zone2_color = pyqtProperty(QColor, get_zone2_color, set_zone2_color, reset_zone2_color)
    bezel_color = pyqtProperty(QColor, get_bezel_color, set_bezel_color, reset_bezel_color)
    bezel_width = pyqtProperty(int, get_bezel_width, set_bezel_width, reset_bezel_width)

    #############################
    # Testing                   #
    #############################
if __name__ == "__main__":
    import sys
    from PyQt5.QtWidgets import QSlider, QWidget, QVBoxLayout
    app = QtWidgets.QApplication(sys.argv)
    w = QWidget()
    w.setGeometry(100, 100, 400, 400)
    w.setWindowTitle('Round Gauge')
    layout = QVBoxLayout(w)
    gauge = Gauge(w)
    gauge.set_max_value(20000)
    gauge.set_max_reading(20)
    gauge.set_threshold(7200)
    gauge.set_setpoint(14000)
    gauge.set_num_ticks(11)
    gauge.set_label("RPM")
    gauge._value_font_size = 10
    gauge._label_font_size = 10
    gauge._dial_font_size = 10
    slider = QSlider(Qt.Horizontal)
    slider.setMinimum(0)
    slider.setMaximum(20000)
    slider.setSingleStep(10)
    slider.setPageStep(100)
    slider.valueChanged.connect(gauge.update_value)
    layout.addWidget(gauge)
    layout.addWidget(slider)
    w.show()
    sys.exit( app.exec_() )
