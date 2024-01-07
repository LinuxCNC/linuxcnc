print('\nUser command file for pause pin found\n')

# import the handlerfile to get reference to it's libraries.
# use <screenname>_handler
import qtdragon_handler as SELF

# needed to instance patch
# reference: https://ruivieira.dev/python-monkey-patching-for-readability.html
import types

############################################
# add new functions for new HAL pins to call
############################################

# external pin will call this
def external_pause_changed(data):
    # only if runnung a program
    if not SELF.STATUS.is_auto_running():
        return
    # only on true state of pin
    # mean can only pause with the external pin
    # toggle might be nicer...
    if data:
        # set pause button
        self.w.action_pause.setChecked(data)
        self.w.action_pause.clicked.emit(data)

        # set spindle pause button
        self.w.btn_spindle_pause.setChecked(data)
        self.w.btn_spindle_pause.clicked.emit(data)

# unpause machine if external offsets state is false 
def external_offset_state_changed(data):
    # only if running a program
    if not SELF.STATUS.is_auto_running():
        return
    # only if machine in on
    if not SELF.STATUS.machine_is_on():
        return
    # only if pin is false
    if not data:
        print('automatic unpause')
        # set pause button
        self.w.action_pause.setChecked(data)
        self.w.action_pause.clicked.emit(data)

def spindle_setting():
    SELF.STATUS.stat.poll()
    s = SELF.STATUS.stat.settings[2]
    if s>0:
        SELF.INFO['DEFAULT_SPINDLE_0_SPEED'] = s
    print(SELF.INFO['DEFAULT_SPINDLE_0_SPEED'])

###############################################
# add hook to make HAL pins at the right time
###############################################

# add a HAL pin with a call back
def after_override__(self):
    try:
        pin = SELF.QHAL.newpin("external-pause", SELF.QHAL.HAL_BIT, SELF.QHAL.HAL_IN)
        pin.value_changed.connect(external_pause_changed)

        pin = SELF.QHAL.newpin("external-eoffset-state", SELF.QHAL.HAL_BIT, SELF.QHAL.HAL_IN)
        pin.value_changed.connect(external_offset_state_changed)

        SELF.STATUS.connect('periodic', lambda w:spindle_setting())
    except Exception as e:
        print(e)

# Here we are instance patching the original handler file to add a new
# function that calls our new function (of the same name)
# defined in this file
self.after_override__ = types.MethodType(after_override__, self)

#########################################
# override disable_spindle_pause method
#########################################
def disable_spindle_pause(self):
    if self.w.action_pause.isChecked():
        # set spindle pause button
        self.w.btn_spindle_pause.setChecked(True)
        self.w.btn_spindle_pause.clicked.emit(True)

# instance patch voodoo
self.disable_spindle_pause = types.MethodType(disable_spindle_pause, self)

#####################################
# override pause key binding method
#####################################
def on_keycall_pause(self,event,state,shift,cntrl):
    if state and SELF.STATUS.is_auto_mode() and self.use_keyboard():
        if not SELF.STATUS.stat.paused:
            external_pause_changed(state)
        else:
            self.w.btn_spindle_pause.setChecked(False)
            self.w.btn_spindle_pause.clicked.emit(False)

# instance patch voodoo
self.on_keycall_pause = types.MethodType(on_keycall_pause, self)

##############################################
# modify stop button to call an MDI command
##############################################
self.w.action_abort.setProperty('abort_action',False)
self.w.action_abort.setProperty('true_python_cmd_string', 'ACTION.ABORT();ACTION.CALL_INI_MDI(0)')

