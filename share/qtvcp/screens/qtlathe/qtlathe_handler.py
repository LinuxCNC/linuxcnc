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
from qtvcp.widgets.file_manager import FileManager as FILEMGR
from qtvcp.widgets.stylesheeteditor import  StyleSheetEditor as SSE
from qtvcp.lib.keybindings import Keylookup
from qtvcp.lib.toolbar_actions import ToolBarActions
from qtvcp.lib.gcodes import GCodes

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
STYLEEDITOR = SSE()
TOOLBAR = ToolBarActions()

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
       # some global variables
        self.hal = halcomp
        self.w = widgets
        self.PATH = paths
        self._big_view = -1
        self.flag = 0
        self.activeStyle = ''' { background-color: white;}'''
        self.defaultStyle = ''' { background-color: light blue;}'''
        self.activeWidgetDict = {'programPage':False,'userPage':False,'machinePage':False,
                            'tooloffsetsPage':False, 'loadPage':False,'mdiPage':False,
                            'workoffsetsPage':False,'setupPage':False}
        self.current_mode = (None,None)
        self._last_count = 0
        self.run_time = 0
        self.time_tenths = 0
        self.timerOn = False
        self.slow_jog_factor = 10

        self.STYLEEDITOR = SSE(widgets,paths)
        self.GCODES = GCodes()

        STATUS.connect('periodic', lambda w: self.update_runtimer())
        STATUS.connect('command-running', lambda w: self.start_timer())
        STATUS.connect('command-stopped', lambda w: self.stop_timer())
        STATUS.connect("metric-mode-changed", lambda w, d: self.mode_changed(d))
        STATUS.connect('state-off', lambda w: self.w.pushbutton_metric.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.w.pushbutton_metric.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.w.pushbutton_metric.setEnabled(self.homed_on_test()))
        STATUS.connect('interp-run', lambda w: self.w.pushbutton_metric.setEnabled(False))
        STATUS.connect('all-homed', lambda w: self.w.pushbutton_metric.setEnabled(True))
        STATUS.connect('not-all-homed', lambda w, data: self.w.pushbutton_metric.setEnabled(False))

    ##########################################
    # Special Functions called from QTVCP
    ##########################################

    # For changing functions in widgets we can 'class patch'.
    # class patching must be done before the class is instantiated.
    # 
    def class_patch__(self):
        GCODE.exitCall = self.editor_exit
        # patch filemanager so we can trap single click loading
        # (doesn't work well on a touchscreen)
        # you can only load using a button defined in designer now
        # keep a reference to the original function as 'superLoad'
        FILEMGR.superLoad = FILEMGR.load
        FILEMGR.load = self.FMGRnoop

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):
        STATUS.emit('play-sound','SPEAK This is a test screen for QT lathe')
        KEYBIND.add_call('Key_F3','on_keycall_F3')
        KEYBIND.add_call('Key_F4','on_keycall_F4')
        KEYBIND.add_call('Key_F5','on_keycall_F5')
        KEYBIND.add_call('Key_F6','on_keycall_F6')
        KEYBIND.add_call('Key_F7','on_keycall_F7')
        KEYBIND.add_call('Key_F9','on_keycall_F9')
        KEYBIND.add_call('Key_F11','on_keycall_F11')
        KEYBIND.add_call('Key_F12','on_keycall_F12')
        TOOLBAR.configure_action(self.w.actionCalculatorDialog, 'calculatordialog')
        TOOLBAR.configure_submenu(self.w.menuGridSize, 'grid_size_submenu')
        TOOLBAR.configure_action(self.w.actionToolOffsetDialog, 'tooloffsetdialog')
        TOOLBAR.configure_action(self.w.actionReload, 'Reload')
        TOOLBAR.configure_statusbar(self.w.statusbar,'message_controls')
        self.w.pushbutton_metric.clicked[bool].connect(self.change_mode)

        # web view widget for SETUP SHEET page
        if self.w.web_view:
            self.w.verticalLayout_setup.addWidget(self.w.web_view)
            self.set_default_html()

        self.GCODES.setup_list()
        self.w.gcode_editor.hide()

    def before_loop__(self):
        STATUS.connect('state-estop',lambda q:self.w.close())


    def processed_key_event__(self,receiver,event,is_pressed,key,code,shift,cntrl):
        # when typing in MDI, we don't want keybinding to call functions
        # so we catch and process the events directly.
        # We do want ESC, F1 and F2 to call keybinding functions though
        if code not in(QtCore.Qt.Key_Escape,QtCore.Qt.Key_F1 ,QtCore.Qt.Key_F2,
                    QtCore.Qt.Key_F3,QtCore.Qt.Key_F5,QtCore.Qt.Key_F5,
                    QtCore.Qt.Key_F6,QtCore.Qt.Key_F7,QtCore.Qt.Key_F11,QtCore.Qt.Key_F12):
            raise

        if event.isAutoRepeat():return True

        # ok if we got here then try keybindings
        try:
            return KEYBIND.call(self,event,is_pressed,shift,cntrl)
        except NameError as e:
            LOG.debug('Exception in KEYBINDING: {}'.format (e))
        except Exception as e:
            LOG.debug('Exception in KEYBINDING:', exc_info=e)
            print('Error in, or no function for: %s in handler file for-%s'%(KEYBIND.convert(event),key))
            return False        

    

    ########################
    # callbacks from STATUS #
    ########################
    def runtime_sec_changed(self, data):
        text = "{:02d}:{:02d}:{:02d}".format(self.h['runtime_hrs'], self.h['runtime_min'], self.h['runtime_sec'])
        self.w.lbl_runtime.setText(text)

    def file_loaded(self, obj, filename):
        if filename is not None:
            self.w.progressBar.setValue(0)
            self.last_loaded_program = filename
        else:
            self.add_alarm("Filename not valid")

    def all_homed(self, obj):
        self.set_dro_homed(True)
        if self.first_turnon is True:
            self.first_turnon = False
            if self.w.chk_reload_tool.isChecked():
                STATUS.emit('update-machine-log', 'PreLoad Tool #{}: '.format(self.reload_tool), 'TIME')
                command = "M61 Q{}".format(self.reload_tool)
                ACTION.CALL_MDI(command)
            if self.last_loaded_program is not None and self.w.chk_reload_program.isChecked():
                STATUS.emit('update-machine-log', 'PreLoading NGC: ' + self.last_loaded_program, 'TIME')
                ACTION.OPEN_PROGRAM(self.last_loaded_program)
                self.w.filemanager.updateDirectoryView(self.last_loaded_program)

    def not_all_homed(self, obj, list):
        self.home_all = False
        self.w.lbl_home_all.setText("HOME\nALL")
        for i in INFO.AVAILABLE_JOINTS:
            if str(i) in list:
                axis = INFO.GET_NAME_FROM_JOINT.get(i).lower()
                try:
                    self.w["dro_axis_{}".format(axis)].setProperty('homed', False)
                    self.w["dro_axis_{}".format(axis)].setStyle(self.w["dro_axis_{}".format(axis)].style())
                except:
                    pass

    #######################
    # callbacks from form #
    #######################
    def percentLoaded(self, fraction):
        if fraction <1:
            self.w.progressBar.setValue(0)
            self.w.progressBar.setFormat('')
        else:
            self.w.progressBar.setValue(fraction)
            self.w.progressBar.setFormat('Loading: {}%'.format(fraction))

    def percentCompleted(self, fraction):
        self.w.progressBar.setValue(fraction)
        if fraction <1:
            self.w.progressBar.setFormat('')
        else:
            self.w.progressBar.setFormat('Completed: {}%'.format(fraction))

    def toggle_prog(self):
        if self.current_mode == ('program', 'run'):
            self.set_active_mode('program', 'load')
        else:
            self.set_active_mode('program', 'run')

    def toggle_MDI(self):
        self.set_active_mode('mdi',None)

    def toggle_setup(self):
        self.set_active_mode('setup',None)

    def toggle_dro(self):
        next = self.w.droPaneStack.currentIndex() +1
        if next == self.w.droPaneStack.count():
            self.w.droPaneStack.setCurrentIndex(0)
        else:
            self.w.droPaneStack.setCurrentIndex(next)
        
    def toggle_offsets(self):
        self.set_active_mode('offsetPage',None)


    def set_edit_mode(self, num):
        if num == 2:
            self.w.gcodeeditor.editMode()
        else:
            self.w.gcodeeditor.readOnlyMode()

    def toggle_graphics(self):
        self.set_active_mode('graphics', None)

    # tool tab
    def btn_m61_clicked(self):
        checked = self.w.tooloffsetview.get_checked_list()
        if len(checked) > 1:
            self.add_alarm("Select only 1 tool to load")
        elif checked:
            ACTION.CALL_MDI("M61 Q{}".format(checked[0]))
        else:
            self.add_alarm("No tool selected")

     # alarm tab
    def btn_clear_alarms_clicked(self):
        ACTION.UPDATE_MACHINE_LOG('', 'DELETE')

    def btn_save_alarms_clicked(self):
        text = self.w.machinelog.toPlainText()
        filename = self.w.lbl_clock.text().encode('utf-8')
        filename = 'alarms_' + filename.replace(' ','_') + '.txt'
        with open(filename, 'w') as f:
            f.write(text)

    def btn_reload_file_clicked(self):
        if self.last_loaded_program:
            self.w.progressBar.setValue(0)
            ACTION.OPEN_PROGRAM(self.last_loaded_program)

    def slow_jog_clicked(self, state):
        slider = self.w.sender().property('slider')
        current = self.w[slider].value()
        max = self.w[slider].maximum()
        if state:
            self.w.sender().setText("SLOW")
            self.w[slider].setMaximum(max / self.slow_jog_factor)
            self.w[slider].setValue(current / self.slow_jog_factor)
            self.w[slider].setPageStep(1)
        else:
            self.w.sender().setText("FAST")
            self.w[slider].setMaximum(max * self.slow_jog_factor)
            self.w[slider].setValue(current * self.slow_jog_factor)
            self.w[slider].setPageStep(1)

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
        self.btn_gcode_edit_clicked(False)

    def set_active_mode(self, mode, index):
        def update(widget):
            for key, value in self.activeWidgetDict.items():
                #print mode,key,value
                if key == widget:
                    print(widget)
                    self.w[key].setStyleSheet('#%s%s'%(key, self.activeStyle))
                    self.activeWidgetDict[key] = True
                elif value == True:
                    print('switch off', key)
                    self.w[key].setStyleSheet('#%s%s'%(key, self.defaultStyle))
                    self.activeWidgetDict[key] = False

        if mode == 'program':
            if index =='run':
                self.w.mainLeftStack.setCurrentIndex(0)# gcode
                self.w.mainPaneStack.setCurrentIndex(0)# normal
                update('programPage')
                self.w.label_mode.setText('Operation-Run Program')
            else:
                self.w.mainLeftStack.setCurrentIndex(0)# gcode
                self.w.mainPaneStack.setCurrentIndex(1)# load
                update('loadPage')
                self.w.label_mode.setText('Operation-Load Program')
        elif mode == 'setup':
            self.w.mainPaneStack.setCurrentIndex(0)# normal
            self.w.widgetswitcher.setCurrentIndex(3)# setup manual
            self.w.mainLeftStack.setCurrentIndex(1) # setup html
            update('setupPage')
            self.w.label_mode.setText('Operation- Manual Setup')
        elif mode == 'mdi':
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
            update('mdiPage')
            self.w.label_mode.setText('Operation- MDI Control')
        elif mode == 'offsetPage':
            self.w.mainPaneStack.setCurrentIndex(0)
            cur = self.w.widgetswitcher.currentIndex()
            if cur == 2:
                self.w.widgetswitcher.setCurrentIndex(0)
                update('tooloffsetsPage')
            else:
                self.w.widgetswitcher.setCurrentIndex(2)
                update('workoffsetsPage')
            return # don't change the mode
        elif mode == 'graphics':
            if self.current_mode[0] == 'program': # gcode
                if self.w.widgetswitcher.get_current_number() == 0:
                    self.w.widgetswitcher.show_id_widget(1)
                    self.w.mainLeftStack.setCurrentIndex(0) # program
                elif self.w.widgetswitcher.get_current_number() == 1:
                    self.w.widgetswitcher.show_default()
                    self.w.mainLeftStack.setCurrentIndex(2)
            elif self.current_mode[0] == 'setup':# setup
                if self.w.mainLeftStack.currentIndex() == 2:
                    self.w.mainLeftStack.setCurrentIndex(1)
                    self.w.widgetswitcher.setCurrentIndex(3)# setup
                # show graphics
                else:
                    self.w.mainLeftStack.setCurrentIndex(2)
                    self.w.widgetswitcher.setCurrentIndex(3)# setup
            elif self.current_mode[0] == 'mdi':# mdi
                # hide graphics
                if self.w.mainLeftStack.currentIndex() == 2:
                    self.w.mainLeftStack.setCurrentIndex(0)
                    self.w.widgetswitcher.setCurrentIndex(4)# setup mdi
                # show graphics
                else:
                    self.w.widgetswitcher.show_default()
                    self.w.mainLeftStack.setCurrentIndex(2)
                    self.w.widgetswitcher.setCurrentIndex(4)# setup mdi
            return # don;t change the mode
        else:
            print ('mode/index not recognized')
            return
        self.current_mode = (mode,index)

    def btn_start_macro_clicked(self):
            self.w.label_mode.setText('Operation- MDI Control')
            self.w.mditouchy.run_command()
            return

    def abort(self, state):
        if not state:
            return
        if STATUS.stat.interp_state == linuxcnc.INTERP_IDLE:
            self.w.close()
        else:
            ACTION.ABORT()

    def make_progressbar(self):
        self.w.progressbBar = QtWidgets.QProgressBar()
        self.w.progressBar.setRange(0,100)
        self.w.statusBar.addWidget(self.w.progressBar)


    def update_runtimer(self):
        if self.timerOn is False or STATUS.is_auto_paused(): return
        self.time_tenths += 1
        if self.time_tenths == 10:
            self.time_tenths = 0
            self.run_time += 1
            hours, remainder = divmod(self.run_time, 3600)
            minutes, seconds = divmod(remainder, 60)
            self.w.lbl_runtime.setText("{:02d}:{:02d}:{:02d}".format(hours, minutes, seconds))    

    def start_timer(self):
        self.run_time = 0
        self.timerOn = True

    def stop_timer(self):
        self.timerOn = False

    def mode_changed(self,data):
        self._block_signal = True
        self.w.pushbutton_metric.setChecked(data)
        # if using state labels option update the labels
        if self.w.pushbutton_metric._state_text:
           self.w.pushbutton_metric.setText(None)
        self._block_signal = False

    def change_mode(self, data):
        if self._block_signal: return
        if data:
            ACTION.CALL_MDI('G21')
        else:
            ACTION.CALL_MDI('G20')

    def homed_on_test(self):
        return (STATUS.machine_is_on()
            and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))

    # file tab
    def btn_gcode_edit_clicked(self, state):
        if not STATUS.is_on_and_idle():
            return
        for x in ["load", "next", "prev"]:
            self.w["btn_file_{}".format(x)].setEnabled(not state)
        if state:
            self.w.filemanager.hide()
            self.w.gcode_editor.show()
            self.w.gcode_editor.editMode()
        else:
            self.w.filemanager.show()
            self.w.gcode_editor.hide()
            self.w.gcode_editor.readOnlyMode()

    def btn_load_file_clicked(self):
        fname = self.w.filemanager.getCurrentSelected()
        if fname[1] is True:
            self.load_code(fname[0])

    # class patched filemanager to trap single click loading
    # this makes the original function do nothing
    def FMGRnoop(self, fname):
        pass
    def load_code(self, fname):
        if fname is None: return
        if fname.endswith(".ngc") or fname.endswith(".py"):
            # call original filemanager load function to load program
            self.w.filemanager.superLoad(fname)

            # change filepath extension to autoload an html setup page
            fname = os.path.splitext(fname)[0]+'.html'
        if self.w.web_view is None:
            return
        if fname.endswith(".html"):
            if os.path.exists(fname):
                self.w.web_view.load(QtCore.QUrl.fromLocalFile(fname))
                return
        self.set_default_html(fname)

    def set_default_html(self,filename=None):
        if filename is None: filename = 'No program Loaded'
        print(filename)
        self.html = """<html>
<head>
<title>Test page for the download:// scheme</title>
</head>
<body>
<h1>Setup Tab</h1>
<p> tried loading::%s</p>
<p>If there was a HTML setup file , it would auto load and be shown here..</p>
<img src="file://%s" alt="lcnc_swoop" />
<hr />

<a href="http://linuxcnc.org/docs/html/lathe/lathe-user.html">Lathe User Information link</a>
</body>
</html>
""" %(filename,os.path.join(self.PATH.IMAGEDIR,'lcnc_swoop.png'))
        self.w.web_view.setHtml(self.html)

    def add_alarm(self, message):
        STATUS.emit('update-machine-log', message, 'TIME')

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

    def on_keycall_pause(self,event,state,shift,cntrl):
        if state and STATUS.is_auto_mode() and self.use_keyboard():
            ACTION.PAUSE()

    # dialogs
    def on_keycall_F3(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'ORIGINOFFSET'})
    def on_keycall_F4(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'CAMVIEW'})
    def on_keycall_F6(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'TOOLOFFSET'})
    def on_keycall_F7(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'VERSAPROBE'})
    def on_keycall_F9(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'Calculator'})
    def on_keycall_F11(self,event,state,shift,cntrl):
        if state:
            pass
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
