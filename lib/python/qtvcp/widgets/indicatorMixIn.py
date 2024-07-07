from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtCore import pyqtProperty, pyqtSlot
from qtvcp.widgets.widget_baseclass import (_HalWidgetBase)
from qtvcp.lib.aux_program_loader import Aux_program_loader as _loader
from qtvcp.core import Action, Status, Info
import hal

AUX_PRGM = _loader()
ACTION = Action()
STATUS = Status()
INFO = Info()

# LED indicator on the right corner
class IndicatedMixIn( _HalWidgetBase):
    def __init__(self):
        super(IndicatedMixIn, self).__init__()
        self._indicator_state = False # Current State
        # if user want to block their signals
        self._external_block_signal = False

        # changing text data
        self._state_text = False # use text
        self._true_string = 'True'
        self._false_string = 'False'

        # python commands data
        self._python_command = False # use commands
        self.true_python_command = '''print("true command")'''
        self.false_python_command = '''print("false command")'''

        # indicator LED data
        self.draw_indicator = False # Show LED
        self._HAL_pin = False # control by HAL
        self._shape = 0 # 0 triangle, 1 circle
        self._size = .3 # triangle
        self._diameter = 10 # circle
        self._corner_radius = 5
        self._h_fraction = .3
        self._w_fraction = .9
        self._right_edge_offset = 0
        self._top_edge_offset = 0
        self._on_color = QtGui.QColor("red")
        self._off_color = QtGui.QColor("black")
        # current flash state
        self._flashState = False
        # is flashing on?
        self._flashing = True
        # should flash?
        self._flashOption = False
        self._flashRate = 200

        self._timer = QtCore.QTimer()
        self._timer.timeout.connect(self.toggleFlashState)
        #self.setFlashing(True)

        # indicator linuxcnc's status options
        self._ind_status = False # control by status
        self._is_estopped = False
        self._is_on = False
        self._is_homed = False
        self._is_idle = False
        self._is_paused = False
        self._invert_status = False
        self._is_flood = False
        self._is_mist = False
        self._is_block_delete = False
        self._is_optional_stop = False
        self._is_joint_homed = False
        self._is_limits_overridden = False
        self._is_manual = False
        self._is_mdi = False
        self._is_auto = False
        self._is_spindle_stopped = False
        self._is_spindle_fwd = False
        self._is_spindle_rev = False
        self._joint_number = 0

        # property data for style sheet dynamic changes
        self._isManualProp = False
        self._isMDIProp = False
        self._isAutoProp = False
        self._isEstopProp = False
        self._isStateOnProp = False
        self._isAllHomed = False
        self._isHomed = False

        # button enabled states
        self._is_all_homed_sensitive = False
        self._is_on_sensitive = False
        self._is_idle_sensitive = False
        self._is_run_sensitive = False
        self._is_run_paused_sensitive = False
        self._is_auto_pause_sensitive = False
        self._is_manual_sensitive = False
        self._is_mdi_sensitive = False
        self._is_auto_sensitive = False
        self._is_spindle_off_sensitive = False
        self._is_spindle_fwd_sensitive = False
        self._is_spindle_rev_sensitive = False

    def _hal_init(self):
        if self._HAL_pin:
            if self._pin_name_ == '':
                pname = self.HAL_NAME_
            else:
                pname = self._pin_name_
            self.hal_pin_led = self.HAL_GCOMP_.newpin(pname + '-led', hal.HAL_BIT, hal.HAL_IN)
            self.hal_pin_led.value_changed.connect(lambda data: self.indicator_update(data))
        elif self._ind_status:
            self._init_state_change()
        self._globalParameter = {'__builtins__' : None, 'MAIN_INSTANCE':self.QTVCP_INSTANCE_,
                                    'INSTANCE':self.THIS_INSTANCE_,
                                 'PROGRAM_LOADER':AUX_PRGM, 'ACTION':ACTION, 'HAL':hal, 'print':print}
        self._localsParameter = {'dir': dir, 'True':True, 'False':False}

        if self._is_on_sensitive:
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
            STATUS.connect('state-on', lambda w: enable_logic_check(True))
        if self._is_all_homed_sensitive:
            STATUS.connect('all-homed', lambda w: enable_logic_check(True))
            STATUS.connect('not-all-homed', lambda w, axis: self.setEnabled(False))
        if self._is_idle_sensitive:
            STATUS.connect('interp-run', lambda w: self.setEnabled(False))
            STATUS.connect('interp-paused', lambda w: self.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: enable_logic_check(True))
        elif self._is_run_sensitive:
            STATUS.connect('interp-run', lambda w: enable_logic_check(True))
            STATUS.connect('interp-paused', lambda w: self.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: self.setEnabled(False))
        elif self._is_run_paused_sensitive:
            STATUS.connect('interp-run', lambda w: enable_logic_check(True))
            STATUS.connect('interp-paused', lambda w: self.setEnabled(True))
            STATUS.connect('interp-idle', lambda w: self.setEnabled(False))
        elif self._is_auto_pause_sensitive:
            STATUS.connect('interp-run', lambda w: enable_logic_check(False))
            STATUS.connect('interp-paused', lambda w: self.setEnabled(True))
            STATUS.connect('interp-idle', lambda w: self.setEnabled(False))
        if self._is_spindle_off_sensitive or \
                self._is_spindle_fwd_sensitive or \
                self._is_spindle_rev_sensitive:
            STATUS.connect('spindle-control-changed',  lambda w, num, state, speed, upto: enable_logic_check(True))
        if self._is_manual_sensitive or self._is_mdi_sensitive or self._is_auto_sensitive:
            STATUS.connect('mode-manual', lambda w: enable_logic_check(self._is_manual_sensitive))
            STATUS.connect('mode-mdi', lambda w: enable_logic_check(self._is_mdi_sensitive))
            STATUS.connect('mode-auto', lambda w: enable_logic_check(self._is_auto_sensitive))

        # check for multiple selected enabled requests
        def enable_logic_check(state):
            if state:
                temp = True
                if self._is_run_sensitive or self._is_run_paused_sensitive:
                    temp = temp and STATUS.is_interp_running()
                if self._is_auto_pause_sensitive:
                    temp = temp and STATUS.is_auto_paused()
                if self._is_on_sensitive:
                    temp = temp and STATUS.machine_is_on()
                if self._is_all_homed_sensitive:
                    temp = temp and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED)
                if self._is_manual_sensitive:
                    temp = temp and STATUS.is_man_mode()
                if self._is_mdi_sensitive:
                    temp = temp and STATUS.is_mdi_mode()
                if self._is_auto_sensitive:
                    temp = temp and STATUS.is_auto_mode()
                if self._is_idle_sensitive:
                    temp = temp and STATUS.is_interp_idle()
                on =  STATUS.is_spindle_on()
                spd = STATUS.get_spindle_speed()
                if self._is_spindle_fwd_sensitive and self._is_spindle_rev_sensitive:
                    temp = temp and on
                elif self._is_spindle_fwd_sensitive:
                    temp = temp and spd > 0
                elif self._is_spindle_rev_sensitive:
                    temp = temp and spd < 0
                if self._is_spindle_off_sensitive:
                    temp = temp and not on

                self.setEnabled(temp)
            else:
                self.setEnabled(False)

        self.connectSignals()

    # Disconnect our standard signals
    def disconnectSignals(self):
        try:
            self.toggled.disconnect()
        except Exception: pass
        try:
            self.pressed.disconnect()
            self.released.disconnect()
        except Exception: pass

    # done so we can adjust 'checkable' signals at run time
    # ie delete all signals an re apply them
    def connectSignals(self):
        def _update(state):
            self.setChecked(state)
            if self._HAL_pin is False:
                self.indicator_update(state)
            # if using state labels option update the labels
            if self._state_text:
                self.setText(None)
            # if python commands call them 
            if self._python_command:
                if state == None:
                    state = self._indicator_state
                self.python_command(state)

        if self.isCheckable():
            self.toggled[bool].connect(_update)
        else:
            self.pressed.connect(lambda: _update(True))
            self.released.connect(lambda: _update(False))
        _update(self.isChecked())

    # for users to indicate they want to block their signals
    def _blockSignals(self, state):
        self._external_block_signal = bool(state)
    def _isSignalsBlocked(self):
        return self._external_block_signal

    def _init_state_change(self):
        def only_false(data):
            if data:
                return
            self._flip_state(False)

        if self._is_estopped:
            STATUS.connect('state-estop', lambda w: self._flip_state(True, prop='isEstopped'))
            STATUS.connect('state-estop-reset', lambda w: self._flip_state(False, prop ='isEstopped'))
        elif self._is_on:
            STATUS.connect('state-on', lambda w: self._flip_state(True, prop ='isStateOn'))
            STATUS.connect('state-off', lambda w: self._flip_state(False, prop ='isStateOn'))
        elif self._is_homed:
            STATUS.connect('all-homed', lambda w: self._flip_state(True, prop = 'isAllHomed'))
            STATUS.connect('not-all-homed', lambda w, axis: self._flip_state(False, prop = 'isAllHomed'))
        elif self._is_idle:
            STATUS.connect('interp-idle', lambda w: self._flip_state(True))
            STATUS.connect('interp-run', lambda w: self._flip_state(False))
        elif self._is_paused:
            STATUS.connect('program-pause-changed', lambda w, data: self._flip_state(data))
        elif self._is_flood:
            STATUS.connect('flood-changed', lambda w, data: self._flip_state(data))
        elif self._is_mist:
            STATUS.connect('mist-changed', lambda w, data: self._flip_state(data))
        elif self._is_block_delete:
            STATUS.connect('block-delete-changed', lambda w, data: self._flip_state(data))
        elif self._is_optional_stop:
            STATUS.connect('optional-stop-changed', lambda w, data: self._flip_state(data))
        elif self._is_joint_homed:
            STATUS.connect('homed', lambda w, data: self._j_homed(data))
            STATUS.connect('not-all-homed', lambda w, data: self._j_unhomed(data))
        elif self._is_limits_overridden:
            STATUS.connect('override-limits-changed', self._check_override_limits)
            STATUS.connect('hard-limits-tripped', lambda w, data, group: only_false(data))
        elif self._is_manual or self._is_mdi or self._is_auto:
            STATUS.connect('mode-manual', lambda w: self._mode_changed(0))
            STATUS.connect('mode-mdi', lambda w: self._mode_changed(1))
            STATUS.connect('mode-auto', lambda w: self._mode_changed(2))
        elif self._is_spindle_stopped or self._is_spindle_fwd or self._is_spindle_rev:
            STATUS.connect('spindle-control-changed',  lambda w, num, state, speed, upto: self._spindle_changed(speed))

    def _flip_state(self, data, prop = None):
            if self._invert_status:
                data = not data
            self.indicator_update(data)
            if prop is not None:
                self._style_polish(prop, data)

    def _j_homed(self, joint):
        if int(joint) == self._joint_number:
            self._flip_state(True, prop = 'isHomed')

    def _j_unhomed(self, jlist):
        if str(self._joint_number) in jlist:
            self._flip_state(False, prop = 'isHomed')

    def _check_override_limits(self, w, state, data):
        for i in data:
            if i == 1:
                self._flip_state(True)
                return
        self._flip_state(False)

    def _mode_changed(self, mode):
        state = False
        if self._is_manual:
            prop ='isManual'
            if  mode == 0:
                state = True
        elif self._is_mdi:
            prop ='isMDI'
            if mode == 1:
                state = True
        elif self._is_auto:
            prop ='isAuto'
            if mode == 2:
                state = True

        self._flip_state(state)
        self._style_polish(prop, state)

    def _spindle_changed(self, state):
        if self._is_spindle_stopped and state == 0:
            self._flip_state(True)
        elif self._is_spindle_rev and state < 0:
            self._flip_state(True)
        elif self._is_spindle_fwd and state > 0:
            self._flip_state(True)
        else:
            self._flip_state(False)

    # force style sheet to update
    def _style_polish(self, prop, state):
        # print(prop,state)
        self.setProperty(prop, state)
        self.style().unpolish(self)
        self.style().polish(self)

    # arbitraray python commands are possible using 'INSTANCE' in the string
    # gives access to widgets and handler functions
    # builtin python commands are restricted see self._globalParameter
    def python_command(self, state = None):
        if self._python_command:
            if state:
                try:
                    exec(self.true_python_command, self._globalParameter, self._localsParameter)
                except TypeError as e:
                    LOG.error('({} called exec in error:{}'.format(self.objectName(),e))
                    LOG.warning('   Command was {}'.format(self.true_python_command))
                except AttributeError as e:
                    LOG.error('({} called exec in error:{}'.format(self.objectName(),e))
                    LOG.warning('   Command was {}'.format(self.true_python_command))
                    LOG.verbose('   List of objects:')
                    LOG.verbose(dir(self.QTVCP_INSTANCE_))
            else:
                try:
                    exec(self.false_python_command, self._globalParameter, self._localsParameter)
                except TypeError as e:
                    LOG.error('({} called exec in error:{}'.format(self.objectName(),e))
                    LOG.warning('   Command was {}'.format(self.false_python_command))
                except AttributeError as e:
                    LOG.error('({} called exec in error:{}'.format(self.objectName(),e))
                    LOG.warning('   Command was {}'.format(self.false_python_command))
                    LOG.verbose('   List of objects:')
                    LOG.verbose(dir(self.QTVCP_INSTANCE_))

    # callback to toggle text when button is toggled
    def toggle_text(self, state=None):
        if not self._state_text:
            return
        else:
            self.setText(None)

    @pyqtSlot()
    def toggleFlashState(self):
        self._flashState = not self._flashState
        self.update()

    def isFlashing(self):
        return self._flashing

    @pyqtSlot(bool)
    def setFlashing(self, value):
        #self._flashing = value
        if self._flashRate > 0 and value:
            if self._timer.isActive():
                return
            self._timer.start(self._flashRate)
        else:
            self._timer.stop()
        self.update()

    def indicator_update(self, data):
        self._flashing = self._indicator_state = data
        self.update()

    # override paint function to first paint the stock button
    # then our indicator paint routine
    def paintEvent(self, event):
        self.paintEvent(event)
        if self.draw_indicator:
            self.paintIndicator()

    # Paint specified size a triangle at the top right
    def paintIndicator(self):
            p = QtGui.QPainter(self)

 
            if self._flashOption and self._flashing:
                if self._flashState:
                    color = self._on_color
                else:
                    color = self._off_color
            elif self._indicator_state:
                color = self._on_color
            else:
                color = self._off_color

            # right triangle
            if self._shape == 0:
                rect = p.window()
                top_right = rect.topRight() - QtCore.QPoint(self._right_edge_offset,-self._top_edge_offset)
                if self.width() < self.height():
                    size = int(self.width() * self._size)
                else:
                    size = int(self.height() * self._size)

                gradient = QtGui.QLinearGradient(top_right- QtCore.QPoint(size, 0), top_right)
                gradient.setColorAt(0, QtCore.Qt.white)
                gradient.setColorAt(1, color)
                p.setBrush(QtGui.QBrush(gradient))
                p.setPen(color)

                triangle = QtGui.QPolygon([top_right, top_right - QtCore.QPoint(size, 0),
                                       top_right + QtCore.QPoint(0, size)])
                p.drawLine(triangle.point(1), triangle.point(2))
                p.drawPolygon(triangle)

            # circle
            elif self._shape == 1:
                x = int(self.width() - self._diameter - self._right_edge_offset)
                y = int(0 + self._top_edge_offset)
                gradient = QtGui.QRadialGradient(x + self._diameter / 2, y + self._diameter / 2,
                                   self._diameter * 0.4, self._diameter * 0.4, self._diameter * 0.4)
                gradient.setColorAt(0, QtCore.Qt.white)
                gradient.setColorAt(1, color)
                p.setBrush(QtGui.QBrush(gradient))
                p.setPen(color)
                p.setRenderHint(QtGui.QPainter.Antialiasing, True)
                p.drawEllipse(x, y, self._diameter - 1, self._diameter - 1)

            # top bar
            elif self._shape == 2:
                rect = p.window()
                topLeft = rect.topLeft()
                #p.setPen(color)
                #p.setBrush(QtGui.QBrush(color, QtCore.Qt.SolidPattern))
                grad = QtGui.QLinearGradient()
                grad.setCoordinateMode(QtGui.QGradient.ObjectBoundingMode)
                grad.setStart(0,0)
                grad.setFinalStop(0,.8)
                grad.setColorAt(0, color)
                grad.setColorAt(.5, QtCore.Qt.white)
                grad.setColorAt(.8, color)
                p.setBrush(QtGui.QBrush(grad))
                p.drawRoundedRect(int(topLeft.x()+(self.width()*((1-self._w_fraction)/2)) + self._top_edge_offset),
                                    int(topLeft.y()+self._right_edge_offset),
                                    int(self.width()*self._w_fraction+2),
                                    int(self.height()*self._h_fraction),
                                    self._corner_radius, self._corner_radius)


            # side bar
            elif self._shape == 3:
                rect = p.window()
                topRight = rect.topRight()
                #p.setPen(color)
                #p.setBrush(QtGui.QBrush(color, QtCore.Qt.SolidPattern))
                grad = QtGui.QLinearGradient()
                grad.setCoordinateMode(QtGui.QGradient.ObjectBoundingMode)
                grad.setStart(0,0)
                grad.setFinalStop(0,.8)
                grad.setColorAt(0, color)
                grad.setColorAt(.5, QtCore.Qt.white)
                grad.setColorAt(.8, color)
                p.setBrush(QtGui.QBrush(grad))
                p.drawRoundedRect(int(topRight.x()- self.width()*self._w_fraction-self._right_edge_offset),
                                    int(topRight.y()+(self.height()*((1-self._h_fraction)/2)) + self._top_edge_offset),
                                    int(self.width()*self._w_fraction),
                                    int(self.height()*self._h_fraction),
                                    self._corner_radius, self._corner_radius)

            # left triangle
            if self._shape == 4:
                rect = p.window()
                top_left = rect.topLeft() - QtCore.QPoint(self._right_edge_offset,-self._top_edge_offset)
                if self.width() < self.height():
                    size = int(self.width() * self._size)
                else:
                    size = int(self.height() * self._size)

                gradient = QtGui.QLinearGradient(top_left- QtCore.QPoint(size, 0), top_left)
                gradient.setColorAt(0, QtCore.Qt.white)
                gradient.setColorAt(1, color)
                p.setBrush(QtGui.QBrush(gradient))
                p.setPen(color)

                triangle = QtGui.QPolygon([top_left, top_left + QtCore.QPoint(size, 0),
                                       top_left + QtCore.QPoint(0, size)])
                p.drawLine(triangle.point(1), triangle.point(2))
                p.drawPolygon(triangle)

    def set_indicator(self, data):
        self.draw_indicator = data
        self.update()
    def get_indicator(self):
        return self.draw_indicator
    def reset_indicator(self):
        self.draw_indicator = False

    def set_shape(self, data):
        self._shape = data
        self.update()
    def get_shape(self):
        return self._shape
    def reset_shape(self):
        self._shape = 0

    def set_HAL_pin(self, data):
        self._HAL_pin = data
        if data:
            self._ind_status = False
    def get_HAL_pin(self):
        return self._HAL_pin
    def reset_HAL_pin(self):
        self._HAL_pin = False

    def set_ind_status(self, data):
        self._ind_status = data
        if data:
            self._HAL_pin = False
    def get_ind_status(self):
        return self._ind_status
    def reset_ind_status(self):
        self._ind_status = False

    def set_state_text(self, data):
        self._state_text = data
        if data:
            self.setText(None)
    def get_state_text(self):
        return self._state_text
    def reset_state_text(self):
        self._state_text = False

    def set_python_command(self, data):
        self._python_command = data
    def get_python_command(self):
        return self._python_command
    def reset_python_command(self):
        self._python_command = False

    def get_on_color(self):
        return self._on_color
    def set_on_color(self, value):
        self._on_color = value
        self.update()
    def get_off_color(self):
        return self._off_color
    def set_off_color(self, value):
        self._off_color = value
        self.update()

    # flash when state on
    def setFlashState(self, value):
        self._flashOption = value
        self.setFlashing(True)
        self.update()

    def getFlashState(self):
        return self._flashOption

    def getFlashRate(self):
        return self._flashRate
    @pyqtSlot(int)
    def setFlashRate(self, value):
        self._flashRate = value
        self.update()

    def set_indicator_size(self, data):
        self._size = data
        self.update()
    def get_indicator_size(self):
        return self._size
    def reset_indicator_size(self):
        self._size = 0.3
        self.update()

    def set_circle_diameter(self, data):
        self._diameter = data
        self.update()
    def get_circle_diameter(self):
        return self._diameter
    def reset_circle_diameter(self):
        self._diameter = 10
        self.update()

    def set_corner_radius(self, data):
        self._corner_radius = data
        self.update()
    def get_corner_radius(self):
        return self._corner_radius
    def reset_corner_radius(self):
        self._corner_radius = 5
        self.update()

    def set_h_fraction(self, data):
        self._h_fraction = data
        self.update()
    def get_h_fraction(self):
        return self._h_fraction
    def reset_h_fraction(self):
        self._h_fraction = .5
        self.update()

    def set_w_fraction(self, data):
        self._w_fraction = data
        self.update()
    def get_w_fraction(self):
        return self._w_fraction
    def reset_w_fraction(self):
        self._w_fraction = .5
        self.update()

    def set_right_edge_offset(self, data):
        self._right_edge_offset = int(data)
        self.update()
    def get_right_edge_offset(self):
        return self._right_edge_offset
    def reset_right_edge_offset(self):
        self._right_edge_offset = 0
        self.update()

    def set_top_edge_offset(self, data):
        self._top_edge_offset = int(data)
        self.update()
    def get_top_edge_offset(self):
        return self._top_edge_offset
    def reset_top_edge_offset(self):
        self._top_edge_offset = 0
        self.update()

    def set_true_string(self, data):
        data = data.replace('\\n' , '\n')
        self._true_string = data
        if self._state_text:
            self.setText(None)
    def get_true_string(self):
        return self._true_string
    def reset_true_string(self):
        self._true_string = 'False'

    def set_false_string(self, data):
        data = data.replace('\\n' , '\n')
        self._false_string = data
        if self._state_text:
            self.setText(None)
    def get_false_string(self):
        return self._false_string
    def reset_false_string(self):
        self._false_string = 'False'

    def set_true_python_command(self, data):
        self.true_python_command = data
    def get_true_python_command(self):
        return self.true_python_command
    def reset_true_python_command(self):
        self.true_python_command = ''

    def set_false_python_command(self, data):
        self.false_python_command = data
    def get_false_python_command(self):
        return self.false_python_command
    def reset_false_python_command(self):
        self.false_python_command = ''

    def set_pin_name(self, value):
        self._pin_name_ = value
    def get_pin_name(self):
        return self._pin_name_
    def reset_pin_name(self):
        self._pin_name_ = ''

    pin_name = QtCore.pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)
    indicator_option = QtCore.pyqtProperty(bool, get_indicator, set_indicator, reset_indicator)
    indicator_HAL_pin_option = QtCore.pyqtProperty(bool, get_HAL_pin, set_HAL_pin, reset_HAL_pin)
    indicator_status_option = QtCore.pyqtProperty(bool, get_ind_status, set_ind_status, reset_ind_status)
    checked_state_text_option = QtCore.pyqtProperty(bool, get_state_text, set_state_text, reset_state_text)
    python_command_option = QtCore.pyqtProperty(bool, get_python_command, set_python_command, reset_python_command)
    on_color = QtCore.pyqtProperty(QtGui.QColor, get_on_color, set_on_color)
    shape_option = QtCore.pyqtProperty(int, get_shape, set_shape, reset_shape)
    off_color = QtCore.pyqtProperty(QtGui.QColor, get_off_color, set_off_color)
    flashIndicator = pyqtProperty(bool, getFlashState, setFlashState)
    flashRate = pyqtProperty(int, getFlashRate, setFlashRate)
    indicator_size = QtCore.pyqtProperty(float, get_indicator_size, set_indicator_size, reset_indicator_size)
    circle_diameter = QtCore.pyqtProperty(int, get_circle_diameter, set_circle_diameter, reset_circle_diameter)
    right_edge_offset = QtCore.pyqtProperty(int, get_right_edge_offset, set_right_edge_offset, reset_right_edge_offset)
    top_edge_offset = QtCore.pyqtProperty(int, get_top_edge_offset, set_top_edge_offset, reset_top_edge_offset)
    corner_radius = QtCore.pyqtProperty(float, get_corner_radius, set_corner_radius, reset_corner_radius)
    height_fraction = QtCore.pyqtProperty(float, get_h_fraction, set_h_fraction, reset_h_fraction)
    width_fraction = QtCore.pyqtProperty(float, get_w_fraction, set_w_fraction, reset_w_fraction)
    true_state_string = QtCore.pyqtProperty(str, get_true_string, set_true_string, reset_true_string)
    false_state_string = QtCore.pyqtProperty(str, get_false_string, set_false_string, reset_false_string)
    true_python_cmd_string = QtCore.pyqtProperty(str, get_true_python_command, set_true_python_command, reset_true_python_command)
    false_python_cmd_string = QtCore.pyqtProperty(str, get_false_python_command, set_false_python_command, reset_false_python_command)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the QtCore.pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_status_properties makes it so we can only select one option
    ########################################################################

    def _toggle_status_properties(self, picked):
        data = ('is_paused', 'is_estopped', 'is_on', 'is_idle', 'is_homed',
                'is_flood', 'is_mist', 'is_block_delete', 'is_optional_stop',
                'is_joint_homed', 'is_limits_overridden','is_manual',
                'is_mdi', 'is_auto', 'is_spindle_stopped', 'is_spindle_fwd',
                'is_spindle_rev')

        for i in data:
            if not i == picked:
                self[i+'_status'] = False

# property getter/setters

    # invert status
    def set_invert_status(self, data):
        self._invert_status = data
    def get_invert_status(self):
        return self._invert_status
    def reset_invert_status(self):
        self._invert_status = False

    # machine is paused status
    def set_is_paused(self, data):
        self._is_paused = data
        if data:
            self._toggle_status_properties('is_paused')
    def get_is_paused(self):
        return self._is_paused
    def reset_is_paused(self):
        self._is_paused = False

    # machine is estopped status
    def set_is_estopped(self, data):
        self._is_estopped = data
        if data:
            self._toggle_status_properties('is_estopped')
    def get_is_estopped(self):
        return self._is_estopped
    def reset_is_estopped(self):
        self._is_estopped = False

    # machine is on status
    def set_is_on(self, data):
        self._is_on = data
        if data:
            self._toggle_status_properties('is_on')
    def get_is_on(self):
        return self._is_on
    def reset_is_on(self):
        self._is_on = False

    # machine is idle status
    def set_is_idle(self, data):
        self._is_idle = data
        if data:
            self._toggle_status_properties('is_idle')
    def get_is_idle(self):
        return self._is_idle
    def reset_is_idle(self):
        self._is_idle = False

    # machine_is_homed status
    def set_is_homed(self, data):
        self._is_homed = data
        if data:
            self._toggle_status_properties('is_homed')
    def get_is_homed(self):
        return self._is_homed
    def reset_is_homed(self):
        self._is_homed = False

    # machine is_flood status
    def set_is_flood(self, data):
        self._is_flood = data
        if data:
            self._toggle_status_properties('is_flood')
    def get_is_flood(self):
        return self._is_flood
    def reset_is_flood(self):
        self._is_flood = False

    # machine is_mist status
    def set_is_mist(self, data):
        self._is_mist = data
        if data:
            self._toggle_status_properties('is_mist')
    def get_is_mist(self):
        return self._is_mist
    def reset_is_mist(self):
        self._is_mist = False

    # machine_is_block_delete status
    def set_is_block_delete(self, data):
        self._is_block_delete = data
        if data:
            self._toggle_status_properties('is_block_delete')
    def get_is_block_delete(self):
        return self._is_block_delete
    def reset_is_block_delete(self):
        self._is_block_delete = False

    # machine_is_optional_stop status
    def set_is_optional_stop(self, data):
        self._is_optional_stop = data
        if data:
            self._toggle_status_properties('is_optional_stop')
    def get_is_optional_stop(self):
        return self._is_optional_stop
    def reset_is_optional_stop(self):
        self._is_optional_stop = False

    # machine_is_joint_homed status
    def set_is_joint_homed(self, data):
        self._is_joint_homed = data
        if data:
            self._toggle_status_properties('is_joint_homed')
    def get_is_joint_homed(self):
        return self._is_joint_homed
    def reset_is_joint_homed(self):
        self._is_joint_homed = False

    # machine_is_limits_overridden status
    def set_is_limits_overridden(self, data):
        self._is_limits_overridden = data
        if data:
            self._toggle_status_properties('is_limits_overridden')
    def get_is_limits_overridden(self):
        return self._is_limits_overridden
    def reset_is_limits_overridden(self):
        self._is_limits_overridden = False

    # machine is manual status
    def set_is_manual(self, data):
        self._is_manual = data
        if data:
            self._toggle_status_properties('is_manual')
    def get_is_manual(self):
        return self._is_manual
    def reset_is_manual(self):
        self._is_manual = False

    # machine is mdi status
    def set_is_mdi(self, data):
        self._is_mdi = data
        if data:
            self._toggle_status_properties('is_mdi')
    def get_is_mdi(self):
        return self._is_mdi
    def reset_is_mdi(self):
        self._is_mdi = False

    # machine is auto status
    def set_is_auto(self, data):
        self._is_auto = data
        if data:
            self._toggle_status_properties('is_auto')
    def get_is_auto(self):
        return self._is_auto
    def reset_is_auto(self):
        self._is_auto = False

    # machine is spindle_stopped status
    def set_is_spindle_stopped(self, data):
        self._is_spindle_stopped = data
        if data:
            self._toggle_status_properties('is_spindle_stopped')
    def get_is_spindle_stopped(self):
        return self._is_spindle_stopped
    def reset_is_spindle_stopped(self):
        self._is_spindle_stopped = False

    # machine is spindle_fwd status
    def set_is_spindle_fwd(self, data):
        self._is_spindle_fwd = data
        if data:
            self._toggle_status_properties('is_spindle_fwd')
    def get_is_spindle_fwd(self):
        return self._is_spindle_fwd
    def reset_is_spindle_fwd(self):
        self._is_spindle_fwd = False

    # machine is spindle_rev status
    def set_is_spindle_rev(self, data):
        self._is_spindle_rev = data
        if data:
            self._toggle_status_properties('is_spindle_rev')
    def get_is_spindle_rev(self):
        return self._is_spindle_rev
    def reset_is_spindle_rev(self):
        self._is_spindle_rev = False

    # Non bool

    # machine_joint_number status
    def set_joint_number(self, data):
        self._joint_number = data
    def get_joint_number(self):
        return self._joint_number
    def reset_joint_number(self):
        self._joint_number = 0

    # designer will show these properties in this order:
    # BOOL
    invert_the_status = QtCore.pyqtProperty(bool, get_invert_status, set_invert_status, reset_invert_status)
    is_paused_status = QtCore.pyqtProperty(bool, get_is_paused, set_is_paused, reset_is_paused)
    is_estopped_status = QtCore.pyqtProperty(bool, get_is_estopped, set_is_estopped, reset_is_estopped)
    is_on_status = QtCore.pyqtProperty(bool, get_is_on, set_is_on, reset_is_on)
    is_idle_status = QtCore.pyqtProperty(bool, get_is_idle, set_is_idle, reset_is_idle)
    is_homed_status = QtCore.pyqtProperty(bool, get_is_homed, set_is_homed, reset_is_homed)
    is_flood_status = QtCore.pyqtProperty(bool, get_is_flood, set_is_flood, reset_is_flood)
    is_mist_status = QtCore.pyqtProperty(bool, get_is_mist, set_is_mist, reset_is_mist)
    is_block_delete_status = QtCore.pyqtProperty(bool, get_is_block_delete, set_is_block_delete, reset_is_block_delete)
    is_optional_stop_status = QtCore.pyqtProperty(bool, get_is_optional_stop, set_is_optional_stop, reset_is_optional_stop)
    is_joint_homed_status = QtCore.pyqtProperty(bool, get_is_joint_homed, set_is_joint_homed, reset_is_joint_homed)
    is_limits_overridden_status = QtCore.pyqtProperty(bool, get_is_limits_overridden, set_is_limits_overridden,
                                               reset_is_limits_overridden)
    is_manual_status = QtCore.pyqtProperty(bool, get_is_manual, set_is_manual, reset_is_manual)
    is_mdi_status = QtCore.pyqtProperty(bool, get_is_mdi, set_is_mdi, reset_is_mdi)
    is_auto_status = QtCore.pyqtProperty(bool, get_is_auto, set_is_auto, reset_is_auto)
    is_spindle_stopped_status = QtCore.pyqtProperty(bool, get_is_spindle_stopped, set_is_spindle_stopped, reset_is_spindle_stopped)
    is_spindle_fwd_status = QtCore.pyqtProperty(bool, get_is_spindle_fwd, set_is_spindle_fwd, reset_is_spindle_fwd)
    is_spindle_rev_status = QtCore.pyqtProperty(bool, get_is_spindle_rev, set_is_spindle_rev, reset_is_spindle_rev)

    # NON BOOL
    joint_number_status = QtCore.pyqtProperty(int, get_joint_number, set_joint_number, reset_joint_number)

# properties for stylesheet dynamic changes
    def setisManual(self, data):
        self._isManualProp = data
    def getisManual(self):
        return self._isManualProp
    isManual = QtCore.pyqtProperty(bool, getisManual, setisManual)

    def setisMDI(self, data):
        self._isMDIProp = data
    def getisMDI(self):
        return self._isMDIProp
    isMDI = QtCore.pyqtProperty(bool, getisMDI, setisMDI)

    def setisAuto(self, data):
        self._isAutoProp = data
    def getisAuto(self):
        return self._isAutoProp
    isAuto = QtCore.pyqtProperty(bool, getisAuto, setisAuto)

    def setisEstop(self, data):
        self._isEstopProp = data
    def getisEstop(self):
        return self._isEstopProp
    isEstop = QtCore.pyqtProperty(bool, getisEstop, setisEstop)

    def setisStateOn(self, data):
        self._isStateOnProp = data
    def getisStateOn(self):
        return self._isStateOnProp
    isStateOn = QtCore.pyqtProperty(bool, getisStateOn, setisStateOn)

    def setisHomed(self, data):
        self._isHomed = data
    def getisHomed(self):
        return self._isHomed
    isHomed = QtCore.pyqtProperty(bool, getisHomed, setisHomed)

    def setisAllHomed(self, data):
        self._isAllHomed = data
    def getisAllHomed(self):
        return self._isAllHomed
    isAllHomed = QtCore.pyqtProperty(bool, getisAllHomed, setisAllHomed)

# properties for enable/disable button on status state
    def setisAllHomedSensitive(self, data):
        self._is_all_homed_sensitive = data
    def getisAllHomedSensitive(self):
        return self._is_all_homed_sensitive
    isAllHomedSensitive = QtCore.pyqtProperty(bool, getisAllHomedSensitive, setisAllHomedSensitive)

    def setisOnSensitive(self, data):
        self._is_on_sensitive = data
    def getisOnSensitive(self):
        return self._is_on_sensitive
    isOnSensitive = QtCore.pyqtProperty(bool, getisOnSensitive, setisOnSensitive)

    def setisIdleSensitive(self, data):
        self._is_idle_sensitive = data
    def getisIdleSensitive(self):
        return self._is_idle_sensitive
    isIdleSensitive = QtCore.pyqtProperty(bool, getisIdleSensitive, setisIdleSensitive)

    def setisRunSensitive(self, data):
        self._is_run_sensitive = data
    def getisRunSensitive(self):
        return self._is_run_sensitive
    isRunSensitive = QtCore.pyqtProperty(bool, getisRunSensitive, setisRunSensitive)

    def setisRunPauseSensitive(self, data):
        self._is_run_paused_sensitive = data
    def getisRunPauseSensitive(self):
        return self._is_run_paused_sensitive
    isRunPausedSensitive = QtCore.pyqtProperty(bool, getisRunPauseSensitive, setisRunPauseSensitive)

    def setisAutoPauseSensitive(self, data):
        self._is_auto_pause_sensitive = data
    def getisAutoPauseSensitive(self):
        return self._is_auto_pause_sensitive
    isAutoPauseSensitive = QtCore.pyqtProperty(bool, getisAutoPauseSensitive, setisAutoPauseSensitive)

    def setisManSensitive(self, data):
        self._is_manual_sensitive = data
    def getisManSensitive(self):
        return self._is_manual_sensitive
    isManSensitive = QtCore.pyqtProperty(bool, getisManSensitive, setisManSensitive)

    def setisMDISensitive(self, data):
        self._is_mdi_sensitive = data
    def getisMDISensitive(self):
        return self._is_mdi_sensitive
    isMDISensitive = QtCore.pyqtProperty(bool, getisMDISensitive, setisMDISensitive)

    def setisAutoSensitive(self, data):
        self._is_auto_sensitive = data
    def getisAutoSensitive(self):
        return self._is_auto_sensitive
    isAutoSensitive = QtCore.pyqtProperty(bool, getisAutoSensitive, setisAutoSensitive)

    def setisSpindleOffSensitive(self, data):
        self._is_spindle_off_sensitive = data
    def getisSpindleOffSensitive(self):
        return self._is_spindle_off_sensitive
    isSpindleOffSensitive = QtCore.pyqtProperty(bool, getisSpindleOffSensitive, setisSpindleOffSensitive)

    def setisSpindleFwdSensitive(self, data):
        self._is_spindle_fwd_sensitive = data
    def getisSpindleFwdSensitive(self):
        return self._is_spindle_fwd_sensitive
    isSpindleFwdSensitive = QtCore.pyqtProperty(bool, getisSpindleFwdSensitive, setisSpindleFwdSensitive)

    def setisSpindleRevSensitive(self, data):
        self._is_spindle_rev_sensitive = data
    def getisSpindleRevSensitive(self):
        return self._is_spindle_rev_sensitive
    isSpindleRevSensitive = QtCore.pyqtProperty(bool, getisSpindleRevSensitive, setisSpindleRevSensitive)

    # boilder code
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)


class IndicatedToolButton(QtWidgets.QToolButton, IndicatedMixIn):
    def __init__(self, parent=None):
        super(IndicatedToolButton, self).__init__(parent)

# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    widget = IndicatedToolButton()
    widget.setCheckable(True)
    widget._shape = 4
    # this doesn't get called without qtvcp loading the widget
    widget.HAL_NAME_ = 'test'
    widget.PREFS_ = None
    widget.QTVCP_INSTANCE_ = None
    widget.THIS_INSTANCE_ = None
    widget.draw_indicator = True
    widget._indicator_state = True
    widget._hal_init()
    widget.show()

    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
