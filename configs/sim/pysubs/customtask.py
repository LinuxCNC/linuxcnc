import emctask
import hal

abort_reasons = {
    1 : "EMC_ABORT_TASK_EXEC_ERROR" ,
    2 : "EMC_ABORT_AUX_ESTOP" ,
    3 : "EMC_ABORT_MOTION_OR_IO_RCS_ERROR",
    4 : "EMC_ABORT_TASK_STATE_OFF",
    5 : "EMC_ABORT_TASK_STATE_ESTOP_RESET" ,
    6 : "EMC_ABORT_TASK_STATE_ESTOP" ,
    7 : "EMC_ABORT_TASK_STATE_NOT_ON" ,
    8 : "EMC_ABORT_TASK_ABORT" }

task_state = {
    1: "EMC_TASK_STATE_ESTOP",
    2: "EMC_TASK_STATE_ESTOP_RESET",
    3: "EMC_TASK_STATE_OFF",
    4: "EMC_TASK_STATE_ON" }

# startup:
#     halui.estop.is-activated = true
#
# estop off:
#     user-enable-out = true -> emc-enable-in becomes true
#     halui.estop.is-activated = false
#
# machine on:
#     halui.machine.is-on = true
#     halui.lube.is-on = true
#     iocontrol.0.lube = true

class CustomTask(emctask.Task):
    def __init__(self):
        emctask.Task.__init__(self)
        h = hal.component("iocontrol.0")
        h.newpin("coolant-flood", hal.HAL_BIT, hal.HAL_OUT)
        h.newpin("coolant-mist", hal.HAL_BIT, hal.HAL_OUT)

        h.newpin("lube-level", hal.HAL_BIT, hal.HAL_OUT)
        h.newpin("lube", hal.HAL_BIT, hal.HAL_OUT)

        h.newpin("emc-enable-in", hal.HAL_BIT, hal.HAL_IN)
        h.newpin("user-enable-out", hal.HAL_BIT, hal.HAL_OUT)
        h.newpin("user-request-enable", hal.HAL_BIT, hal.HAL_OUT)

        h.newpin("tool-change", hal.HAL_BIT, hal.HAL_OUT)
        h.newpin("tool-changed", hal.HAL_BIT, hal.HAL_IN)
        h.newpin("tool-prep-number", hal.HAL_S32, hal.HAL_OUT)
        h.newpin("tool-prep-pocket", hal.HAL_S32, hal.HAL_OUT)
        h.newpin("tool-prepare", hal.HAL_BIT, hal.HAL_OUT)
        h.newpin("tool-prepared", hal.HAL_BIT, hal.HAL_IN)
        h.ready()
        self.hal = h
        self.hal_init_pins()
        self.e = emctask.emcstat
        self.e.estop = 1
        print "Py CustomTask.init"


    def emcIoInit(self):
        print "py:  emcIoInit"
        self.e.io.estop = 1
        self.e.io.tool.pocketPrepped = -1;
        self.e.io.tool.toolInSpindle = 0;
        self.e.io.coolant.mist = 0
        self.e.io.coolant.flood = 0
        self.e.io.lube.on = 0
        self.e.io.lube.level = 1
        return 0

    def hal_init_pins(self):
        """ Sets HAL pins default values """
        self.hal["user-enable-out"] = 0
        self.hal["user-request-enable"] = 0
        self.hal["coolant-mist"] = 0
        self.hal["coolant-flood"] = 0
        self.hal["lube"] = 0
        self.hal["tool-prepare"] = 0
        self.hal["tool-prepared"] = 0
        self.hal["tool-prep-number"] = 0
        self.hal["tool-prep-pocket"] = 0
        self.hal["tool-change"] = 0

    def emcIoUpdate(self):
        #        print "py:  emcIoUpdate"
        self.hal["user-request-enable"] = 0
        self.e.io.estop = not self.hal["emc-enable-in"]
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcIoHalt(self):
        print "py:  emcIoHalt"
        return 0

    def emcIoAbort(self,reason):
        print "py:  emcIoAbort reason=",reason, abort_reasons[reason],"state=",task_state[self.e.task.state]
        self.hal["coolant-mist"] = 0
        self.e.io.coolant.mist = 0
        self.hal["coolant-flood"] = 0
        self.e.io.coolant.flood = 0
        self.hal["tool-change"] = 0
        self.hal["tool-prepare"] = 0
        return 0

    def emcToolStartChange(self):
        print "py:  emcToolStartChange"
        return 0

    def emcAuxEstopOn(self):
        print "py:  emcAuxEstopOn taskstate=",task_state[self.e.task.state]
        self.hal["user-enable-out"] = 0
        return 0

    def emcAuxEstopOff(self):
        print "py:  emcAuxEstopOff"
        self.hal["user-enable-out"] = 1
        self.hal["user-request-enable"] = 1
        return 0

    def emcCoolantMistOn(self):
        print "py:  emcCoolantMistOn"
        self.hal["coolant-mist"] = 1
        self.e.io.coolant.mist = 1
        return 0

    def emcCoolantMistOff(self):
        print "py:  emcCoolantMistOff"
        self.hal["coolant-mist"] = 0
        self.e.io.coolant.mist = 0
        return 0

    def emcCoolantFloodOn(self):
        print "py:  emcCoolantFloodOn"
        self.hal["coolant-flood"] = 1
        self.e.io.coolant.flood = 1
        return 0

    def emcCoolantFloodOff(self):
        print "py:  emcCoolantFloodOff"
        self.hal["coolant-flood"] = 0
        self.e.io.coolant.flood = 0
        return 0

    def emcLubeOn(self):
        print "py:  emcLubeOn"
        self.hal["lube"] = 1
        self.e.io.lube.on = 1
        return 0

    def emcLubeOff(self):
        print "py:  emcLubeOff"
        self.hal["lube"] = 0
        self.e.io.lube.on = 0
        return 0

    def emcIoSetDebug(self,debug):
        print "py:   emcIoSetDebug debug =",debug
        return 0

    def emcToolPrepare(self,p,tool):
        print "py:   emcToolPrepare p =",p,"tool =",tool
        return 0

    def emcToolLoad(self):
        print "py:  emcToolLoad"
        return 0

    def emcToolUnload(self):
        print "py:  emcToolUnload"
        return 0

    def emcToolSetNumber(self,number):
        print "py:   emcToolSetNumber number =",number
        return 0

    def emcToolSetOffset(self,pocket,toolno,offset,diameter,frontangle,backangle,orientation):
        print "py:  emcToolSetOffset", pocket,toolno,offset,diameter,frontangle,backangle,orientation
        return 0


    def test(self,arg):
        print "py:  test arg=",arg
        return 0
