#NOTE:
#     The legacy names *selected_pocket* and *current_pocket* actually reference
#     a sequential tooldata index for tool items loaded from a tool
#     table ([EMCIO]TOOL_TABLE) or via a tooldata database ([EMCIO]DB_PROGRAM)

# stdglue - canned prolog and epilog functions for the remappable builtin codes (T,M6,M61,S,F)
#
# we dont use argspec to avoid the generic error message of the argspec prolog and give more
# concise ones here

# cycle_prolog,cycle_epilog: generic code-independent support glue for oword sub cycles
#
# these are provided as starting point - for more concise error message you would better
# write a prolog specific for the code
#
# Usage:
#REMAP=G84.3  modalgroup=1 argspec=xyzqp prolog=cycle_prolog ngc=g843 epilog=cycle_epilog

import emccanon
from interpreter import *
from emccanon import MESSAGE
throw_exceptions = 1

# used so screens can get info.
# add this to toplevel to call it:

# import remap
# def __init__(self):
#     if self.task:
#         remap.build_hal(self)

def build_hal(self):
    import hal
    try:
        h=hal.component('remapStat')
        h.newpin("tool", hal.HAL_S32, hal.HAL_OUT)
        h.newpin("wear", hal.HAL_S32, hal.HAL_OUT)
        h.ready()
        self.hal_tool_comp = h
    except Exception as e:
        print(e)

# REMAP=S   prolog=setspeed_prolog  ngc=setspeed epilog=setspeed_epilog
# exposed parameter: #<speed>

def setspeed_prolog(self,**words):
    try:
        c = self.blocks[self.remap_level]
        if not c.s_flag:
            self.set_errormsg("S requires a value") 
            return INTERP_ERROR
        self.params["speed"] = c.s_number
    except Exception as e:
        self.set_errormsg("S/setspeed_prolog: %s)" % (e))
        return INTERP_ERROR
    return INTERP_OK

def setspeed_epilog(self,**words):
    try:
        if not self.value_returned:
            r = self.blocks[self.remap_level].executing_remap
            self.set_errormsg("the %s remap procedure %s did not return a value"
                             % (r.name,r.remap_ngc if r.remap_ngc else r.remap_py))
            return INTERP_ERROR
        if self.return_value < -TOLERANCE_EQUAL: # 'less than 0 within interp's precision'
            self.set_errormsg("S: remap procedure returned %f" % (self.return_value)) 
            return INTERP_ERROR
        if self.blocks[self.remap_level].builtin_used:
            pass
            #print "---------- S builtin recursion, nothing to do"
        else:
            self.speed = self.params["speed"]
            emccanon.enqueue_SET_SPINDLE_SPEED(self.speed)
        return INTERP_OK
    except Exception as e:
        self.set_errormsg("S/setspeed_epilog: %s)" % (e))
        return INTERP_ERROR
    return INTERP_OK    

# REMAP=F   prolog=setfeed_prolog  ngc=setfeed epilog=setfeed_epilog
# exposed parameter: #<feed>

def setfeed_prolog(self,**words):
    try:
        c = self.blocks[self.remap_level]
        if not c.f_flag:
            self.set_errormsg("F requires a value") 
            return INTERP_ERROR
        self.params["feed"] = c.f_number
    except Exception as e:
        self.set_errormsg("F/setfeed_prolog: %s)" % (e))
        return INTERP_ERROR
    return INTERP_OK    

def setfeed_epilog(self,**words):
    try:
        if not self.value_returned:
            r = self.blocks[self.remap_level].executing_remap
            self.set_errormsg("the %s remap procedure %s did not return a value"
                             % (r.name,r.remap_ngc if r.remap_ngc else r.remap_py))
            return INTERP_ERROR
        if self.blocks[self.remap_level].builtin_used:
            pass
            #print "---------- F builtin recursion, nothing to do"
        else:
            self.feed_rate = self.params["feed"]
            emccanon.enqueue_SET_FEED_RATE(self.feed_rate)
        return INTERP_OK
    except Exception as e:
        self.set_errormsg("F/setfeed_epilog: %s)" % (e))
        return INTERP_ERROR
    return INTERP_OK    

# REMAP=T   prolog=prepare_prolog ngc=prepare epilog=prepare_epilog
# exposed parameters: #<tool> #<pocket>

def prepare_prolog(self,**words):
    try:
        cblock = self.blocks[self.remap_level]
        if not cblock.t_flag:
            self.set_errormsg("T requires a tool number")
            return INTERP_ERROR
        tool  = cblock.t_number
        if tool:
            (status, pocket) = self.find_tool_pocket(tool)
            if status != INTERP_OK:
                self.set_errormsg("T%d: pocket not found" % (tool))
                return status
        else:
            pocket = -1 # this is a T0 - tool unload
        self.params["tool"] = tool
        self.params["pocket"] = pocket
        return INTERP_OK
    except Exception as e:
        self.set_errormsg("T%d/prepare_prolog: %s" % (int(words['t']), e))
        return INTERP_ERROR

def prepare_epilog(self, **words):
    try:
        if not self.value_returned:
            r = self.blocks[self.remap_level].executing_remap
            self.set_errormsg("the %s remap procedure %s did not return a value"
                             % (r.name,r.remap_ngc if r.remap_ngc else r.remap_py))
            return INTERP_ERROR
        if self.blocks[self.remap_level].builtin_used:
            #print "---------- T builtin recursion, nothing to do"
            return INTERP_OK
        else:
            if self.return_value > 0:
                self.selected_tool = int(self.params["tool"])
                self.selected_pocket = int(self.params["pocket"])
                emccanon.SELECT_TOOL(self.selected_tool)
                return INTERP_OK
            else:
                self.set_errormsg("T%d: aborted (return code %.1f)" % (int(self.params["tool"]),self.return_value))
                return INTERP_ERROR
    except Exception as e:
        self.set_errormsg("T%d/prepare_epilog: %s" % (tool,e))
        return INTERP_ERROR       

# REMAP=M6  modalgroup=6 prolog=change_prolog ngc=change epilog=change_epilog
# exposed parameters:
#    #<tool_in_spindle>
#    #<selected_tool>
#    #<current_pocket>
#    #<selected_pocket>

def change_prolog(self, **words):
    try:
        # this is relevant only when using iocontrol-v2.
        if self.params[5600] > 0.0:
            if self.params[5601] < 0.0:
                self.set_errormsg("Toolchanger hard fault %d" % (int(self.params[5601])))
                return INTERP_ERROR
            print("change_prolog: Toolchanger soft fault %d" % int(self.params[5601]))

        if self.selected_pocket < 0:
            self.set_errormsg("M6: no tool prepared")
            return INTERP_ERROR
        if self.cutter_comp_side:
            self.set_errormsg("Cannot change tools with cutter radius compensation on")
            return INTERP_ERROR
        self.params["tool_in_spindle"] = self.current_tool
        self.params["selected_tool"] = self.selected_tool
        self.params["current_pocket"] = self.current_pocket
        self.params["selected_pocket"] = self.selected_pocket
        return INTERP_OK
    except Exception as e:
        self.set_errormsg("M6/change_prolog: %s" % (e))
        return INTERP_ERROR

def change_epilog(self, **words):
    try:
        if not self.value_returned:
            r = self.blocks[self.remap_level].executing_remap
            self.set_errormsg("the %s remap procedure %s did not return a value"
                             % (r.name,r.remap_ngc if r.remap_ngc else r.remap_py))
            yield INTERP_ERROR
        # this is relevant only when using iocontrol-v2.
        if self.params[5600] > 0.0:
            if self.params[5601] < 0.0:
                self.set_errormsg("Toolchanger hard fault %d" % (int(self.params[5601])))
                yield INTERP_ERROR
            print("change_epilog: Toolchanger soft fault %d" % int(self.params[5601]))

        if self.blocks[self.remap_level].builtin_used:
            #print "---------- M6 builtin recursion, nothing to do"
            yield INTERP_OK
        else:
            if self.return_value > 0.0:
                # commit change
                self.selected_pocket =  int(self.params["selected_pocket"])
                emccanon.CHANGE_TOOL(self.selected_pocket)
                self.current_pocket = self.selected_pocket
                self.selected_pocket = -1
                self.selected_tool = -1
                # cause a sync()
                self.set_tool_parameters()
                self.toolchange_flag = True
                yield INTERP_EXECUTE_FINISH
            else:
                self.set_errormsg("M6 aborted (return code %.1f)" % (self.return_value))
                yield INTERP_ERROR
    except Exception as e:
        self.set_errormsg("M6/change_epilog: %s" % (e))
        yield INTERP_ERROR

# REMAP=M61  modalgroup=6 prolog=settool_prolog ngc=settool epilog=settool_epilog
# exposed parameters: #<tool> #<pocket>

def settool_prolog(self,**words):
    try:
        c = self.blocks[self.remap_level]
        if not c.q_flag:
            self.set_errormsg("M61 requires a Q parameter") 
            return INTERP_ERROR
        tool = int(c.q_number)
        if tool < -TOLERANCE_EQUAL: # 'less than 0 within interp's precision'
            self.set_errormsg("M61: Q value < 0") 
            return INTERP_ERROR
        (status,pocket) = self.find_tool_pocket(tool)
        if status != INTERP_OK:
            self.set_errormsg("M61 failed: requested tool %d not in table" % (tool))
            return status
        self.params["tool"] = tool
        self.params["pocket"] = pocket
        return INTERP_OK
    except Exception as e:
        self.set_errormsg("M61/settool_prolog: %s)" % (e))
        return INTERP_ERROR

def settool_epilog(self,**words):
    try:
        if not self.value_returned:
            r = self.blocks[self.remap_level].executing_remap
            self.set_errormsg("the %s remap procedure %s did not return a value"
                             % (r.name,r.remap_ngc if r.remap_ngc else r.remap_py))
            return INTERP_ERROR

        if self.blocks[self.remap_level].builtin_used:
            #print "---------- M61 builtin recursion, nothing to do"
            return INTERP_OK
        else:
            if self.return_value > 0.0:
                self.current_tool = int(self.params["tool"])
                self.current_pocket = int(self.params["pocket"])
                emccanon.CHANGE_TOOL_NUMBER(self.current_pocket)
                # cause a sync()
                self.tool_change_flag = True
                self.set_tool_parameters()
            else:
                self.set_errormsg("M61 aborted (return code %.1f)" % (self.return_value))
                return INTERP_ERROR
    except Exception as e:
        self.set_errormsg("M61/settool_epilog: %s)" % (e))
        return INTERP_ERROR

# educational alternative: M61 remapped to an all-Python handler
# demo - this really does the same thing as the builtin (non-remapped) M61
#
# REMAP=M61 modalgroup=6 python=set_tool_number

def set_tool_number(self, **words):
    try:
        c = self.blocks[self.remap_level]
        if c.q_flag:
            toolno = int(c.q_number)
        else:
            self.set_errormsg("M61 requires a Q parameter")
            return status 
        (status,pocket) = self.find_tool_pocket(toolno)
        if status != INTERP_OK:
            self.set_errormsg("M61 failed: requested tool %d not in table" % (toolno))
            return status
        if words['q'] > -TOLERANCE_EQUAL: # 'greater equal 0 within interp's precision'
            self.current_pocket = pocket
            self.current_tool = toolno
            emccanon.CHANGE_TOOL_NUMBER(pocket)
            # cause a sync()
            self.tool_change_flag = True
            self.set_tool_parameters()
            return INTERP_OK
        else:
            self.set_errormsg("M61 failed: Q=%4" % (toolno))
            return INTERP_ERROR
    except Exception as e:
        self.set_errormsg("M61/set_tool_number: %s" % (e))
        return INTERP_ERROR

_uvw = ("u","v","w","a","b","c")
_xyz = ("x","y","z","a","b","c")
# given a plane, return  sticky words, incompatible axis words and plane name
# sticky[0] is also the movement axis
_compat = {
    emccanon.CANON_PLANE_XY : (("z","r"),_uvw,"XY"),
    emccanon.CANON_PLANE_YZ : (("x","r"),_uvw,"YZ"),
    emccanon.CANON_PLANE_XZ : (("y","r"),_uvw,"XZ"),
    emccanon.CANON_PLANE_UV : (("w","r"),_xyz,"UV"),
    emccanon.CANON_PLANE_VW : (("u","r"),_xyz,"VW"),
    emccanon.CANON_PLANE_UW : (("v","r"),_xyz,"UW")}           

# extract and pass parameters from current block, merged with extra parameters on a continuation line
# keep tjose parameters across invocations
# export the parameters into the oword procedure
def cycle_prolog(self,**words):
    # self.sticky_params is assumed to have been initialized by the
    # init_stgdlue() method below
    global _compat
    try:    
        # determine whether this is the first or a subsequent call
        c = self.blocks[self.remap_level]
        r = c.executing_remap
        if c.g_modes[1] == r.motion_code:
            # first call - clear the sticky dict
            self.sticky_params[r.name] = dict()

        self.params["motion_code"] = c.g_modes[1]

        (sw,incompat,plane_name) =_compat[self.plane]
        for (word,value) in list(words.items()):
            # inject current parameters
            self.params[word] = value
            # record sticky words
            if word in sw:
                if self.debugmask & 0x00080000: print("%s: record sticky %s = %.4f" % (r.name,word,value))
                self.sticky_params[r.name][word] = value
            if word in incompat:
                return "%s: Cannot put a %s in a canned cycle in the %s plane" % (r.name, word.upper(), plane_name)

        # inject sticky parameters which were not in words:
        for (key,value) in list(self.sticky_params[r.name].items()):
            if not key in words:
                if self.debugmask & 0x00080000: print("%s: inject sticky %s = %.4f" % (r.name,key,value))
                self.params[key] = value

        if not "r" in self.sticky_params[r.name]:
            return "%s: cycle requires R word" % (r.name)
        else:
            if self.sticky_params[r.name] <= 0.0:
                return "%s: R word must be > 0 if used (%.4f)" % (r.name, words["r"])

        if "l" in words:
            # checked in interpreter during block parsing
            # if l <= 0 or l not near an int
            self.params["l"] = words["l"]
            
        if "p" in words:
            p = words["p"]
            if p < 0.0:
                return "%s: P word must be >= 0 if used (%.4f)" % (r.name, p)
            self.params["p"] = p

        if self.feed_rate == 0.0:
            return "%s: feed rate must be > 0" % (r.name)
        if self.feed_mode == INVERSE_TIME:
            return "%s: Cannot use inverse time feed with canned cycles" % (r.name)
        if self.cutter_comp_side:
            return "%s: Cannot use canned cycles with cutter compensation on" % (r.name)
        return INTERP_OK

    except Exception as e:
        raise
        return "cycle_prolog failed: %s" % (e)

# make sure the next line has the same motion code, unless overridden by a
# new G code
def cycle_epilog(self,**words):
    try:
        c = self.blocks[self.remap_level]
        self.motion_mode = c.executing_remap.motion_code # retain the current motion mode
        return INTERP_OK
    except Exception as e:
        return "cycle_epilog failed: %s" % (e)

# this should be called from TOPLEVEL __init__()
def init_stdglue(self):
    self.sticky_params = dict()

#####################################
# pure python remaps
#####################################

# REMAP=M6 python=ignore_m6
#
# m5 silently ignored
#
def ignore_m6(self,**words):
    try:
        return INTERP_OK
    except Exception as e:
        return "Ignore M6 failed: %s" % (e)

# REMAP=T python=index_lathe_tool_with_wear
#
# uses T101 for tool 1, wear 1 no M6 needed
# tool offsets for tool 1 and tool 10001 are added together.
#
def index_lathe_tool_with_wear(self,**words):
    # only run this if we are really moving the machine
    # skip this if running task for the screen
    if not self.task:
        yield INTERP_OK
    try:
        # check there is a tool number from the Gcode
        cblock = self.blocks[self.remap_level]
        if not cblock.t_flag:
            self.set_errormsg("T requires a tool number")
            yield INTERP_ERROR
        tool_raw = int(cblock.t_number)

        # interpret the raw tool number into tool and wear number
        # If it's less then 100 someone forgot to add the wear #, so we added it automatically
        # separate out tool number (tool) and wear number (wear), add 10000 to wear number
        if tool_raw <100:
            tool_raw=tool_raw*100
        tool = int(tool_raw/100)
        wear = 10000 + tool_raw % 100

        # uncomment for debugging
        #print'***tool#',cblock.t_number,'toolraw:',tool_raw,'tool split:',tool,'wear split',wear
        if tool:
            # check for tool number entry in tool file
            (status, pocket) = self.find_tool_pocket(tool)
            if status != INTERP_OK:
                self.set_errormsg("T%d: tool entry not found" % (tool))
                yield status
        else:
            tool = -1
            pocket = -1
            wear = -1
        self.params["tool"] = tool
        self.params["pocket"] = pocket
        self.params["wear"] =  wear
        try:
            self.hal_tool_comp['tool']= tool_raw
            self.hal_tool_comp['wear']= wear
        except:
            pass
        # index tool immediately to tool number
        self.selected_tool = int(self.params["tool"])
        self.selected_pocket = int(self.params["pocket"])
        emccanon.SELECT_TOOL(self.selected_tool)
        if self.selected_pocket < 0:
            self.set_errormsg("T0 not valid")
            yield INTERP_ERROR
        if self.cutter_comp_side:
            self.set_errormsg("Cannot change tools with cutter radius compensation on")
            yield INTERP_ERROR
        self.params["tool_in_spindle"] = self.current_tool
        self.params["selected_tool"] = self.selected_tool
        self.params["current_pocket"] = self.current_pocket
        self.params["selected_pocket"] = self.selected_pocket

        # change tool
        try:
            self.selected_pocket =  int(self.params["selected_pocket"])
            emccanon.CHANGE_TOOL(self.selected_pocket)
            self.current_pocket = self.selected_pocket
            self.selected_pocket = -1
            self.selected_tool = -1
            # cause a sync()
            self.set_tool_parameters()
            self.toolchange_flag = True
        except:
            self.set_errormsg("T change aborted (return code %.1f)" % (self.return_value))
            yield INTERP_ERROR

        # add tool offset
        self.execute("g43 h%d"% tool)
        # if the wear offset is specified, add it's offset
        try:
            if wear>10000:
                self.execute("g43.2 h%d"% wear)
            yield INTERP_OK
        except:
            self.set_errormsg("Tool change aborted - No wear %d entry found in tool table" %wear)
            yield INTERP_ERROR
    except:
        self.set_errormsg("Tool change aborted (return code %.1f)" % (self.return_value))
        yield INTERP_ERROR


# REMAP=M6 modalgroup=10 python=tool_probe_m6
#
# auto tool probe on m6
# move to tool change position for toolchange
# wait for acknowledge of tool change
# move to tool setter probe position
# probe tool on tool setter
# move back to tool change position
# set offsets
# based on Versaprobe remap
#
# param 5000 holds the work piece height
# param 4999 should be set to 1 if the 
# machine is based in imperial
#
# required INI settings
# (Abs coordinates/ machine based units)
#
#[CHANGE_POSITION]
#X = 5
#Y = 0
#Z = 0

#[TOOLSENSOR]
#X = 5.00
#Y = -1
#Z = -1
#PROBEHEIGHT = 2.3
#MAXPROBE =  -3
#SEARCH_VEL = 20
#PROBE_VEL = 5

def tool_probe_m6(self, **words):

    # only run this if we are really moving the machine
    # skip this if running task for the screen
    if not self.task:
        yield INTERP_OK

    IMPERIAL_BASED = not(bool(self.params['_metric_machine']))

    try:
        # we need to be in machine based units
        # if we aren't - switch
        # remember so we can switch back later
        switchUnitsFlag = False
        if bool(self.params["_imperial"]) != IMPERIAL_BASED:
            print ("not right Units: {}".format(bool(self.params["_imperial"])))
            if IMPERIAL_BASED:
                print ("switched Units to imperial")
                self.execute("G20")
            else:
                print ("switched Units to metric")
                self.execute("G21")
            switchUnitsFlag = True

        self.params["tool_in_spindle"] = self.current_tool
        self.params["selected_tool"] = self.selected_tool
        self.params["current_pocket"] = self.current_pocket
        self.params["selected_pocket"] = self.selected_pocket

        # cancel tool offset
        self.execute("G49")

        # change tool where ever we are
        # user sets toolchange position prior to toolchange
        # we will return here after

        try:
            self.selected_pocket =  int(self.params["selected_pocket"])
            emccanon.CHANGE_TOOL(self.selected_pocket)
            self.current_pocket = self.selected_pocket
            self.selected_pocket = -1
            self.selected_tool = -1
            # cause a sync()
            self.set_tool_parameters()
            self.toolchange_flag = True
        except InterpreterException as e:
            self.set_errormsg("tool_probe_m6 remap error: %s" % (e))
            yield INTERP_ERROR

        yield INTERP_EXECUTE_FINISH

        # record current position; probably should record every axis
        self.params[4990] = emccanon.GET_EXTERNAL_POSITION_X()
        self.params[4998] = emccanon.GET_EXTERNAL_POSITION_Y()
        self.params[4997] = emccanon.GET_EXTERNAL_POSITION_Z()

        try:
            # move to tool probe position (from INI)
            self.execute("G90")
            self.execute("G53 G0 X[#<_ini[TOOLSENSOR]X>] Y[#<_ini[TOOLSENSOR]Y>]")
            self.execute("G53 G0 Z[#<_ini[TOOLSENSOR]Z>]")

            # set incremental mode
            self.execute("G91")

            # course probe
            self.execute("F [#<_ini[TOOLSENSOR]SEARCH_VEL>]")
            self.execute("G38.2 Z [#<_ini[TOOLSENSOR]MAXPROBE>]")

            # Wait for results
            yield INTERP_EXECUTE_FINISH

            # FIXME if there is an error it never comes back
            # which leaves linuxcnc in g91 state
            if self.params[5070] == 0 or self.return_value > 0.0:
                self.execute("G90")
                self.set_errormsg("tool_probe_m6 remap error:")
                yield INTERP_ERROR

            # rapid up off trigger point to do it again
            if bool(self.params["_imperial"]):
                f = 0.25
            else:
                f = 4.0
            self.execute("G0 Z{}".format(f))

            self.execute("F [#<_ini[TOOLSENSOR]PROBE_VEL>]")
            self.execute("G38.2 Z-0.5")
            yield INTERP_EXECUTE_FINISH

            # FIXME if there is an error it never comes back
            # which leaves linuxcnc in g91 state
            if self.params[5070] == 0 or self.return_value > 0.0:
                self.execute("G90")
                self.set_errormsg("tool_probe_m6 remap error:")
                yield INTERP_ERROR

            # set back absolute state
            self.execute("G90")

            # return to recorded tool change position
            self.execute("G53 G0 Z[#4997]")
            yield INTERP_EXECUTE_FINISH
            self.execute("G53 G0 X[#4999] Y[#4998]")

            # adjust tool offset from calculations
            proberesult = self.params[5063]
            probeheight = self.params["_ini[TOOLSENSOR]PROBEHEIGHT"] 
            workheight = self.params[5000]

            adj = proberesult - probeheight + workheight
            self.execute("G10 L1 P#<selected_tool> Z{}".format(adj))

            # apply tool offset
            self.execute("G43")

            # if we switched units for tool change - switch back
            if switchUnitsFlag:
                if IMPERIAL_BASED:
                    self.execute("G21")
                    print ("switched Units back to metric")
                else:
                    self.execute("G20")
                    print ("switched Units back to imperial")
 
        except InterpreterException as e:
            msg = "%d: '%s' - %s" % (e.line_number,e.line_text, e.error_message)
            print (msg)
            yield INTERP_ERROR

    except:
        self.set_errormsg("tool_probe_m6 remap error." )
        yield INTERP_ERROR



