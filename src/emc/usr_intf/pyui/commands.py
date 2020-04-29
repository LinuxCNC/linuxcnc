import linuxcnc
import os,sys
# path to the configuration the user requested
# used to see if the is local handler files to use
try:
    CONFIGPATH = os.environ['CONFIG_DIR']
    CONFIGDIR = os.path.join(CONFIGPATH, 'panelui_handler.py')
except:
    print '**** PANEL COMMAND: no panelui_handlers.py file in config directory'
    CONFIGPATH = os.path.expanduser("~")
    CONFIGDIR = os.path.join(CONFIGPATH, 'panelui_handler.py')

# constants
JOGJOINT  = 1
JOGTELEOP = 0

inifile = linuxcnc.ini(os.environ['INI_FILE_NAME'])
trajcoordinates = inifile.find("TRAJ", "COORDINATES").lower().replace(" ","")
jointcount = int(inifile.find("KINS","JOINTS"))

DBG_state = 0
def DBG(str):
    if DBG_state > 0:
        print str

# Loads user commands from a file named 'panelui_handler.py' from the config
def load_handlers(usermod,halcomp,builder,commands,master):
    hdl_func = 'get_handlers'
    mod = object = None
    def add_handler(method, f):
        if method in handlers:
            handlers[method].append(f)
        else:
            handlers[method] = [f]
    handlers = {}
    for u in usermod:
        (directory,filename) = os.path.split(u)
        (basename,extension) = os.path.splitext(filename)
        if directory == '':
            directory = '.'
        if directory not in sys.path:
            sys.path.insert(0,directory)
            DBG( 'panelui: adding import dir %s' % directory)
        try:
            mod = __import__(basename)
        except ImportError,msg:
            print ("panelui: module '%s' skipped - import error: %s" %(basename,msg))
	    continue
        DBG( "panelui: module '%s' imported OK" % mod.__name__)
        try:
            # look for 'get_handlers' function
            h = getattr(mod,hdl_func,None)
            if h and callable(h):
                DBG("panelui: module '%s' : '%s' function found" % (mod.__name__,hdl_func))
                objlist = h(halcomp,builder,commands,master)
            else:
                # the module has no get_handlers() callable.
                # in this case we permit any callable except class Objects in the module to register as handler
                DBG("panelui: module '%s': no '%s' function - registering only functions as callbacks" % (mod.__name__,hdl_func))
                objlist =  [mod]
            # extract callback candidates
            for object in objlist:
                #DBG("Registering handlers in module %s object %s" % (mod.__name__, object))
                if isinstance(object, dict):
                    methods = dict.items()
                else:
                    methods = map(lambda n: (n, getattr(object, n, None)), dir(object))
                for method,f in methods:
                    if method.startswith('_'):
                        continue
                    if callable(f):
                        DBG("panelui: Register callback '%s' in %s" % (method, basename))
                        add_handler(method, f)
        except Exception, e:
            print ("**** PANELUI ERROR: trouble looking for handlers in '%s': %s" %(basename, e))

    # Wrap lists in Trampoline, unwrap single functions
    for n,v in list(handlers.items()):
        if len(v) == 1:
            handlers[n] = v[0]
        else:
            handlers[n] = Trampoline(v)

    return handlers,mod,object

# trampoline and load_handlers are used for custom keyboard commands
class Trampoline(object):
    def __init__(self,methods):
        self.methods = methods

    def __call__(self, *a, **kw):
        for m in self.methods:
            m(*a, **kw)

# linuxcnc commands
class CNC_COMMANDS():
        def __init__(self, master):
            global DBG_state
            DBG_state = master._dbg
            self.emc = linuxcnc
            self.emcstat = linuxcnc.stat()
            self.emccommand = linuxcnc.command()
            self.return_to_mode = -1 # if not -1 return to the mode specified
            self.sb = 0;
            self.jog_velocity = 100.0/60.0
            self.angular_jog_velocity = 3600/60
            self._mdi = 0
            self.isjogging = [0,0,0,0,0,0,0,0,0]
            self.restart_line_number = self.restart_reset_line = 0
            try:
                handlers,self.handler_module,self.handler_instance = \
                load_handlers([CONFIGDIR], self.emcstat, self.emccommand,self, master)
            except Exception, e:
                print e

        def mdi_active(self, wname, m):
            self._mdi = m

        def mist_on(self, wname, b):
            self.emccommand.mist(1)

        def mist_off(self, wname, b):
            self.emccommand.mist(0)

        def flood_on(self, wname, b):
            self.emccommand.flood(1)

        def flood_off(self, wname, b):
            self.emccommand.flood(0)

        def estop(self, wname, b):
            self.emccommand.state(self.emc.STATE_ESTOP)

        def estop_reset(self, wname, b):
            self.emccommand.state(self.emc.STATE_ESTOP_RESET)

        def machine_off(self, wname, b):
            self.emccommand.state(self.emc.STATE_OFF)

        def machine_on(self, wname, b):
            self.emccommand.state(self.emc.STATE_ON)

        def home_all(self, wname, b):
            self.emccommand.mode(self.emc.MODE_MANUAL)
            self.emccommand.home(-1)

        def unhome_all(self, wname, b):
            self.emccommand.mode(self.emc.MODE_MANUAL)
            self.emccommand.unhome(-1)

        def home_selected(self, wname, joint):
            self.emccommand.mode(self.emc.MODE_MANUAL)
            self.emccommand.home(int(joint))

        def unhome_selected(self, wname, joint):
            self.emccommand.mode(self.emc.MODE_MANUAL)
            self.emccommand.unhome(int(joint))

        def jogging(self, wname, b):
            self.emccommand.mode(self.emc.MODE_MANUAL)

        def override_limits(self, wname, b):
            self.emccommand.mode(self.emc.MODE_MANUAL)
            self.emccommand.override_limits()

        def spindle_forward_adjust(self, wname, rpm=100):
            if self.get_mode() == self.emc.MODE_MDI:
                self.emccommand.mode(self.emc.MODE_MANUAL)
            speed = self.is_spindle_running()
            if  speed == 0:
                self.emccommand.spindle(1,float(rpm));
            elif speed > 0:
                self.emccommand.spindle(self.emc.SPINDLE_INCREASE)
            else:
                self.emccommand.spindle(self.emc.SPINDLE_DECREASE)

        def spindle_forward(self, wname, rpm=100):
            self.emccommand.mode(self.emc.MODE_MANUAL)
            speed = self.is_spindle_running()
            if  speed == 0:
                self.emccommand.spindle(1,float(rpm));

        def spindle_stop(self, wname, b):
            self.emccommand.mode(self.emc.MODE_MANUAL)
            self.emccommand.spindle(0);

        def spindle_reverse(self, wname, rpm=100):
            self.emccommand.mode(self.emc.MODE_MANUAL)
            speed = self.is_spindle_running()
            if  speed == 0:
                self.emccommand.spindle(-1,float(rpm));

        def spindle_reverse_adjust(self, wname, rpm=100):
            if self.get_mode() == self.emc.MODE_MDI:
                self.emccommand.mode(self.emc.MODE_MANUAL)
            speed = self.is_spindle_running()
            if  speed == 0:
                self.emccommand.spindle(-1,float(rpm));
            elif speed < 0:
                self.emccommand.spindle(self.emc.SPINDLE_INCREASE)
            else:
                self.emccommand.spindle(self.emc.SPINDLE_DECREASE)

        def spindle_faster(self, wname, b):
            self.emccommand.mode(self.emc.MODE_MANUAL)
            self.emccommand.spindle(0,self.emc.SPINDLE_INCREASE)

        def spindle_slower(self, wname, b):
            self.emccommand.mode(self.emc.MODE_MANUAL)
            self.emccommand.spindle(0,self.emc.SPINDLE_DECREASE)

        def set_linear_jog_velocity(self, wname, cmd):
            velocity = float(cmd)
            if velocity is not None:
                rate = self.jog_velocity = velocity / 60.0
                for axisnum in (0,1,2,6,7,8):
                    if self.isjogging[axisnum]:
                        jjogmode,j_or_a = self.get_jog_info(axisnum)
                        self.emccommand.jog(self.emc.JOG_CONTINUOUS, jjogmode, j_or_a, self.isjogging[i] * rate)

        def set_angular_jog_velocity(self, wname, cmd):
            angular = float(cmd)
            if velocity is not None:
                rate = self.angular_jog_velocity = angular / 60.0
                for axisnum in (3,4,5):
                    if self.isjogging[axisnum]:
                        jjogmode,j_or_a = self.get_jog_info(axisnum)
                        self.emccommand.jog(self.emc.JOG_CONTINUOUS, jjogmode, j_or_a, self.isjogging[i] * rate)

        def continuous_jog(self, wname, cmd):
            axisnum = int(cmd[0])
            jjogmode,j_or_a = self.get_jog_info(axisnum)
            direction = int(cmd[1])
            if direction == 0:
                self.isjogging[axisnum] = 0
                self.emccommand.jog(self.emc.JOG_STOP, jjogmode, j_or_a)
            else:
                if axisnum in (3,4,5):
                    rate = self.angular_jog_velocity
                else:
                    rate = self.jog_velocity
                self.isjogging[axisnum] = direction
                self.emccommand.jog(self.emc.JOG_CONTINUOUS, jjogmode, j_or_a, direction * rate)

        def incremental_jog(self, wname, cmd):
            axisnum = int(cmd[0])
            jjogmode,j_or_a = self.get_jog_info(axisnum)
            direction = int(cmd[1])
            distance = float(cmd[2])
            self.isjogging[axisnum] = direction
            if axisnum in (3,4,5):
                rate = self.angular_jog_velocity
            else:
                rate = self.jog_velocity
            self.emccommand.jog(self.emc.JOG_INCREMENT, jjogmode, axisnum, direction * rate, distance)
            self.isjogging[axisnum] = 0

        def quill_up(self, wname, cmd):
            self.emccommand.mode(self.emc.MODE_MANUAL)
            self.emccommand.wait_complete()
            self.mdi('G53 G0 Z %f'% float(cmd))

        def feed_hold(self, wname, cmd):
            self.emccommand.set_feed_hold(int(cmd))

        def feed_override(self, wname, f):
            self.emccommand.feedrate(f)

        def rapid_override(self, wname, f):
            self.emccommand.rapidrate(f)

        def spindle_override(self, wname, s):
            self.emccommand.spindleoverride(0,s)

        def max_velocity(self, wname, m):
            self.emccommand.maxvel(m)

        def reload_tooltable(self, wname, b):
            self.emccommand.load_tool_table()

        def optional_stop(self, wname, cmd):
            self.emccommand.set_optional_stop(int(cmd))

        def block_delete(self, wname, cmd):
            self.emccommand.set_block_delete(int(cmd))

        def abort(self, wname, cmd=None):
            self.emccommand.abort()

        def pause(self, wname, cmd=None):
            self.emccommand.auto(self.emc.AUTO_PAUSE)

        def resume(self, wname, cmd=None):
            self.emccommand.auto(self.emc.AUTO_RESUME)

        def single_block(self, wname, s):
            self.sb = s
            self.emcstat.poll()
            if self.emcstat.queue > 0 or self.emcstat.paused:
                # program or mdi is running
                if s:
                    self.emccommand.auto(self.emc.AUTO_PAUSE)
                else:
                    self.emccommand.auto(self.emc.AUTO_RESUME)

        # make sure linuxcnc is in AUTO mode
        # if Linuxcnc is paused then pushing cycle start will step the program
        # else the program starts from restart_line_number
        # after restarting it resets the restart_line_number to 0.
        # You must explicitily set a different restart line each time
        def smart_cycle_start(self, wname, cmd=None):
            self.emcstat.poll()
            if self.emcstat.task_mode != self.emc.MODE_AUTO:
                self.emccommand.mode(self.emc.MODE_AUTO)
                self.emccommand.wait_complete()
            self.emcstat.poll()
            if self.emcstat.paused:
                self.emccommand.auto(self.emc.AUTO_STEP)
                return
            if self.emcstat.interp_state == self.emc.INTERP_IDLE:
                print self.restart_line_number
                self.emccommand.auto(self.emc.AUTO_RUN, self.restart_line_number)
            self.restart_line_number = self.restart_reset_line

        # This restarts the program at the line specified directly (without cyscle start push)
        def re_start(self, wname, line):
            self.emccommand.mode(self.emc.MODE_AUTO)
            self.emccommand.wait_complete()
            self.emccommand.auto(self.emc.AUTO_RUN, line)
            self.restart_line_number = self.restart_reset_line

        # checks if ready for commands
        # calls MDI commands and when idle, periodic() will return to the mode it was in
        def mdi_and_return(self, wname, cmd):
            if self.ok_for_mdi():
                self.return_to_mode = self.get_mode() # for periodic()
                self.set_mdi_mode()
                if isinstance(cmd,list):
                    for i in cmd:
                        print str(i)
                        self.emccommand.mdi(str(i))
                else:
                    self.emccommand.mdi(str(cmd))

        # call MDI commands, set mode if needed
        def mdi(self, wname, cmd):
            self.set_mdi_mode()
            if isinstance(cmd,list):
                for i in cmd:
                    print str(i)
                    self.emccommand.mdi(str(i))
            else:
                self.emccommand.mdi(str(cmd))

        # set the restart line, you can the either restart directly
        # or restart on the cycle start button push
        # see above.
        # reset option allows one to change the default restart after it next restarts
        # eg while a restart dialog is open, always restart at the line it says
        # when the dialog close change the  line and reset both to zero
        def set_restart_line (self,  wname, line,reset=0):
            self.restart_line_number = line
            self.restart_reset_line = reset

        def set_manual_mode(self):
            self.emcstat.poll()
            if self.emcstat.task_mode != self.emc.MODE_MANUAL:
                self.emccommand.mode(self.emc.MODE_MANUAL)
                self.emccommand.wait_complete()

        def set_mdi_mode(self):
            self.emcstat.poll()
            if self.emcstat.task_mode != self.emc.MODE_MDI:
                self.emccommand.mode(self.emc.MODE_MDI)
                self.emccommand.wait_complete()

        def set_auto_mode(self):
            self.emcstat.poll()
            if self.emcstat.task_mode != self.emc.MODE_AUTO:
                self.emccommand.mode(self.emc.MODE_AUTO)
                self.emccommand.wait_complete()

        def get_mode(self):
            self.emcstat.poll()
            return self.emcstat.task_mode

        def ok_for_mdi(self):
            self.emcstat.poll()
            s = self.emcstat
            return not s.estop and s.enabled and s.homed and \
                (s.interp_state == self.emc.INTERP_IDLE)

        def is_spindle_running(self):
            self.emcstat.poll()
            s = self.emcstat
            if s.spindle[0]['enabled']:
                return s.spindle[0]['speed']
            else:
                return 0

        def periodic(self):
            # return mode back to preset variable, when idle
            if self.return_to_mode > -1:
                self.emcstat.poll()
                if self.emcstat.interp_state == self.emc.INTERP_IDLE:
                    self.emccommand.mode(self.return_to_mode)
                    self.return_to_mode = -1

        def __getitem__(self, item):
            return getattr(self, item)
        def __setitem__(self, item, value):
            return setattr(self, item, value)

        def get_jjogmode(self):
            self.emcstat.poll()
            if self.emcstat.motion_mode == linuxcnc.TRAJ_MODE_FREE:
                return JOGJOINT
            if self.emcstat.motion_mode == linuxcnc.TRAJ_MODE_TELEOP:
                return JOGTELEOP
            print "commands.py: unexpected motion_mode",self.emcstat.motion_mode
            return JOGTELEOP

        def jnum_for_axisnum(self,axisnum):
            if self.emcstat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
                print ("\n%s:\n  Joint jogging not supported for"
                       "non-identity kinematics"%__file__)
                return -1 # emcJogCont() et al reject neg joint/axis no.s
            jnum = trajcoordinates.index( "xyzabcuvw"[axisnum] )
            if jnum > jointcount:
                print ("\n%s:\n  Computed joint number=%d for axisnum=%d "
                       "exceeds jointcount=%d with trajcoordinates=%s"
                       %(__file__,jnum,axisnum,jointcount,trajcoordinates))
                # Note: primary gui should protect for this misconfiguration
                # decline to jog
                return -1 # emcJogCont() et al reject neg joint/axis no.s
            return jnum

        def get_jog_info (self,axisnum):
            jjogmode = self.get_jjogmode()
            j_or_a = axisnum
            if jjogmode == JOGJOINT: j_or_a = self.jnum_for_axisnum(axisnum)
            return jjogmode,j_or_a

