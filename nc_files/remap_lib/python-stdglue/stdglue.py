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
from subprocess import PIPE, Popen
throw_exceptions = 1

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

# make sure the next line has the same motion code, unless overriden by a
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
        return INTERP_OK
    try:
        # check there is a tool number from the Gcode
        cblock = self.blocks[self.remap_level]
        if not cblock.t_flag:
            self.set_errormsg("T requires a tool number")
            return INTERP_ERROR
        tool_raw = int(cblock.t_number)

        # interpet the raw tool number into tool and wear number
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
                return status
        else:
            tool = -1
            pocket = -1
            wear = -1
        self.params["tool"] = tool
        self.params["pocket"] = pocket
        self.params["wear"] =  wear

        # index tool immediately to tool number
        self.selected_tool = int(self.params["tool"])
        self.selected_pocket = int(self.params["pocket"])
        emccanon.SELECT_TOOL(self.selected_tool)
        if self.selected_pocket < 0:
            self.set_errormsg("T0 not valid")
            return INTERP_ERROR
        if self.cutter_comp_side:
            self.set_errormsg("Cannot change tools with cutter radius compensation on")
            return INTERP_ERROR
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
            return INTERP_ERROR

        # add tool offset
        self.execute("g43 h%d"% tool)
        # if the wear offset is specified, add it's offset
        if wear>10000:
            self.execute("g43.2 h%d"% wear)
        return INTERP_OK

    except Exception as e:
        print(e)
        self.set_errormsg("T%d index_lathe_tool_with_wear: %s" % (int(words['t']), e))
        return INTERP_ERROR


# REMAP=M6 modalgroup=10 python=tool_probe_m6
#
# auto tool probe on m6
# save user units
# move to Z max
# move to EMCIO position for toolchange
# backup offset and position
# wait for acknowledge of tool change
# move to tool setter probe position
# probe tool on tool setter
# move back to tool change position
# set tool offsets using toolsetterheight and _backup_coord_offset
# move to Z max
# restore user units
#
# required INI settings
# (Abs coordinates/ machine based units)
#
# will follow these directives:
#{EMCIO]
#TOOL_CHANGE_POSITION = 0 0 0
#TOOL_CHANGE_WITH_SPINDLE_ON = 0
#TOOL_CHANGE_QUILL_UP = 1

## exemple with metric unit mm
#[TOOLSENSOR]
## Absolute coordinates of the toolsetter pad G53 machine cooordinates
#X = -40
#Y = -40
## Absolute Z start search coordinates is a move relative G0 from toolchange position
#Z = -30
## The height of you tool setter
#HEIGHT = 40
## Maximum search distance and direction (positive number but used as negative inside macro)
#MAXPROBE = 80
## Latched distance after probing use value like 1mm for standalone
## or something like 0.3mm is enough if you use REVERSE_LATCH
#LATCH = 0.3
## If setter as spring inside you can use REVERSE_LATCH with G38.5 with value like 1mm
## or if your setter does not have a spring inside (inhibit G38.5) with value 0
#REVERSE_LATCH = 1
## Fast first probe velocity
#SEARCH_VEL = 100
## Slow final probe velocity
#PROBE_VEL = 5


def tool_probe_m6(self, **words):

# only run this if we are really moving the machine
    # skip this if running task for the screen
    if not self.task:
        yield INTERP_OK

    METRIC_BASED = (bool(self.params['_metric_machine']))      #(commented out a workaround for 2.8)
#    if Popen('halcmd getp halui.machine.units-per-mm',stdout=PIPE,shell=True).communicate()[0].strip() == "1":
#        METRIC_BASED = 1
#    else:
#        METRIC_BASED = 0

#    if Popen('halcmd getp probe.use_tool_measurement',stdout=PIPE,shell=True).communicate()[0].strip() == 'FALSE':
#        BYPASS_MEASURE = 1
#    else:
#        BYPASS_MEASURE = 0

    # Saving G90 G91 at startup
    ABSOLUTE_FLAG = (bool(self.params["_absolute"]))

    # Saving feed at startup
    FEED_BACKUP = (bool(self.params["_feed"]))

    # cancel tool offset
    self.execute("G49")

    # record current position XY but Z only without toolchange_quill_up enabled
    X = emccanon.GET_EXTERNAL_POSITION_X()
    Y = emccanon.GET_EXTERNAL_POSITION_Y()
    Z = emccanon.GET_EXTERNAL_POSITION_Z()                     # restoring Z can be removed or keep this need to choosed

    # turn off all spindles if required                        #(commented out a workaround for 2.8)
    if not self.tool_change_with_spindle_on:
        for s in range(0,self.num_spindles):
            emccanon.STOP_SPINDLE_TURNING(s)

#    if not self.params["_ini[EMCIO]TOOL_CHANGE_WITH_SPINDLE_ON"]:
#        for s in range(0,int(self.params["_ini[EMCMOT]NUM_SPINDLES"])):
#            emccanon.STOP_SPINDLE_TURNING(s)

    try:
        # we need to be in machine based units
        # if we aren't - switch
        BACKUP_METRIC_FLAG = self.params["_metric"]
        if BACKUP_METRIC_FLAG != METRIC_BASED:
            print ("not right Units: {}".format(bool(self.params["_metric"])))
            if METRIC_BASED:
                print ("switched Units to metric")
                self.execute("G21")
            else:
                print ("switched Units to imperial")
                self.execute("G20")

        # Force absolute for G53 move
        self.execute("G90")

        # Z up first if required + wait finished : NEVED TRY TO safety check here or issue appears because this is before change tool #(comment out workaround for 2.8)
        if self.tool_change_quill_up:
#        if self.params["_ini[EMCIO]TOOL_CHANGE_QUILL_UP"]:
            self.execute("G53 G0 Z0")
            #yield INTERP_EXECUTE_FINISH                                                                 # sometime i will have diagonal move without waiting

        self.params["tool_in_spindle"] = self.current_tool
        self.params["selected_tool"] = self.selected_tool
        self.params["current_pocket"] = self.current_pocket
        self.params["selected_pocket"] = self.selected_pocket

        # change tool where ever we are
        # user sets toolchange position prior to toolchange
        # we will return here after only for XY
############################################################## TODO ACTUAL SYSTEM DOES NOT SHOW CONFIRMATION POPUP IF YOU M6Tx SAME TOOL AS ACTUAL
        try:
            self.selected_pocket =  int(self.params["selected_pocket"])                                # this code imo is redundant with line 578 but need review???
            emccanon.CHANGE_TOOL(self.selected_pocket)
            self.current_pocket = self.selected_pocket
            self.selected_pocket = -1
            self.selected_tool = -1
            # cause a sync()
            self.set_tool_parameters()
            self.toolchange_flag = True
        except InterpreterException as e:
            # if we switched units for tool change - switch back
            tool_probe_restore_sub(self, ABSOLUTE_FLAG, FEED_BACKUP, BACKUP_METRIC_FLAG)
            self.set_errormsg("tool_probe_m6 remap error: %s" % (e))
            yield INTERP_ERROR

        print (self.params["tool_in_spindle"], self.current_tool)
        print (self.params["selected_tool"], self.selected_tool)
        print (self.params["current_pocket"], self.current_pocket)
        print (self.params["selected_pocket"], self.selected_pocket)


        # Prevent tool measurement if M6T0 or invalid negative number
        if self.params["selected_tool"] < 0:
            self.execute("(ABORT,TOOLNUMBER NEGATIVE NOT POSSIBLE")
            tool_probe_restore_sub(self, ABSOLUTE_FLAG, FEED_BACKUP, BACKUP_METRIC_FLAG)
            yield INTERP_ERROR

        if self.params["selected_tool"] == 0:
            yield INTERP_OK

#        # bypass option for restoring simple manual mode without autolength
#        if BYPASS_MEASURE:
#            # re-apply tool offset
#            self.execute("G43")
#            yield INTERP_OK


        try:
            # move to tool setter position (from INI)
            self.execute("G90")

            self.execute("G53 G0 X#<_ini[TOOLSENSOR]X> Y#<_ini[TOOLSENSOR]Y>")
            self.execute("G53 G0 Z#<_ini[TOOLSENSOR]Z>")

            # backup G5x offset for correct tool measurement
            if self.params["_coord_system"] == 540:
                self.params["_backup_coord_offset"] = self.params[5223]
            elif self.params["_coord_system"] == 550:
                self.params["_backup_coord_offset"] = self.params[5243]
            elif self.params["_coord_system"] == 560:
                self.params["_backup_coord_offset"] = self.params[5263]
            elif self.params["_coord_system"] == 570:
                self.params["_backup_coord_offset"] = self.params[5283]
            elif self.params["_coord_system"] == 580:
                self.params["_backup_coord_offset"] = self.params[5303]
            elif self.params["_coord_system"] == 590:
                self.params["_backup_coord_offset"] = self.params[5323]
            elif self.params["_coord_system"] == 591:
                self.params["_backup_coord_offset"] = self.params[5343]
            elif self.params["_coord_system"] == 592:
                self.params["_backup_coord_offset"] = self.params[5363]
            elif self.params["_coord_system"] == 593:
                self.params["_backup_coord_offset"] = self.params[5383]
            print ("_backup_coord_offset", self.params["_backup_coord_offset"])

            # set incremental mode
            self.execute("G91")

            # Fast Search probe
            self.execute("G38.3 Z-#<_ini[TOOLSENSOR]MAXPROBE> F#<_ini[TOOLSENSOR]SEARCH_VEL>")
            # Wait for results
            yield INTERP_EXECUTE_FINISH
            # Check if we have get contact or not
            tool_probe_check_sub(self, ABSOLUTE_FLAG, FEED_BACKUP, BACKUP_METRIC_FLAG)

            if self.params["_ini[TOOLSENSOR]REVERSE_LATCH"] > 0:
                # Spring mounted latch probe
                self.execute("G38.5 Z#<_ini[TOOLSENSOR]REVERSE_LATCH> F[#<_ini[TOOLSENSOR]SEARCH_VEL>*0.5]")
                yield INTERP_EXECUTE_FINISH
                # Check if we have get contact or not
                tool_probe_check_sub(self, ABSOLUTE_FLAG, FEED_BACKUP, BACKUP_METRIC_FLAG)
                # Additional Retract
                self.execute("G1 Z#<_ini[TOOLSENSOR]LATCH> F[#<_ini[TOOLSENSOR]SEARCH_VEL>]")
            else:
                # Retract Latch
                self.execute("G1 Z#<_ini[TOOLSENSOR]LATCH> F[#<_ini[TOOLSENSOR]SEARCH_VEL>]")

            # Final probe
            self.execute("G38.3 Z-[#<_ini[TOOLSENSOR]LATCH>*2] F#<_ini[TOOLSENSOR]PROBE_VEL>")
            yield INTERP_EXECUTE_FINISH
            # Check if we have get contact or not
            tool_probe_check_sub(self, ABSOLUTE_FLAG, FEED_BACKUP, BACKUP_METRIC_FLAG)

            # Save the probe result now due to possible use of G38.5 for retract
            proberesult = self.params[5063]

            if self.params["_ini[TOOLSENSOR]REVERSE_LATCH"] > 0:
                # Spring mounted latch probe
                self.execute("G38.5 Z#<_ini[TOOLSENSOR]REVERSE_LATCH> F[#<_ini[TOOLSENSOR]SEARCH_VEL>*0.5]")
                yield INTERP_EXECUTE_FINISH
                # Check if we have get contact or not
                tool_probe_check_sub(self, ABSOLUTE_FLAG, FEED_BACKUP, BACKUP_METRIC_FLAG)

            # Force absolute for G53 move
            self.execute("G90")

            # calculations for tool offset saved in the tooltable
            adj = proberesult - self.params["_ini[TOOLSENSOR]HEIGHT"] + self.params["_backup_coord_offset"]     # Your welcome for other solution !
            self.execute("G10 L1 P#<selected_tool> Z{}".format(adj))            # REGISTER NEW TOOL LENGTH

            # apply tool offset
            self.execute("G43")

            # Z up first if required + wait finished (status test already done before)      #(commented out a workaround for 2.8)
            if self.tool_change_quill_up:
#            if self.params["_ini[EMCIO]TOOL_CHANGE_QUILL_UP"]:
                self.execute("G53 G0 Z0")
            else:                                                                           # restoring Z can be removed or keep this need to be chosen
                self.execute("G53 G0 Z{:.5f}".format(Z))

            self.execute("G53 G0 X{:.5f} Y{:.5f}".format(X,Y))

            # if we switched units for tool change - switch back and act ok to interp
            tool_probe_restore_sub(self, ABSOLUTE_FLAG, FEED_BACKUP, BACKUP_METRIC_FLAG)
            yield INTERP_OK

        except InterpreterException as e:
            # if we switched units for tool change - switch back
            tool_probe_restore_sub(self, ABSOLUTE_FLAG, FEED_BACKUP, BACKUP_METRIC_FLAG)
            msg = "%d: '%s' - %s" % (e.line_number,e.line_text, e.error_message)
            print (msg)
            yield INTERP_ERROR

    except Exception as e:
        # if we switched units for tool change - switch back
        tool_probe_restore_sub(self, ABSOLUTE_FLAG, FEED_BACKUP, BACKUP_METRIC_FLAG)
        print (e)
        self.set_errormsg("tool_probe_m6 remap error: nothing restored or unknown state : %s" % (e))
        yield INTERP_ERROR

# Check if we have get contact or not
def tool_probe_check_sub(self, ABSOLUTE_FLAG, FEED_BACKUP, BACKUP_METRIC_FLAG):
            if self.params[5070] == 0 or self.return_value > 0.0:
                # if we switched units for tool change - switch back
                tool_probe_restore_sub(self, ABSOLUTE_FLAG, FEED_BACKUP, BACKUP_METRIC_FLAG)
                #self.set_errormsg("tool_probe_m6 remap error: Probe contact not found")
                #yield INTERP_ERROR
                self.execute("(ABORT,Probe contact not found)")

# if we switched units for tool change - switch back
def tool_probe_restore_sub(self, ABSOLUTE_FLAG, FEED_BACKUP, BACKUP_METRIC_FLAG):
            print ("restore Units", ABSOLUTE_FLAG, FEED_BACKUP, BACKUP_METRIC_FLAG)
            if ABSOLUTE_FLAG:
                self.execute("G90")
            else:
                self.execute("G91")

            if BACKUP_METRIC_FLAG:
                self.execute("G21")
                print ("switched Units back to metric")
            else:
                self.execute("G20")
                print ("switched Units back to imperial")

            self.params["feed"] = FEED_BACKUP
            
            
