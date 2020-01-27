############################
# **** IMPORT SECTION **** #
############################
import sys
import os
import linuxcnc
import hal

from PyQt5 import QtCore, QtWidgets

from qtvcp.widgets.mdi_line import MDILine as MDI_WIDGET
from qtvcp.widgets.gcode_editor import GcodeEditor as GCODE
from qtvcp.widgets.stylesheeteditor import  StyleSheetEditor as SSE
from qtvcp.lib.keybindings import Keylookup

from qtvcp.core import Status, Action, Info

# Set up logging
from qtvcp import logger
LOG = logger.getLogger(__name__)

# Set the log level for this module
#LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

###########################################
# **** instantiate libraries section **** #
###########################################

KEYBIND = Keylookup()
STATUS = Status()
ACTION = Action()
INFO = Info()

LOG = logger.getLogger(__name__)
# Set the log level for this module
#LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

###################################
# **** HANDLER CLASS SECTION **** #
###################################

class HandlerClass:

    ########################
    # **** INITIALIZE **** #
    ########################
    # widgets allows access to  widgets from the qtvcp files
    # at this point the widgets and hal pins are not instantiated
    def __init__(self, halcomp,widgets,paths):
        self.hal = halcomp
        self.w = widgets
        self.PATH = paths
        self._big_view = -1
        self.STYLEEDITOR = SSE(widgets,paths)
        self.flag =0
        self.activeStyle = ''' { background-color: white;}'''
        self.defaultStyle = ''' { background-color: light blue;}'''
        self.activeWidgetDict = {'programPage':False,'userPage':False,'machinePage':False,
                            'tooloffsetsPage':False, 'loadPage':False,'mdiPage':False,
                            'workoffsetsPage':False,'setupPage':False}
        self.current_mode = (None,None)
        self._last_count = 0

    ##########################################
    # Special Functions called from QTVCP
    ##########################################

    # For changing functions in widgets we can 'class patch'.
    # class patching must be done before the class is instantiated.
    # 
    def class_patch__(self):
        GCODE.exitCall = self.editor_exit

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):
        STATUS.emit('play-sound','SPEAK This is a test screen for Haas styled QT lathe')
        KEYBIND.add_call('Key_F3','on_keycall_F3')
        KEYBIND.add_call('Key_F4','on_keycall_F4')
        KEYBIND.add_call('Key_F5','on_keycall_F5')
        KEYBIND.add_call('Key_F6','on_keycall_F6')
        KEYBIND.add_call('Key_F7','on_keycall_F7')
        KEYBIND.add_call('Key_F12','on_keycall_F12')

        self.pin_mpg_in = self.hal.newpin('mpg-in',hal.HAL_S32, hal.HAL_IN)
        self.pin_mpg_in.value_changed.connect(lambda s: self.external_mpg(s))

        self.pin_cycle_start_in = self.hal.newpin('cycle-start-in',hal.HAL_BIT, hal.HAL_IN)
        self.pin_cycle_start_in.value_changed.connect(lambda s: self.cycleStart(s))

        self.pin_abort = self.hal.newpin('abort',hal.HAL_BIT, hal.HAL_IN)
        self.pin_abort.value_changed.connect(lambda s: self.abort(s))

        self.pin_select_scroll = self.hal.newpin('select-scroll',hal.HAL_BIT, hal.HAL_IN)
        self.pin_select_fo = self.hal.newpin('select-feedoverride',hal.HAL_BIT, hal.HAL_IN)
        self.pin_pin_select_ro = self.hal.newpin('select-rapidoverride',hal.HAL_BIT, hal.HAL_IN)
        self.pin_select_so = self.hal.newpin('select-spindleoverride',hal.HAL_BIT, hal.HAL_IN)

    def before_loop__(self):
        STATUS.connect('state-estop',lambda q:self.w.close())
        self.w.close()

    def processed_key_event__(self,receiver,event,is_pressed,key,code,shift,cntrl):
        # when typing in MDI, we don't want keybinding to call functions
        # so we catch and process the events directly.
        # We do want ESC, F1 and F2 to call keybinding functions though
        if code not in(QtCore.Qt.Key_Escape,QtCore.Qt.Key_F1 ,QtCore.Qt.Key_F2,
                    QtCore.Qt.Key_F3,QtCore.Qt.Key_F5,QtCore.Qt.Key_F5):
            raise

        # ok if we got here then try keybindings
        try:
            return KEYBIND.call(self,event,is_pressed,shift,cntrl)
        except NameError as e:
            LOG.debug('Exception in KEYBINDING: {}'.format (e))
        except Exception as e:
            LOG.debug('Exception in KEYBINDING:', exc_info=e)
            print 'Error in, or no function for: %s in handler file for-%s'%(KEYBIND.convert(event),key)
            return False

    ########################
    # callbacks from STATUS #
    ########################

    #######################
    # callbacks from form #
    #######################
    def toggle_prog(self):
        cur = self.w.mainPaneStack.currentIndex()
        if self.current_mode == ('program', 'run'):
            self.w.mainPaneStack.setCurrentIndex(1)
            self.set_active_mode('program', 'load')
        else:
            self.w.mainPaneStack.setCurrentIndex(0)
            self.set_active_mode('program', 'run')

    def toggle_MDI(self):
        self.w.mainPaneStack.setCurrentIndex(0)

        cur = self.w.widgetswitcher.currentIndex()
        if cur == 4:
            next = self.w.mdi_tab.currentIndex() +1
            if next > self.w.mdi_tab.count() - 1:
                next = 0
            self.w.mdi_tab.setCurrentIndex(next)
        else:
            self.w.widgetswitcher.setCurrentIndex(4)
            self.w.mdi_tab.setCurrentIndex(0)
        self.set_active_mode('mdi',cur)

    def toggle_setup(self):
        self.w.widgetswitcher.setCurrentIndex(3)
        self.set_active_mode('setup',None)

    def toggle_dro(self):
        cur = self.w.droPaneStack.currentIndex()
        if cur == 0:
            self.w.droPaneStack.setCurrentIndex(cur+1)
        else:
            self.w.droPaneStack.setCurrentIndex(0)


    def toggle_offsets(self):
        self.w.mainPaneStack.setCurrentIndex(0)
        cur = self.w.widgetswitcher.currentIndex()
        if cur == 2:
            self.set_active_mode('offsetPage','tool')
            self.w.widgetswitcher.setCurrentIndex(0)
        else:
            self.w.widgetswitcher.setCurrentIndex(2)
            self.set_active_mode('offsetPage','work')

    def set_edit_mode(self, num):
        if num == 2:
            self.w.gcodeeditor.editMode()
        else:
            self.w.gcodeeditor.readOnlyMode()

    def toggle_graphics(self):
        cur = self.w.mainLeftStack.currentIndex()
        if cur == 0:
            if self.w.widgetswitcher.get_current_number() == 0:
                self.w.widgetswitcher.show_default()
                self.w.mainLeftStack.setCurrentIndex(1)
            elif self.w.widgetswitcher.get_current_number() == 1:
                self.w.widgetswitcher.show_default()
                self.w.mainLeftStack.setCurrentIndex(0)
        elif cur == 1:
            self.w.mainLeftStack.setCurrentIndex(0)
            self.w.widgetswitcher.show_id_widget(1)

    #####################
    # general functions #
    #####################

    def kb_jog(self, state, joint, direction, fast = False, linear = True):
        if not STATUS.is_man_mode() or not STATUS.machine_is_on():
            return
        if linear:
            distance = STATUS.get_jog_increment()
            rate = STATUS.get_jograte()/60
        else:
            distance = STATUS.get_jog_increment_angular()
            rate = STATUS.get_jograte_angular()/60
        if state:
            if fast:
                rate = rate * 2
            ACTION.JOG(joint, direction, rate, distance)
        else:
            ACTION.JOG(joint, 0, 0, 0)


    def editor_exit(self):
        self.w.gcodeeditor.exit()

    def set_active_mode(self, mode, index):
        #print mode,index
        def update(widget):
            for key, value in self.activeWidgetDict.iteritems():
                #print mode,key,value
                if key == widget:
                    print widget
                    self.w[key].setStyleSheet('#%s%s'%(key, self.activeStyle))
                    self.activeWidgetDict[key] = True
                elif value == True:
                    print 'switch off', key
                    self.w[key].setStyleSheet('#%s%s'%(key, self.defaultStyle))
                    self.activeWidgetDict[key] = False

        if mode == 'program':
            if index =='run':
                update('programPage')
                self.w.label_mode.setText('Operation-Run Program')
            else:
                update('loadPage')
                self.w.label_mode.setText('Operation-Load Program')
        elif mode == 'setup':
            update('setupPage')
            self.w.label_mode.setText('Operation- Manual Setup')
        elif mode == 'mdi':
            update('mdiPage')
            self.w.label_mode.setText('Operation- MDI Control')
        elif mode == 'offsetPage':
            if index == 'tool':
                update('tooloffsetsPage')
            elif index == 'work':
                update('workoffsetsPage')
        else:
            print ('mode/index not recognized')
            return
        self.current_mode = (mode,index)

    def abort(self, state):
        if not state:
            return
        if STATUS.stat.interp_state == linuxcnc.INTERP_IDLE:
            self.w.close()
        else:
            ACTION.ABORT()

    def cycleStart(self, state):
        print state, self.current_mode
        if state:
            if self.current_mode[0] == 'program':
                if self.current_mode[1] == 'run':
                    print 'start cycle!', self.w.gcode_editor.get_line()
                    ACTION.RUN(line=0)
                elif self.current_mode[1] == 'load':
                    print 'load program'
                    self.w.filemanager.load()
            elif self.current_mode[0] == 'mdi':
                self.w.mdihistory.run_command()

    # MPG scolling of program or MDI history
    def external_mpg(self, count):
        diff = count - self._last_count
        if self.pin_select_scroll.get():
            if self.current_mode[0] == 'program':
                if self.current_mode[1] == 'run':
                    self.w.gcode_editor.jump_line(diff)
                elif self.current_mode[1] == 'load':
                    if diff <0:
                        self.w.filemanager.down()
                    else:
                        self.w.filemanager.up()
            elif self.current_mode[0] == 'mdi':
                if diff <0:
                    self.w.mdihistory.line_down()
                else:
                    self.w.mdihistory.line_up()
        elif self.pin_select_fo.get():
            scaled = (STATUS.stat.feedrate * 100 + diff)
            if scaled <0 :scaled = 0
            elif scaled > INFO.MAX_FEED_OVERRIDE:scaled = INFO.MAX_FEED_OVERRIDE
            ACTION.SET_FEED_RATE(scaled)
        elif self.pin_pin_select_ro.get():
            scaled = (STATUS.stat.rapidrate * 100 + diff)
            if scaled <0 :scaled = 0
            elif scaled > 100:scaled = 100
            ACTION.SET_RAPID_RATE(scaled)
        elif self.pin_select_so.get():
            scaled = (STATUS.stat.spindle[0]['override'] * 100 + diff)
            if scaled < INFO.MIN_SPINDLE_OVERRIDE:scaled = INFO.MIN_SPINDLE_OVERRIDE
            elif scaled > INFO.MAX_SPINDLE_OVERRIDE:scaled = INFO.MAX_SPINDLE_OVERRIDE
            ACTION.SET_SPINDLE_RATE(scaled)
        self._last_count = count

    #####################
    # KEY BINDING CALLS #
    #####################

    # Machine control
    def on_keycall_ESTOP(self,event,state,shift,cntrl):
        if state:
            ACTION.SET_ESTOP_STATE(STATUS.estop_is_clear())
    def on_keycall_POWER(self,event,state,shift,cntrl):
        if state:
            ACTION.SET_MACHINE_STATE(not STATUS.machine_is_on())
    def on_keycall_HOME(self,event,state,shift,cntrl):
        if state:
            if STATUS.is_all_homed():
                ACTION.SET_MACHINE_UNHOMED(-1)
            else:
                ACTION.SET_MACHINE_HOMING(-1)
    def on_keycall_ABORT(self,event,state,shift,cntrl):
        if state:
            self.abort(state)

    # dialogs
    def on_keycall_F3(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'ORIGINOFFSET'})
    def on_keycall_F4(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'CAMVIEW'})
    def on_keycall_F5(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'MACROTAB'})
    def on_keycall_F6(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'TOOLOFFSET'})
    def on_keycall_F7(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'VERSAPROBE'})
    def on_keycall_F12(self,event,state,shift,cntrl):
        if state:
            self.STYLEEDITOR.load_dialog()


    # Linear Jogging
    def on_keycall_XPOS(self,event,state,shift,cntrl):
        self.kb_jog(state, 0, 1, shift)

    def on_keycall_XNEG(self,event,state,shift,cntrl):
        self.kb_jog(state, 0, -1, shift)

    def on_keycall_YPOS(self,event,state,shift,cntrl):
        self.kb_jog(state, 1, 1, shift)

    def on_keycall_YNEG(self,event,state,shift,cntrl):
        self.kb_jog(state, 1, -1, shift)

    def on_keycall_ZPOS(self,event,state,shift,cntrl):
        self.kb_jog(state, 2, 1, shift)

    def on_keycall_ZNEG(self,event,state,shift,cntrl):
        self.kb_jog(state, 2, -1, shift)

    def on_keycall_APOS(self,event,state,shift,cntrl):
        pass
        #self.kb_jog(state, 3, 1, shift, False)

    def on_keycall_ANEG(self,event,state,shift,cntrl):
        pass
        #self.kb_jog(state, 3, -1, shift, linear=False)

    ###########################
    # **** closing event **** #
    ###########################
    def closing_cleanup__(self):
        pass

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

################################
# required handler boiler code #
################################

def get_handlers(halcomp,widgets,paths):
     return [HandlerClass(halcomp,widgets,paths)]
