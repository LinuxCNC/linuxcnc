import emctask
import hal
import emc # use for ini *only*
import tooltable


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
        self.e.io.aux.estop = 1
        self.e.io.status  = emctask.RCS_DONE
        self.pin = None
        print "Py CustomTask.init"


    def emcIoInit(self):
        print "py:  emcIoInit"
        self.e.io.aux.estop = 1
        self.e.io.tool.pocketPrepped = -1;
        self.e.io.tool.toolInSpindle = 0;
        self.e.io.coolant.mist = 0
        self.e.io.coolant.flood = 0
        self.e.io.lube.on = 0
        self.e.io.lube.level = 1
        self.e.io.tool.toolTable[0].toolno = -1
        self.tt = tooltable.EmcToolTable(filename='/home/mah/emc2-tc/configs/sim/nstools-random.tbl')

        for tool in self.tt.table_entries():
            self.e.io.tool.toolTable[tool.pocket].toolno = tool.tool_number
            self.e.io.tool.toolTable[tool.pocket].offset_z = tool.offset_z
        self.e.io.status  = emctask.RCS_DONE
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


    def emcIoPluginCall(self, len, msg):
        print "py: emcIoPluginCall",len,msg
        self.pin = "tool-changed"
        self.pin_value = 1
        self.e.io.status  = emctask.RCS_EXEC
        return 0

    def emcIoUpdate(self):
        #        print "py:  emcIoUpdate"
        self.hal["user-request-enable"] = 0
        self.e.io.aux.estop = not self.hal["emc-enable-in"]
        if self.pin:
            if self.hal[self.pin] == self.pin_value:
                self.pin = None
                self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcIoHalt(self):
        print "py:  emcIoHalt"
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcIoAbort(self,reason):
        print "py:  emcIoAbort reason=",reason,"state=",self.e.task.state
        self.hal["coolant-mist"] = 0
        self.e.io.coolant.mist = 0
        self.hal["coolant-flood"] = 0
        self.e.io.coolant.flood = 0
        self.hal["tool-change"] = 0
        self.hal["tool-prepare"] = 0
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcToolStartChange(self):
        print "py:  emcToolStartChange"
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcAuxEstopOn(self):
        print "py:  emcAuxEstopOn taskstate=",self.e.task.state
        self.hal["user-enable-out"] = 0
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcAuxEstopOff(self):
        print "py:  emcAuxEstopOff"
        self.hal["user-enable-out"] = 1
        self.hal["user-request-enable"] = 1
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcCoolantMistOn(self):
        print "py:  emcCoolantMistOn"
        self.hal["coolant-mist"] = 1
        self.e.io.coolant.mist = 1
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcCoolantMistOff(self):
        print "py:  emcCoolantMistOff"
        self.hal["coolant-mist"] = 0
        self.e.io.coolant.mist = 0
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcCoolantFloodOn(self):
        print "py:  emcCoolantFloodOn"
        self.hal["coolant-flood"] = 1
        self.e.io.coolant.flood = 1
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcCoolantFloodOff(self):
        print "py:  emcCoolantFloodOff"
        self.hal["coolant-flood"] = 0
        self.e.io.coolant.flood = 0
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcLubeOn(self):
        print "py:  emcLubeOn"
        self.hal["lube"] = 1
        self.e.io.lube.on = 1
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcLubeOff(self):
        print "py:  emcLubeOff"
        self.hal["lube"] = 0
        self.e.io.lube.on = 0
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcIoSetDebug(self,debug):
        print "py:   emcIoSetDebug debug =",debug
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcToolLoadToolTable(self,file):
        print "py:  emcToolLoadToolTable file =",file
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcToolPrepare(self,p,tool):
        print "py:   emcToolPrepare p =",p,"tool =",tool
        if self.random_toolchanger and (p == 0):
            print "it doesn't make sense to prep the spindle pocket"
            return 0

        self.hal["tool-prep-pocket"] = p
        if not self.random_toolchanger and (p == 0):
            self.hal["tool-prep-number"] = 0
            prep = 0
        else:
            prep = self.e.io.tool.toolTable[p].toolno  # or tool

        self.e.io.tool.pocketPrepped = p
        self.hal["tool-prepare"] = 1
        #        self.hal["tool-prepared"] = 0
        # need rcs_exec wait now
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcToolLoad(self):
        print "py:  emcToolLoad"
        self.e.io.tool.toolInSpindle = self.e.io.tool.toolTable[self.e.io.tool.pocketPrepped].toolno
        self.e.io.tool.pocketPrepped = -1
        self.hal["tool-prep-number"] = 0
        self.hal["tool-prep-pocket"] = 0
        self.hal["tool-change"] = 0
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcToolUnload(self):
        print "py:  emcToolUnload"
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcToolSetNumber(self,number):
        print "py:   emcToolSetNumber number =",number
        self.e.io.status  = emctask.RCS_DONE
        return 0

    def emcToolSetOffset(self,pocket,toolno,offset,diameter,frontangle,backangle,orientation):
        print "py:  emcToolSetOffset", pocket,toolno,offset,diameter,frontangle,backangle,orientation
        self.e.io.status  = emctask.RCS_DONE
        return 0

