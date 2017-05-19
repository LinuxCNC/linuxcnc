import hal
import linuxcnc

DBG_state = 0
DBG_supress = True
def DBG(str):
    if not DBG_state or DBG_supress: return
    print str

""" Set of base classes """
class _WidgetBase:
    def hal_init(self, comp, name,metadata,command,widgets,dbg):
        self.hal, self.hal_name = comp, name
        self.metadata = metadata
        self.widgets = widgets
        self.cmd = command
        global DBG_supress
        global DBG_state
        DBG_state = dbg
        self.state = False
        if not 'TRUE_STATE' in self.metadata:
            self.metadata['TRUE_STATE'] = 1
        if not 'FALSE_STATE' in self.metadata:
            self.metadata['FALSE_STATE'] = 0
        # pintype and 
        if self.metadata['OUTPUT'] == 'S32':
            self.pintype=hal.HAL_S32
            self.true_state = int(self.metadata['TRUE_STATE'])
            self.false_state = int(self.metadata['FALSE_STATE'])
        elif self.metadata['OUTPUT'] == 'U32':
            self.pintype=hal.HAL_U32
            self.true_state = int(self.metadata['TRUE_STATE'])
            self.false_state = int(self.metadata['FALSE_STATE'])
        elif self.metadata['OUTPUT'] == 'FLOAT':
            self.pintype=hal.HAL_FLOAT
            self.true_state = float(self.metadata['TRUE_STATE'])
            self.false_state = float(self.metadata['FALSE_STATE'])
        elif self.metadata['OUTPUT'] == 'BIT':
            self.pintype=hal.HAL_BIT
            self.true_state = True#self.metadata['TRUE_STATE']
            self.false_state = False#self.metadata['FALSE_STATE']
        elif self.metadata['OUTPUT'] == 'COMMAND':
            self.pintype='COMMAND'
            self.true_command = (self.metadata['TRUE_COMMAND'])
            self.false_command = (self.metadata['FALSE_COMMAND'])
        else:
            self.pintype = None
        # require a status pin?
        try:
            if self.metadata['STATUS_PIN'].lower() == 'true':
                self.status_pin = True
            else:
                self.status_pin = False
        except:
                self.status_pin = False
        # default state
        try:
            if self.metadata['DEFAULT'].lower() == 'true':
                self.default_state = True
            else:
                self.default_state = False
        except:
                self.default_state = False
        self._hal_init()

    def _hal_init(self):
        """ Child HAL initialization functions """
        pass

    def toggle_state(self,pressed=True):
        """ Update internal button state """
        pass

    def set_state(self, state):
        self.state = state

    def hal_update(self):
        """ Update HAL state """
        pass

class _ToggleBase(_WidgetBase):
    def _hal_init(self):
        pintype = self.pintype
        try:
            self.state = self.default_state
        except:
            pass
        if pintype not in(None,'COMMAND'):
            self.hal_pin = self.hal.newpin(self.hal_name, pintype, hal.HAL_OUT)
            self.hal_pin_not = self.hal.newpin(self.hal_name + "-not", pintype, hal.HAL_OUT)
        if self.status_pin:
            self.hal_status_pin = self.hal.newpin(self.hal_name+ "-state", hal.HAL_BIT, hal.HAL_OUT)
            self.hal_status_pin_not = self.hal.newpin(self.hal_name+ "-state-not", hal.HAL_BIT, hal.HAL_OUT)
        if not pintype == 'COMMAND':
            DBG_supress = True
            self.hal_update()
            DBG_supress = False

    # This finds the function in either command class or
    # the optionally loaded custom handler class
    # If there are functions in both named the same
    # the optionally loaded one is used
    def find_method ( self, meth_name):
       # print meth_name
        try:
            if meth_name.lower() in dir(self.cmd.handler_instance):
                return self.cmd.handler_instance
            elif meth_name.lower() in dir(self.cmd):
                return self.cmd
            else:
                return None
        except Exception, e:
            print e
            return None

    def toggle_state(self,pressed):
        """ Update internal button state """
        if pressed:
            self.state = not self.state
            self.hal_update()

    def hal_update(self, *a):
        #print 'hal update',self.pintype,self.state
        if self.pintype not in(None,'COMMAND'):
            output = self.true_state if self.state else self.false_state
            output_not = self.false_state if self.state else self.true_state
            self.hal_pin.set(output)
            self.hal_pin_not.set(output_not)
            DBG( '  Button: %s\n Output: %s\n State: %s\nHAL: %s\n'%(self.hal_name,output,self.state,self.hal_pin.get()))
        elif self.pintype == 'COMMAND':
            output = self.true_command if self.state else self.false_command
            #print 'output',output
            if isinstance(output,list):
                arg1 = output[1]
                output = output[0]
                #print 'command:',output,arg1
            else:
                arg1 = None
            if not output.lower() in( 'clear','none'):
                module = self.find_method(output)
                #print module
                if not module is None:
                    #print 'found method:',output,'using arg:',arg1
                    module[output.lower()](self, arg1)
                    DBG( '  Button: %s\n Command: %s\n State: %s\n'%(self.hal_name,output,self.state))
                else:
                    print 'Unknown Command',output,self.state
        if self.status_pin:
            self.hal_status_pin.set(self.state)
            self.hal_status_pin_not.set(not self.state)

class GROUP(_WidgetBase):
    def _hal_init(self):
        pintype = self.pintype
        #self.state = self.default_state
        self.hal_pin = self.hal.newpin(self.hal_name, pintype, hal.HAL_OUT)
        self.hal_pin_not = self.hal.newpin(self.hal_name + "-not", pintype, hal.HAL_OUT)
        # preset the radio group
        i = self.metadata['DEFAULT']
        self.widgets[i].set_state(True)
        self.widgets[i].hal_update()
        self.toggle_state(i)
        DBG_supress = True
        self.hal_update()
        DBG_supress = False

    def add_list(self,grouplist):
        self.group_list = grouplist

    def toggle_state(self,skip):
        for i in self.group_list:
            if i==skip: continue
            self.widgets[i].set_state(False)
            self.widgets[i].hal_update()
        raw = float(self.widgets[skip].metadata['GROUP_OUTPUT'])
        if self.metadata['OUTPUT'] in('U32', 'S32'):
            self.output = int(raw)
        elif self.metadata['OUTPUT'] == 'FLOAT':
            self.output = float(raw)
        self.hal_update()

    def hal_update(self, *a):
        self.hal_pin.set(self.output)
        self.hal_pin_not.set(self.output * -1)
        DBG( '  group: %s\n Output: %s\n State: %s\nHAL: %s\n'%(self.hal_name,self.output,self.state,self.hal_pin.get()))

class TOGGLE_BUTTONS( _ToggleBase):
    pass

class MOMENTARY_BUTTONS( _ToggleBase):
    def toggle_state(self,pressed):
        """ Update internal button state """
        self.state = not self.state
        self.hal_update()

class RADIO_BUTTONS( _ToggleBase):
    # toggle this buttons state
    # then change all the other buttons in the group 
    def toggle_state(self,pressed):
        if pressed:
            if self.state: return
            self.state = not self.state
            self.hal_update()
            self.widgets[self.metadata['GROUP']].toggle_state(self.hal_name)


