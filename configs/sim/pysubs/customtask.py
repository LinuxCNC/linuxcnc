import re
import os

import emctask
import emccanon
import hal

try:
    import cPickle as pickle
except ImportError:
    import pickle

try:
    from userfuncs import UserFuncs
except ImportError:
    from nulluserfuncs import UserFuncs



def debug():
    return interpreter.this.debugmask &  0x00040000 # EMC_DEBUG_PYTHON_TASK


# support queuing calls to Python methods:
# trap call, pickle a tuple of name and arguments and enqueue with canon IO_PLUGIN_CALL
class EnqueueCall(object):
    def __init__(self,e):
        print "EnqueueCall.__init__()"
        self._e = e

    def _encode(self,*args,**kwargs):
        if hasattr(self._e,self._name) and callable(getattr(self._e,self._name)):
            p = pickle.dumps((self._name,args,kwargs)) # ,-1) # hm, binary wont work just yet
            emccanon.IO_PLUGIN_CALL(int(len(p)),p)
        else:
            raise AttributeError,"no such Task method: " + self._name

    def __getattr__(self, name):
        self._name = name
        return self._encode


class EmcToolTable(object):
    ttype = { 'T' : int, 'P': int, 'Q':int,
              'X' : float, 'Y' : float, 'Z' : float,
              'A' : float, 'B' : float, 'C' : float,
              'U' : float, 'V' : float, 'W' : float,
              'I' : float, 'J' : float, 'D' : float }

    def __init__(self,filename,random_toolchanger):
         self.filename = filename
         self.comments = dict()
         self.random_toolchanger = random_toolchanger

    def assign(self,tooltable,entry):
        if self.random_toolchanger:
            pocket = entry['P']
        else:
            self.fakepocket += 1
            pocket = self.fakepocket

        tooltable[pocket].zero()
        for (key,value) in entry.items():
            if key == 'T' : tooltable[pocket].toolno = value
            if key == 'Q' : tooltable[pocket].orientation = value
            if key == 'D' : tooltable[pocket].diameter = value
            if key == 'I' : tooltable[pocket].frontangle = value
            if key == 'J' : tooltable[pocket].backangle = value
            if key == 'X' : tooltable[pocket].offset.x = value
            if key == 'Y' : tooltable[pocket].offset.y = value
            if key == 'Z' : tooltable[pocket].offset.z = value
            if key == 'A' : tooltable[pocket].offset.a = value
            if key == 'B' : tooltable[pocket].offset.b = value
            if key == 'C' : tooltable[pocket].offset.c = value
            if key == 'U' : tooltable[pocket].offset.u = value
            if key == 'V' : tooltable[pocket].offset.v = value
            if key == 'W' : tooltable[pocket].offset.w = value
            if key == 'comment' : self.comments[pocket] = value # aaargh

    def load(self, tooltable):
        self.fakepocket = 0
        fp = open(self.filename)
        lno = 0
        for line in fp.readlines():
            lno += 1
            if not line.startswith(';'):
                if line.strip():
                    entry = self.parseline(lno,line.strip())
                    if entry:
                        self.assign(tooltable,entry)
        fp.close()

    def parseline(self,lineno,line):
        """
        read a tooltable line
        if an entry was parsed successfully, return a  Tool() instance
        """
        if re.match('\A\s*T\d+',line): # an MG line
            semi = line.find(";")
            comment = line[semi+1:]
            entry = line.split(';')[0]
            gd = dict()
            for field in entry.split():
                (name,value)  = re.search('([a-zA-Z])([+-]?\d*\.?\d*)',field).groups()
                if name:
                    key = name.upper()
                    gd[key] = EmcToolTable.ttype[key](value)
                else:
                    print "%s:%d  bad line: '%s' " % (self.filename, lineno, entry)
            gd['comment'] = comment
            return gd
        print "%s:%d: unrecognized tool table entry   '%s'" % (self.filename,lineno,line)

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

class CustomTask(emctask.Task,UserFuncs):
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
        h.newpin("tool-number", hal.HAL_S32, hal.HAL_OUT)
        h.newpin("tool-prep-number", hal.HAL_S32, hal.HAL_OUT)
        h.newpin("tool-prep-pocket", hal.HAL_S32, hal.HAL_OUT)
        h.newpin("tool-prepare", hal.HAL_BIT, hal.HAL_OUT)
        h.newpin("tool-prepared", hal.HAL_BIT, hal.HAL_IN)
        h.ready()
        self.components = dict()
        self.components["iocontrol.0"] = h
        self.hal = h
        self.hal_init_pins()
        self.e = emctask.emcstat
        self.e.io.aux.estop = 1
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        self._callback = None
        self._check = None
        UserFuncs.__init__(self)
        self.enqueue = EnqueueCall(self)
        print "Py CustomTask.init"

    def emcIoInit(self):
        print "py:  emcIoInit"
        self.tt = EmcToolTable(self.tooltable_filename, self.random_toolchanger)
        self.tt.load(self.e.io.tool.toolTable)
        print "tt file=",self.tt

        # on nonrandom machines, always start by assuming the spindle is empty
        if not self.random_toolchanger:
             self.e.io.tool.toolTable[0].zero()

        self.e.io.aux.estop = 1
        self.e.io.tool.pocketPrepped = -1;
        self.e.io.tool.toolInSpindle = 0;
        self.e.io.coolant.mist = 0
        self.e.io.coolant.flood = 0
        self.e.io.lube.on = 0
        self.e.io.lube.level = 1

        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcToolLoadToolTable(self,file):
        print "py:  emcToolLoadToolTable file =",file
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def prepare_complete(self):
        print "prepare complete"
        self.e.io.tool.pocketPrepped = self.hal["tool-prep-pocket"]
        self.hal["tool-prepare"] = 0
        pass

    def emcToolPrepare(self,p,tool):
        print "py:   emcToolPrepare p =",p,"tool =",tool
        if self.random_toolchanger and (p == 0):
            print "it doesn't make sense to prep the spindle pocket"
            return 0

        self.hal["tool-prep-pocket"] = p
        if not self.random_toolchanger and (p == 0):
            self.hal["tool-prep-number"] = 0
        else:
            self.hal["tool-prep-number"] = self.e.io.tool.toolTable[p].toolno

        self.hal["tool-prepare"] = 1
        # and tell task to wait until status changes to RCS_DONE
        self.e.io.status =  self.wait_for_named_pin(1,"iocontrol.0.tool-prepared",self.prepare_complete)
        return 0

    def load_tool(self,pocket):
        if self.random_toolchanger:
            print "FIXME" # pass # swap tt0,ttpock,comments,savetooltable
        else:
            if pocket == 0:
                self.e.io.tool.toolTable[0].zero()
            else:
                # self.e.io.tool.toolTable[0].assign(self.e.io.tool.toolTable[pocket])
                self.e.io.tool.toolTable[0] = self.e.io.tool.toolTable[pocket]

    def change_complete(self):
        print "change complete"
        if not self.random_toolchanger and (self.e.io.tool.pocketPrepped == 0):
            self.e.io.tool.toolInSpindle = 0
        else:
            self.e.io.tool.toolInSpindle = self.e.io.tool.toolTable[self.e.io.tool.pocketPrepped].toolno
        self.hal["tool-number"] = self.e.io.tool.toolInSpindle
        self.load_tool(self.e.io.tool.pocketPrepped)
        self.e.io.tool.pocketPrepped = -1
        self.hal["tool-prep-number"] = 0
        self.hal["tool-prep-pocket"] = 0
        self.hal["tool-change"] = 0
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcToolLoad(self):
        print "py:  emcToolLoad"

        if self.random_toolchanger and (self.e.io.tool.pocketPrepped == 0):
            self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
            return 0

        if not self.random_toolchanger and (self.e.io.tool.pocketPrepped > 0) and self.e.io.tool.toolInSpindle == self.e.io.tool.toolTable[self.e.io.tool.pocketPrepped].toolno:
            self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
            return 0

        if self.e.io.tool.pocketPrepped != -1:
            self.hal["tool-change"] = 1
            self.e.io.status =  self.wait_for_named_pin(1,"iocontrol.0.tool-changed",self.change_complete)
            return 0
        return 0

    def emcToolUnload(self):
        print "py:  emcToolUnload"
        self.e.io.tool.toolInSpindle = 0
        # this isnt in ioControlv1, but I think it should be.
        self.hal["tool-number"] = self.e.io.tool.toolInSpindle
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcToolSetNumber(self,number):
        print "py:   emcToolSetNumber number =",number
        self.e.io.tool.toolInSpindle = number
        self.hal["tool-number"] = self.e.io.tool.toolInSpindle
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcToolSetOffset(self,pocket,toolno,offset,diameter,frontangle,backangle,orientation):
        print "py:  emcToolSetOffset", pocket,toolno,str(offset),diameter,frontangle,backangle,orientation

        self.e.io.tool.toolTable[pocket].toolno = toolno
        self.e.io.tool.toolTable[pocket].orientation = orientation
        self.e.io.tool.toolTable[pocket].diameter = diameter
        self.e.io.tool.toolTable[pocket].frontangle = frontangle
        self.e.io.tool.toolTable[pocket].backangle = backangle
        self.e.io.tool.toolTable[pocket].offset = offset

        #print "result: ",str(self.e.io.tool.toolTable[pocket])
        if self.e.io.tool.toolInSpindle  == toolno:
            self.e.io.tool.toolTable[0] = self.e.io.tool.toolTable[pocket]
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0


    def emcIoPluginCall(self, len, msg):
        print "py: emcIoPluginCall" # ,msg
        call = pickle.loads(msg)
        func = getattr(self, call[0], None)
        if func:
            self.e.io.status = func(*call[1],**call[2])
        else:
            raise AttributeError, "no such method: " + call[0]
        return 0


    def emcIoHalt(self):
        print "py:  emcIoHalt"
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcIoAbort(self,reason):
        print "py:  emcIoAbort reason=",reason,"state=",self.e.task.state
        self.hal["coolant-mist"] = 0
        self.e.io.coolant.mist = 0
        self.hal["coolant-flood"] = 0
        self.e.io.coolant.flood = 0
        self.hal["tool-change"] = 0
        self.hal["tool-prepare"] = 0
        if self._callback:
            print "emcIoAbort: cancelling callback to ",self._callback
            self._callback = None
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcToolStartChange(self):
        print "py:  emcToolStartChange NIY"
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcAuxEstopOn(self):
        print "py:  emcAuxEstopOn taskstate=",self.e.task.state
        self.hal["user-enable-out"] = 0
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcAuxEstopOff(self):
        print "py:  emcAuxEstopOff"
        self.hal["user-enable-out"] = 1
        self.hal["user-request-enable"] = 1
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcCoolantMistOn(self):
        print "py:  emcCoolantMistOn"
        self.hal["coolant-mist"] = 1
        self.e.io.coolant.mist = 1
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcCoolantMistOff(self):
        print "py:  emcCoolantMistOff"
        self.hal["coolant-mist"] = 0
        self.e.io.coolant.mist = 0
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcCoolantFloodOn(self):
        print "py:  emcCoolantFloodOn"
        self.hal["coolant-flood"] = 1
        self.e.io.coolant.flood = 1
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcCoolantFloodOff(self):
        print "py:  emcCoolantFloodOff"
        self.hal["coolant-flood"] = 0
        self.e.io.coolant.flood = 0
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcLubeOn(self):
        print "py:  emcLubeOn"
        self.hal["lube"] = 1
        self.e.io.lube.on = 1
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcLubeOff(self):
        print "py:  emcLubeOff"
        self.hal["lube"] = 0
        self.e.io.lube.on = 0
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcIoSetDebug(self,debug):
        print "py:   emcIoSetDebug debug =",debug
        self.e.io.status  = emctask.RCS_STATUS.RCS_DONE
        return 0

    def emcIoUpdate(self):
        #        print "py:  emcIoUpdate"
        self.hal["user-request-enable"] = 0
        self.e.io.aux.estop = not self.hal["emc-enable-in"]
        if self._check:
            self.e.io.status  = self._check()
        return 0

    def wait_for_named_pin_callback(self):
        if self._comp[self._pin] == self._value:
            print "wait CB done"
            if self._callback: self._callback()
            self._check = None
            self._callback = None
            return emctask.RCS_STATUS.RCS_DONE
        return emctask.RCS_STATUS.RCS_EXEC

    def wait_for_named_pin(self,value,name,callback = None):
        print "wait_for_named_pin ",value,name
        (component, pin) = name.rsplit('.',1)
        comp = self.components[component]
        if comp[pin] == value:
            print "wait_for_named_pin: already set"
            if callback: callback()
            return emctask.RCS_STATUS.RCS_DONE
        # else set up callback
        self._comp = comp
        self._pin = pin
        self._value = value
        self._check = self.wait_for_named_pin_callback
        self._callback = callback
        # and tell task to wait until status changes to RCS_DONE
        return emctask.RCS_STATUS.RCS_EXEC


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
        self.hal["tool-number"] = 0

