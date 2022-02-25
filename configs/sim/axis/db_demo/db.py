#!/usr/bin/env python3

# Exampele [EMCIO]DB_PROGRAM:
# Demonstrate LinuxCNC interface for a database of tools
# with tool time usage monitoriing

#  Example command line testing:
#  $ ./db_nonran.py
#  g               (g: get all tools)
#  p t11 p12 d.3   (u: put t11 for received update)
#  g               (g: note changes from p cmd)

#-----------------------------------------------------------
import sys
import time
db_ran_savefile    = "/tmp/db_ran_file"     # db flatfile
db_nonran_savefile = "/tmp/db_nonran_file"  # db flatfile

#-----------------------------------------------------------
# Notes
#  1) all writes to stdout are piped to host
#     (use stderr for debug prints)
#  2) tool usage time is computed based on
#     loads/unloads.  Tool must be unloaded
#     when LinuxCNC is stopped.
#  3) a database savefile will be created at
#     startup if it does not exist applying
#     rules for a simulation setup.

#-----------------------------------------------------------
# use tooldb module for low-level interface:

from tooldb import tooldb_callbacks # functions (g,p,l,u)
from tooldb import tooldb_tools     # list of tool numbers
from tooldb import tooldb_loop      # main loop

#-----------------------------------------------------------
# Simulation tool number limits:
toolno_min = 10
toolno_max = 19

#-----------------------------------------------------------
# globals
ewrite = sys.stderr.write  # shorthand
tools = dict()             # tools[tno]: dict of text toollines
restore_pocket = dict()    # for nonran toolchanger
p0tool = -1                # tool number in spindle (p0)
start_time = 0             # 0 ==> indeterminate
elapsed_time = 0           # tool time usage
mfmt = "%.3f"              # format for M letter usage time

# tletters: interface to LinuxCNC
tletters = ['T','P', 'D','X','Y','Z','A','B','C','U','V','W','I','J','Q']

# aux_letters: augment db usage
aux_letters = ['M'] # M for operating Minutes

all_letters = tletters + aux_letters

#-----------------------------------------------------------
# Default: support random_toolchanger
# Use symbolic link with "nonran" in its name to implement
# a nonrandom_toolchanger
if sys.argv[0].find("nonran")>=0:
    ewrite("%s:starting(nonrandom_toolchanger)\n"%sys.argv[0])
    random_toolchanger = 0
    db_savefile = db_nonran_savefile
else:
    ewrite("%s:starting(random_toolchanger)\n"%sys.argv[0])
    random_toolchanger = 1
    db_savefile = db_ran_savefile

#-----------------------------------------------------------
def umsg(txt): # debug usage
    ewrite("%s UNEXPECTED: %s"%(sys.argv[0],txt))

def toolline_to_list(line): return line.upper().split()

def toolline_to_dict(line,letters):
    # line: "Tt Pp Dd Xx Yy Zz ..."
    # dict:  D["T"]=tno, D["P"]="p", D["D"]=d,  ...
    D = dict()
    line_nocomment = line.upper().split(';')[0]
    for item in line_nocomment.split():
        for l in letters:
            if l in item: D[l]=item.lstrip(l)
    return D

def dict_to_toolline(D,letters):
    toolline = str()
    for l in letters:
        if l in D: toolline = toolline + " " + l + D[l]
    toolline = toolline.strip()
    return toolline

def save_tools_to_file(fname):
    global tools, all_letters
    f = open(fname,"w")
    for tno in tools:
        if debug: ewrite("@@Save: tno=%d %s\n"%(tno,tools[tno]))
        D = toolline_to_dict(tools[tno],all_letters)
        for l in all_letters:
           if l == "T" or     l == "P": continue # always include
           if l in D   and D[l] == "0": del D[l] # drop zero entries
        tools[tno] = dict_to_toolline(D,all_letters)
    for i in sorted(tools,key=tools.get): f.write(tools[i]+"\n")
    f.close()

def startup_pno(tno): # simulation rule:
    return tno+1      # startup pocket number different than tool number

def init_tools(tfirst,tlast):
    # set tool parameters for all tools
    global tools
    ewrite("init_tools(): tfirst=%d tlast=%d\n"%(tfirst,tlast))
    for tno in range(tfirst,tlast+1):
        tools[tno] = "T%-3d P%-3d Z0.%d D0.%d"%(tno,startup_pno(tno),tno,tno)
    if random_toolchanger:
        tools[0] = "T0   P0 ; Empty pocket"

    # for testing tool time usage computations:
    tools[11] = "T11 P12 Z0.11 D0.10" # special same diameter as T10
    tools[12] = "T12 P13 Z0.12 D0.10" # special same diameter as T10

    save_tools_to_file(db_savefile)
    return

def make_tools(tfirst,tlast):
    global tools,p0tool
    try:   f = open(db_savefile,"r")
    except IOError:
        init_tools(tfirst,tlast)
        return
    except Exception as e:
        ewrite("make_tools: e=%s\n"%e)

    # use preexisting file:
    while True:
        line = f.readline().strip()
        if not line: break
        D = toolline_to_dict(line,all_letters)
        tno = int(D["T"])
        if not random_toolchanger:
            D["P"] = str(startup_pno(tno))
        tools[tno] = dict_to_toolline(D,all_letters)
        if debug: ewrite("@@tno=%d line=%s\n"%(tno,line))
        if random_toolchanger and D['P'] == "0":
            p0tool = tno
    f.close()
    save_tools_to_file(db_savefile)
    return

def start_tool_timer(tno):
    global start_time, elapsed_time, p0tool
    if tno != p0tool:
        umsg("start_tool_timer tno %d != p0tool %d\n"%(tno,p0tool))
    #ewrite("START TIMER LOAD tno=%d\n"%tno)
    start_time = time.time()

def stop_tool_timer(tno):
    global start_time, elapsed_time, p0tool
    if start_time == 0 and random_toolchanger and tno != 0:
        # ran_tc and starting with tool in spindle
        ewrite("NOTE: Tool %d was not unloaded in prior run\n"%tno)
        return

    if p0tool == -1: umsg("stop_tool_timer with no loaded tool\n")
    elapsed_time = time.time() - start_time
    D = toolline_to_dict(tools[tno],all_letters)
    if not 'M' in D:
        D['M'] = mfmt%(elapsed_time/60.0)
    else:
        tot_time_m = float(D['M'].strip('M')) + elapsed_time/60.0
        D['M'] = mfmt%(tot_time_m)
    #ewrite("TIME T%d incr: %s total: %s (minutes)\n"%(
    #       tno, mfmt%(elapsed_time/60.0), D['M']))
    if tno == 0: del D['M']
    tools[tno] = dict_to_toolline(D,all_letters)

def update_tool(tno,update_line):
    # NOTE: host uses 'R'(radius) but tool table uses 'D'(diameter)
    D = toolline_to_dict(tools[tno],all_letters)
    savep = D['P']
    for item in toolline_to_list(update_line):
        # g10l1 reports radius 'R' but tool table uses diameter 'D':
        if "R" in item.upper():
            item = "D" + str(float(item.upper().strip("R"))*2.0)
        for l in tletters:
            if l in item: D[l]=item.lstrip(l) #supersede by update_line
    if not random_toolchanger: D['P'] = savep # nonran handle p0 case
    if D['P'] != savep: umsg("update_tool(): %s %s\n"%(D['P'],savep))
    tools[tno] = dict_to_toolline(D,all_letters)

def apply_db_rules():
    # Apply database rules (typically when a tooldata reload requested)
    # Rule: For tools having identical diameter (within diameter_epsilon),
    #       lowest tool number is assigned to the pocket/parameters for
    #       the tool with the lowest operating time (in minutes).
    #
    #       So if t10diameter==t11diameter==t12diameter,
    #       t10 will select tool with lowest operating minutes
    #       ...
    #       t12 will select tool with most   operating minutes
    #
    # NOTE: Tooldata must be reloaded for computed updates.
    #       Use gui reload buttons or G10L0 in mdi or gcode programs

    #ewrite("APPLY_DB_RULES================================\n")
    diameter_epsilon = 0.001
    for tno in range(toolno_min,toolno_max+1):
        CUR   = toolline_to_dict(tools[tno],all_letters)
        diam  = float(CUR["D"])
        if not 'M' in CUR: CUR['M'] = "0"

        for tryno in range(tno+1,toolno_max+1):
            TRY = toolline_to_dict(tools[tryno],all_letters)
            if abs(diam - float(TRY["D"])) > diameter_epsilon: continue
            # found equal diameters
            #ewrite("MATCH: tno=%d tryno=%d diam=%s\n"%(tno,tryno,CUR["D"]))
            if not 'M' in TRY: TRY['M'] = "0.0"
            if float(TRY['M']) < float(CUR['M']):
                CUR['T'] = str(tryno)
                TRY['T'] = str(tno)
                tools[tno]   = dict_to_toolline(TRY,all_letters)
                tools[tryno] = dict_to_toolline(CUR,all_letters)
                CUR = toolline_to_dict(tools[tno],all_letters)
                save_tools_to_file(db_savefile)

#-----------------------------------------------------------
# Interface functions

#   user_get_tool: host requests tool data
def user_get_tool(toolno):
    global toollist
    # detect tool data reload and apply db rules:
    if toolno == toollist[0]: apply_db_rules()

    tno = int(toolno)
    if debug: ewrite("@@user_get_tool: %s\n"%tools[tno])
    ans = ""
    for item in toolline_to_list(tools[tno]):
        for l in tletters:
            if l in item: ans = ans + " " + item
    return ans.strip()

#   user_put_tool: host updates tool data:
def user_put_tool(toolno,params):
    tno = int(toolno)
    update_tool(tno,params.upper() )
    save_tools_to_file(db_savefile)

# examples cmds received for load_spindle and unload_spindle
# NONRAN example:
# t15m6 l T15  P0
# t0 m6 u T0   P0
#
# t15m6 l T15  P0
# t16m6 l T16  P0
# t0 m6 u T0   P0

# RAN example (two commands each step):
# t15 m6 u T0   P16
#        l T15  P0
# t0  m6 u T15  P16
#        l T0   P0
#
# t15 m6 u T0   P16
#        l T15  P0
# t16 m6 u T15  P17
#        l T16  P0
# t0  m6 u T16  P16
#        l T0   P0

def user_load_spindle_nonran_tc(toolno,params):
    global p0tool
    #ewrite("user_load_spindle_nonran_tc 'l %s'\n"%params)
    tno = int(toolno)

    TMP = toolline_to_dict(params,['T','P'])
    if TMP['P'] != "0": umsg("user_load_spindle_nonran_tc P=%s\n"%TMP['P'])
    if tno      ==  0:  umsg("user_load_spindle_nonran_tc tno=%d\n"%tno)

    # save restore_pocket as pocket may have been altered by apply_db_rules()
    D   = toolline_to_dict(tools[tno],all_letters)
    restore_pocket[tno] = D['P']
    D['P'] = "0"

    if p0tool != -1:  # accrue time for prior tool:
        stop_tool_timer(p0tool)
        RESTORE = toolline_to_dict(tools[p0tool],all_letters)
        RESTORE['P'] = restore_pocket[p0tool]
        tools[p0tool] = dict_to_toolline(RESTORE,all_letters)

    p0tool = tno
    D['T'] = str(tno)
    D['P'] = "0"
    start_tool_timer(p0tool)
    tools[tno] = dict_to_toolline(D,all_letters)
    save_tools_to_file(db_savefile)

def user_unload_spindle_nonran_tc(toolno,params):
    global p0tool
    #ewrite("user_unload_spindle_nonran_tc 'u %s'\n"%params)
    tno = int(toolno)

    if p0tool == -1: return # ignore
    TMP = toolline_to_dict(params,['T','P'])
    if tno       !=  0:  umsg("user_unload_spindle_nonran_tc tno=%d\n"%tno)
    if TMP['P']  != "0": umsg("user_unload_spindle_nonran_tc P=%s\n"%TMP['P'])

    stop_tool_timer(p0tool)
    D = toolline_to_dict(tools[p0tool],all_letters)
    D['T'] = str(p0tool)
    D['P'] = restore_pocket[p0tool]
    tools[p0tool] = dict_to_toolline(D,all_letters)

    p0tool = -1
    save_tools_to_file(db_savefile)

def user_load_spindle_ran_tc(toolno,params):
    global p0tool
    #ewrite("user_load_spindle_ran_tc   'l %s'\n"%params)
    tno = int(toolno)
    D   = toolline_to_dict(tools[tno],all_letters)

    TMP = toolline_to_dict(params,['T','P'])
    D['T'] = TMP['T']
    D['P'] = TMP['P']

    if p0tool != -1:
        stop_tool_timer(p0tool)
        umsg("user_load_spindle_ran_tc p0tool=%d\n"%p0tool)
    p0tool = tno
    if tno != 0: start_tool_timer(p0tool)
    tools[tno] = dict_to_toolline(D,all_letters)
    save_tools_to_file(db_savefile)

def user_unload_spindle_ran_tc(toolno,params):
    global p0tool
    #ewrite("user_unload_spindle_ran_tc 'u %s'\n"%params)
    tno = int(toolno)
    TMP = toolline_to_dict(params,['T','P'])
    if p0tool == -1:
        D = dict()
        D['T'] = TMP['T']
        D['P'] = TMP['P']
        tools[0] = dict_to_toolline(D,['T','P'])
        #ewrite("user_unload_spindle_ran_tc: ignoring for no p0tool\n")
        pass
    else:
        if p0tool == 0:
            pass
        stop_tool_timer(p0tool)
        D = toolline_to_dict(tools[p0tool],all_letters)
        D['T'] = TMP['T']
        D['P'] = TMP['P']
        tools[tno] = dict_to_toolline(D,all_letters)
        p0tool = -1

    save_tools_to_file(db_savefile)
#-----------------------------------------------------------
# begin
debug = 0 # debug var for command line testing
if (len(sys.argv) > 1 and sys.argv[1] == 'debug'): debug = 1

make_tools(toolno_min,toolno_max)

if random_toolchanger: tooldb_callbacks(user_get_tool,
                                        user_put_tool,
                                        user_load_spindle_ran_tc,
                                        user_unload_spindle_ran_tc)
else: tooldb_callbacks(user_get_tool,
                       user_put_tool,
                       user_load_spindle_nonran_tc,
                       user_unload_spindle_nonran_tc)


toollist = list(range(toolno_min,toolno_max+1))
if random_toolchanger: toollist.append(0) # T0 provision
tooldb_tools(toollist)

try:
    tooldb_loop()  # loop forever, use callbacks
except Exception as e:
    if sys.stdin.isatty():
        ewrite(("Exception=%s\n"%str(e)))
    else: pass # avoid messages at termination
