#!/usr/bin/env python3
# Qtvcp Joypad widget
#
# Copyright (c) 2021  Jim Sloot <persei802@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
###############################################################################
import sys
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtCore import Qt, QPoint, QPointF, QLineF, QRect, QRectF, QSize, QSizeF, QEvent, Q_ENUM
from PyQt5.QtGui import QPainter, QPainterPath, QPen, QBrush, QColor, QFont, QPixmap, QRadialGradient

import hal
from qtvcp.widgets.widget_baseclass import _HalWidgetBase

class IndicatorPosition:
    NONE = 0
    LEFT = 1
    RIGHT = 2
    CENTER = 3
    TOP = 4
    BOTTOM = 5
    LEFTRIGHT = 6
    TOPBOTTOM = 7

class JoyPad(QtWidgets.QWidget, IndicatorPosition):
    IndicatorPosition = IndicatorPosition
    Q_ENUM(IndicatorPosition)

    joy_btn_pressed = QtCore.pyqtSignal(str)
    joy_btn_released = QtCore.pyqtSignal(str)
    joy_l_pressed = QtCore.pyqtSignal(bool)
    joy_l_released = QtCore.pyqtSignal(bool)
    joy_r_pressed = QtCore.pyqtSignal(bool)
    joy_r_released = QtCore.pyqtSignal(bool)
    joy_c_pressed = QtCore.pyqtSignal(bool)
    joy_c_released = QtCore.pyqtSignal(bool)
    joy_t_pressed = QtCore.pyqtSignal(bool)
    joy_t_released = QtCore.pyqtSignal(bool)
    joy_b_pressed = QtCore.pyqtSignal(bool)
    joy_b_released = QtCore.pyqtSignal(bool)
    def __init__(self, parent=None):
        super(JoyPad, self).__init__(parent)
        self.rect1 = QRectF()
        self.rect2 = QRectF()
        self.left_image = None
        self.right_image = None
        self.top_image = None
        self.bottom_image = None
        self.center_image = None
        self._dummyPixmap = QtGui.QPixmap()
        self._font = QFont('Lato Heavy', 20)
        self._text_color = QColor('white')
        self._textL = ''
        self._textR = ''
        self._textC = ''
        self._textT = ''
        self._textB = ''
        self.colorState = False
        self._true_color = QColor('lawngreen')
        self._false_color = QColor('gray')
        self.highlight_color = self._false_color
        self._highlightPosition = IndicatorPosition.NONE
        self.highlight_left = False
        self.highlight_right = False
        self.highlight_top = False
        self.highlight_bottom = False
        self.highlight_center = False
        self.last_active_btn = None
        self.setMouseTracking(True)
        self.setToolTipDuration(2000)
        self.installEventFilter(self)
        self.btn_names = {'L': 'left', 'R': 'right', 'T': 'top', 'B': 'bottom', 'C': 'center'}
        self.tooltips = {'L': '', 'R': '', 'T': '', 'B': '', 'C': ''}
        self.axis_list = ('X', 'Y', 'Z', 'A')

    def eventFilter(self, obj, event):
        if obj is self and self.isEnabled():
            if event.type() == QEvent.MouseButtonPress:
                if event.button() == Qt.RightButton:
                    event.ignore()
                else:
                    pos = event.localPos()
                    active_btn = self.get_active_btn(pos)
                    self.last_active_btn = active_btn
                    if active_btn is not None:
                        self._pressedOutput(active_btn)
            elif event.type() == QEvent.MouseButtonRelease:
                if event.button() == Qt.RightButton:
                    event.ignore()
                elif self.last_active_btn is not None:
                    self._releasedOutput(self.last_active_btn)
            elif event.type() == QEvent.MouseMove:
                pos = event.pos()
                active_btn = self.get_active_btn(pos)
                if active_btn is not None:
                    self.setToolTip(self.tooltips[active_btn])
        return super(JoyPad, self).eventFilter(obj, event)

    def _pressedOutput(self, btncode):
        self.joy_btn_pressed.emit(btncode)
        self['joy_{}_pressed'.format(btncode.lower())].emit(True)

    def _releasedOutput(self, btncode):
        self.joy_btn_released.emit(btncode)
        self['joy_{}_released'.format(btncode.lower())].emit(False)

    def get_active_btn(self, pos):
        if self.center_path.contains(pos): return 'C'
        elif self.left_path.contains(pos): return 'L'
        elif self.right_path.contains(pos): return 'R'
        elif self.bottom_path.contains(pos): return 'B'
        elif self.top_path.contains(pos): return 'T'
        return None

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(painter.Antialiasing)
        w = min(event.rect().width(), event.rect().height())
        self.rect1.setSize(QSizeF(w * 0.4, w * 0.4))
        self.rect2.setSize(QSizeF(w * 0.9, w * 0.9))
        self.create_paths(painter, event)
        self.draw_painter_paths(painter, event)
        self.draw_icons(painter, event)
        self.draw_highlight(painter, event)
        painter.end()

    def create_paths(self, qp, event):
        self.left_path = QPainterPath()
        self.right_path = QPainterPath()
        self.bottom_path = QPainterPath()
        self.top_path = QPainterPath()
        self.center_path = QPainterPath()
        center = event.rect().center()
        self.rect1.moveCenter(center)
        self.rect2.moveCenter(center)
        left_start = QPointF(self.rect1.topLeft())
        right_start = QPointF(self.rect1.bottomRight())
        bottom_start = QPointF(self.rect1.bottomLeft())
        top_start = QPointF(self.rect1.topRight())
        path = (self.right_path, self.top_path, self.left_path, self.bottom_path)
        start = (right_start, top_start, left_start, bottom_start)
        angle = -45
        for i in range(4):
            path[i].moveTo(start[i])
            path[i].arcTo(self.rect1, angle, 90)
            path[i].arcTo(self.rect2, angle + 90, -90)
            path[i].closeSubpath()
            angle += 90
        cap = QRectF()
        cap.setSize(QSizeF(self.rect1.width()*0.8, self.rect1.height()*0.8))
        cap.moveCenter(center)
        self.center_path.addEllipse(cap)

    def draw_painter_paths(self, qp, event):
        w = min(event.rect().width(), event.rect().height())
        center = event.rect().center()
        fp = QPoint(int(center.x() - w/4), int(center.y() - w/4))
        bg = QRadialGradient(center, w/2, fp)
        bg.setColorAt(0, QColor(180, 180, 180))
        bg.setColorAt(1, QColor(40, 40, 40))
        qp.setBrush(QBrush(bg))
        qp.setPen(QPen(QColor(Qt.black), 4))
        qp.drawPath(self.left_path)
        qp.drawPath(self.right_path)
        qp.drawPath(self.top_path)
        qp.drawPath(self.bottom_path)
        qp.drawPath(self.center_path)

    def draw_icons(self, qp, event):
        rect = QRect()
        rect.setSize(QSize(int(self.rect1.width() * 0.4), int(self.rect1.height() * 0.4)))
        center = event.rect().center()
        qp.setPen(QPen(self._text_color, 2))
        qp.setFont(self._font)
        # left button
        rect.moveCenter(QPoint(int(center.x() - self.rect2.width()/3), center.y()))
        if isinstance(self.left_image, QPixmap):
            pix = self.left_image
            qp.drawPixmap(rect, pix, pix.rect())
        elif isinstance(self.left_image, str):
            qp.drawText(rect, Qt.AlignCenter, self.left_image)
        # right button
        rect.moveCenter(QPoint(int(center.x() + self.rect2.width()/3), center.y()))
        if isinstance(self.right_image, QPixmap):
            pix = self.right_image
            qp.drawPixmap(rect, pix, pix.rect())
        elif isinstance(self.right_image, str):
            qp.drawText(rect, Qt.AlignCenter, self.right_image)
        # bottom button
        rect.moveCenter(QPoint(center.x(), int(center.y() + self.rect2.width()/3)))
        if isinstance(self.bottom_image, QPixmap):
            pix = self.bottom_image
            qp.drawPixmap(rect, pix, pix.rect())
        elif isinstance(self.bottom_image, str):
            qp.drawText(rect, Qt.AlignCenter, self.bottom_image)
        # top button
        rect.moveCenter(QPoint(center.x(), int(center.y() - self.rect2.width()/3)))
        if isinstance(self.top_image, QPixmap):
            pix = self.top_image
            qp.drawPixmap(rect, pix, pix.rect())
        elif isinstance(self.top_image, str):
            qp.drawText(rect, Qt.AlignCenter, self.top_image)
        # center button
        rect.moveCenter(QPoint(center.x(), center.y()))
        if isinstance(self.center_image, QPixmap):
            pix = self.center_image
            qp.drawPixmap(rect, pix, pix.rect())
        elif isinstance(self.center_image, str):
            qp.drawText(rect, Qt.AlignCenter, self.center_image)

    def draw_highlight(self, qp, event):
        rect = QRectF()
        rect.setSize(self.rect1.size() * 0.9)
        center = event.rect().center()
        rect.moveCenter(center)
        pen_width = self.rect1.width() * 0.08
        qp.setPen(QPen(self.highlight_color, pen_width, cap = Qt.FlatCap))
        if self.highlight_center is True:
            qp.drawArc(rect, 0, 5760)
        else:
            if self.highlight_right is True:
                qp.drawArc(rect, -45 * 16, 90 * 16)
            if self.highlight_left is True:
                qp.drawArc(rect, 135 * 16, 90 * 16)
            if self.highlight_top is True:
                qp.drawArc(rect, 45 * 16, 90 * 16)
            if self.highlight_bottom is True:
                qp.drawArc(rect, 225 * 16, 90 * 16)

    def reset_highlight(self):
        self.highlight_left = False
        self.highlight_right = False
        self.highlight_top = False
        self.highlight_bottom = False
        self.highlight_center = False

    def set_highlight(self, btn, state=True):
        if type(btn) == int:
            if btn == IndicatorPosition.LEFT:
                btn ='L'
            elif btn == IndicatorPosition.RIGHT:
                btn ='R'
            elif btn == IndicatorPosition.CENTER:
                btn ='C'
            elif btn == IndicatorPosition.TOP:
                btn ='T'
            elif btn == IndicatorPosition.BOTTOM:
                btn ='B'
            elif btn == IndicatorPosition.LEFTRIGHT:
                btn ='X'
            elif btn == IndicatorPosition.TOPBOTTOM:
                btn ='Z'
            elif btn == IndicatorPosition.NONE:
                self.reset_highlight()
                return
            else:
                print ('undefined position:{}'.format(btn))
                return
        if btn not in self.axis_list and btn not in self.btn_names.keys(): return
        if btn == 'X' or btn == 'A':
            self.highlight_left = state
            self.highlight_right = state
        elif btn == 'Y' or btn == 'Z':
            self.highlight_top = state
            self.highlight_bottom = state
        else:
            name = self.btn_names[btn]
            self['highlight_' + name] = state
        self.update()

    def set_button_icon(self, btn, path):
        self.set_icon(btn, 'image', path)
    def set_button_text(self, btn, text):
        self.set_icon(btn, 'text', text)

    def set_icon(self, btn, kind, data):
        if btn not in self.btn_names.keys(): return
        name = self.btn_names[btn]
        if kind == 'image':
            if data is None:
                self[name + "_image"] = None
            else:
                self[name + "_image"] = QPixmap(data)
        elif kind == 'text':
            self[name + "_image"] = data
        else: return
        self.update()

    def set_tooltip(self, btn, tip):
        if btn in self.btn_names.keys():
            self.tooltips[btn] = tip

    def setLight(self, data):
        if data:
            self.highlight_color = self._true_color
        else:
            self.highlight_color = self._false_color
        self.update()

    def set_HighlightPosition(self, position):
        self._highlightPosition = position
        self.reset_highlight()
        self.set_highlight(position, True)
        self.update()

    def get_HighlightPosition(self):
        return self._highlightPosition

    def reset_HighlightPosition(self):
        self._highlightPosition = IndicatorPosition.NONE
        self.reset_highlight()
        self.update()

    highlightPosition = QtCore.pyqtProperty(IndicatorPosition,
                        get_HighlightPosition, set_HighlightPosition, reset_HighlightPosition)

    @QtCore.pyqtSlot()
    def set_colorStateTrue(self):
        self.setLight(True)
    @QtCore.pyqtSlot()
    def set_colorStateFalse(self):
        self.setLight(False)

    @QtCore.pyqtSlot(bool)
    def set_colorState(self, state):
        self.colorState = bool(state)
        self.setLight(state)

    def get_colorState(self):
        return self.colorState

    def reset_colorState(self):
        self.colorState = False
        self.setLight(False)

    setColorState = QtCore.pyqtProperty(bool, get_colorState, set_colorState, reset_colorState)

    def setLeftImagePath(self, data):
        if data.isNull():
            data = None
        self.set_icon('L', 'image', data)
    def getLeftImagePath(self):
        if isinstance(self.left_image, QPixmap):
            self.left_image
        else:
            return self._dummyPixmap
    def resetLeftImagePath(self):
        pass

    def setRightImagePath(self, data):
        if data.isNull():
            data = None
        self.set_icon('R', 'image', data)
    def getRightImagePath(self):
        return self._dummyPixmap
    def resetRightImagePath(self):
        pass

    def setCenterImagePath(self, data):
        if data.isNull():
            data = None
        self.set_icon('C', 'image', data)
    def getCenterImagePath(self):
        return self._dummyPixmap
    def resetCenterImagePath(self):
        pass

    def setTopImagePath(self, data):
        if data.isNull():
            data = None
        self.set_icon('T', 'image', data)
    def getTopImagePath(self):
        return self._dummyPixmap
    def resetTopImagePath(self):
        pass

    def setBottomImagePath(self, data):
        if data.isNull():
            data = None
        self.set_icon('B', 'image', data)
    def getBottomImagePath(self):
        return self._dummyPixmap
    def resetBottomImagePath(self):
        pass

    left_image_path = QtCore.pyqtProperty(QPixmap, getLeftImagePath, setLeftImagePath, resetLeftImagePath)
    right_image_path = QtCore.pyqtProperty(QPixmap, getRightImagePath, setRightImagePath, resetRightImagePath)
    center_image_path = QtCore.pyqtProperty(QPixmap, getCenterImagePath, setCenterImagePath, resetCenterImagePath)
    top_image_path = QtCore.pyqtProperty(QPixmap, getTopImagePath, setTopImagePath, resetTopImagePath)
    bottom_image_path = QtCore.pyqtProperty(QPixmap, getBottomImagePath, setBottomImagePath, resetBottomImagePath)


    def getFont(self):
        return self._font
    def setFont(self, value):
        self._font = value
    def resetFont(self):
        self._font = QFont('Lato Heavy', 20)
    button_font = QtCore.pyqtProperty(QFont, getFont, setFont, resetFont)

    def setLeftText(self, data):
        self._textL = data
        if not data.strip():
            data = None
        self.set_icon('L', 'text', data)
    def getLeftText(self):
        return self._textL
    def resetLeftText(self):
        self._textL = ''
        self.set_icon('L', 'text', '')

    def setRightText(self, data):
        self._textR = data
        if not data.strip():
            data = None
        self.set_icon('R', 'text', data)
    def getRightText(self):
        return self._textR
    def resetRightText(self):
        self._textR = ''
        self.set_icon('R', 'text', '')

    def setCenterText(self, data):
        self._textC = data
        if not data.strip():
            data = None
        self.set_icon('C', 'text', data)
    def getCenterText(self):
        return self._textC
    def resetCenterText(self):
        self._textC = ''
        self.set_icon('C', 'text', '')

    def setTopText(self, data):
        self._textT = data
        if not data.strip():
            data = None
        self.set_icon('T', 'text', data)
    def getTopText(self):
        return self._textT
    def resetTopText(self):
        self._textT = ''
        self.set_icon('T', 'text', '')

    def setBottomText(self, data):
        self._textB = data
        if not data.strip():
            data = None
        self.set_icon('B', 'text', data)
    def getBottomText(self):
        return self._textB
    def resetBottomText(self):
        self._textB = ''
        self.set_icon('B', 'text', '')

    left_text = QtCore.pyqtProperty(str, getLeftText, setLeftText, resetLeftText)
    right_text = QtCore.pyqtProperty(str, getRightText, setRightText, resetRightText)
    center_text = QtCore.pyqtProperty(str, getCenterText, setCenterText, resetCenterText)
    top_text = QtCore.pyqtProperty(str, getTopText, setTopText, resetTopText)
    bottom_text = QtCore.pyqtProperty(str, getBottomText, setBottomText, resetBottomText)

    @QtCore.pyqtSlot(QColor)
    def set_true_color(self, color):
        self._true_color = color
        self.setLight(self.colorState)
    @QtCore.pyqtSlot(str)
    def set_true_color(self, color):
        self._true_color = QColor(color)
        self.setLight(self.colorState)

    def get_true_color(self):
        return self._true_color

    def reset_true_color(self):
        self._true_color = QColor('lawngreen')
        self.setLight(self.colorState)

    @QtCore.pyqtSlot(QColor)
    def set_false_color(self, color):
        self._false_color = color
        self.setLight(self.colorState)
    @QtCore.pyqtSlot(str)
    def set_false_color(self, color):
        self._false_color = QColor(color)
        self.setLight(self.colorState)

    def get_false_color(self):
        return self._false_color

    def reset_false_color(self):
        self._false_color = QColor('gray')
        self.setLight(self.colorState)

    def set_text_color(self, color):
        self._text_color = QColor(color)
        self.update()
    def get_text_color(self):
        return self._text_color
    def reset_text_color(self):
        self._text_color = QColor('white')
        self.update()

    true_color = QtCore.pyqtProperty(QColor, get_true_color, set_true_color, reset_true_color)
    false_color = QtCore.pyqtProperty(QColor, get_false_color, set_false_color, reset_false_color)
    text_color = QtCore.pyqtProperty(QColor, get_text_color, set_text_color, reset_text_color)

    @QtCore.pyqtSlot(str)
    def btn_pressed(self, btn):
        print("Button pressed", btn)

    @QtCore.pyqtSlot(str)
    def btn_released(self, btn):
        print("Button released", btn)

    # required code for object indexing
    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)

class HALPinType:
    NONE = 0
    BIT = hal.HAL_BIT
    S32 = hal.HAL_S32
    FLOAT = hal.HAL_FLOAT

class HALPad(JoyPad, _HalWidgetBase, HALPinType):
    HALPinType = HALPinType
    Q_ENUM(HALPinType)

    def __init__(self, parent=None):
        super(HALPad, self).__init__(parent)
        self._pin_name = ''
        self._pin_type = HALPinType.BIT

        self._trueOutputR = 1.0
        self._trueOutputL = 1.0
        self._trueOutputC = 1.0
        self._trueOutputT = 1.0
        self._trueOutputB = 1.0
        self._falseOutput = 0.0

        self._true_color = QColor('lawngreen')
        self._false_color = QColor('red')

        # we want center active for an indicator light
        self.setProperty('highlightPosition',JoyPad.CENTER)
        self.setLight(False)

    def _hal_init(self):
        if self._pin_name == '':
            pname = self.HAL_NAME_
        else:
            pname = self._pin_name
        if not self._pin_type == HALPinType.NONE:
            ptype = self._pin_type
            self.halPinR = self.HAL_GCOMP_.newpin(pname + '.right', ptype, hal.HAL_OUT)
            self.halPinL = self.HAL_GCOMP_.newpin(pname + '.left', ptype, hal.HAL_OUT)
            self.halPinT = self.HAL_GCOMP_.newpin(pname + '.top', ptype, hal.HAL_OUT)
            self.halPinB = self.HAL_GCOMP_.newpin(pname + '.bottom', ptype, hal.HAL_OUT)
            self.halPinC = self.HAL_GCOMP_.newpin(pname + '.center', ptype, hal.HAL_OUT)
        if not self._highlightPosition == IndicatorPosition.NONE:
            self.halPinLightCenter = self.HAL_GCOMP_.newpin(pname + '.light.center', hal.HAL_BIT, hal.HAL_IN)
            self.halPinLightCenter.value_changed.connect(lambda data: self.setLight(data))

    def _pressedOutput(self, btncode):
        self.joy_btn_pressed.emit(btncode)
        self['joy_{}_pressed'.format(btncode.lower())].emit(True)

        if self._pin_type == HALPinType.BIT:
            data = True
        elif self._pin_type == HALPinType.FLOAT:
            data = float(self['_trueOutput{}'.format(btncode)])
        elif self._pin_type == HALPinType.S32:
            data = int(self['_trueOutput{}'.format(btncode)])
        else:
            return
        try:
            self['halPin{}'.format(btncode)].set(data)
        except:
            pass

    def _releasedOutput(self, btncode):
        self.joy_btn_released.emit(btncode)
        self['joy_{}_released'.format(btncode.lower())].emit(False)

        if self._pin_type == HALPinType.BIT:
            data = False
        elif self._pin_type == HALPinType.FLOAT:
            data = float(self._falseOutput)
        elif self._pin_type == HALPinType.S32:
            data = int(self._falseOutput)
        else:
            return
        try:
            self['halPin{}'.format(btncode)].set(data)
        except:
            pass

    def setPinTrueOutput(self, btn, data):
        btncode = self.btn_names[btn.lower()]
        self['_trueOutput{}'.format(btncode)] = data

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    ########################################################################

    def set_pin_name(self, value):
        self._pin_name = value
    def get_pin_name(self):
        return self._pin_name
    def reset_pin_name(self):
        self._pin_name = ''

    def set_pin_type(self, value):
        self._pin_type = value
        print(value)
    def get_pin_type(self):
        return self._pin_type
    def reset_pin_type(self):
        self._pin_type = HALPinType.BIT

    # designer will show these properties in this order:
    pin_type = QtCore.pyqtProperty(HALPinType, get_pin_type, set_pin_type, reset_pin_type)
    pin_name = QtCore.pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)

    #############################
    # Testing                   #
    #############################
if __name__ == "__main__":
    import sys
    from PyQt5.QtWidgets import QWidget, QVBoxLayout, QApplication
    app = QtWidgets.QApplication(sys.argv)
    w = QWidget()
    w.setGeometry(100, 100, 600, 400)
    w.setWindowTitle('JoyPad')
    joy = JoyPad()
    joy.set_icon('L', 'text', 'X-')
    joy.set_icon('R', 'text', 'X+')
    joy.set_icon('T', 'text', 'Y+')
    joy.set_icon('B', 'text', 'Y-')
    joy.set_icon('C', 'image', 'stop.png')
    joy.set_tooltip('T', 'This is the top button')
    joy.set_tooltip('C', 'This is the center button')
    joy.set_false_color('red')
    joy.set_highlight('T', True)
    joy.joy_btn_pressed.connect(joy.btn_pressed)
    joy.joy_btn_released.connect(joy.btn_released)
    layout = QtWidgets.QVBoxLayout()
    layout.addWidget(joy)
    w.setLayout(layout)
    w.show()
    sys.exit( app.exec_() )

