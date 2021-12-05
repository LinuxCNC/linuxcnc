#!/usr/bin/env python3

# Demonstrate LinuxCNC interface for a database of tools

#  Example command line testing:
#  $ ./db_nonran.py
#  g               (g: get all tools)
#  p t11 p12 d.3   (u: put t11 for received update)
#  g               (g: note changes from p cmd)

# Notes
#  1) all writes to stdout are piped to host
#     (use stderr for debug prints)
#-----------------------------------------------------------

import sys
db_ran_savefile    = "/tmp/db_ran_file"
db_nonran_savefile = "/tmp/db_nonran_file"

# Default: support random_toolchanger
# Use symbolic link with "nonran" in its name to
# use nonrandom_toolchanger provisions
if sys.argv[0].find("nonran")>=0:
    sys.stderr.write("%s:starting(nonrandom_toolchanger)\n"%sys.argv[0])
    random_toolchanger = 0
    db_savefile = db_nonran_savefile
else:
    sys.stderr.write("%s:starting(random_toolchanger)\n"%sys.argv[0])
    random_toolchanger = 1
    db_savefile = db_ran_savefile

#-----------------------------------------------------------

from tooldb import tooldb_callbacks # functions
from tooldb import tooldb_tools     # list of tool numbers
from tooldb import tooldb_loop      # main loop

#-----------------------------------------------------------
# Simulate a database of tools, using a flat file:

tools = dict() # global: tools[tno]=toollines for integer tno

letters = ['T','P','D','X','Y','Z','A','B','C','U','V','W','I','J','Q']
# NOTE: host uses 'R'(radius) instead of 'D'(diameter)
#       see: update_tool()

def toolline_to_list(line): return line.upper().split()

def toolline_to_dict(line):
    # line: "Tt Pp Dd Xx Yy Zz ..."
    # dict:  D["T"]="Tt", D["P"]="Pp", D["D"]=Dd,  ...
    D = dict()
    for item in line.upper().split():
        for l in letters:
            if l in item: D[l]=item
    return D

def dict_to_toolline(D):
    toolline = str()
    for l in letters:
        if l in D: toolline = toolline + " " + D[l]
    toolline = toolline.strip()
    return toolline

def save_tools_to_file(fname):
    global tools
    f = open(fname,"w")
    for tno in tools:
        if debug: sys.stderr.write("@@Save: tno=%d %s\n"%(tno,tools[tno]))
        D = toolline_to_dict(tools[tno])
        for l in letters:
           if l == "T" or     l == "P":   continue # always include
           if l in D   and D[l] == l+"0": del D[l] # drop zero entries
        tools[tno] = dict_to_toolline(D)
        f.write(tools[tno]+"\n")
    f.close()

def home_pocket(tno):
    # simulation rule:
    return tno+1 # make home pocket number differ from tool number

def init_tools(tfirst,tlast):
    # set tool parameters for all tools
    global tools
    sys.stderr.write("tfirst=%d\n"%tfirst)
    sys.stderr.write("tlast=%d\n"%tlast)
    for tno in range(tfirst,tlast+1):
        tools[tno] = "T%-3d P%-3d Z0.%d D0.%d"%(tno,home_pocket(tno),tno,tno)
    if random_toolchanger:
        tools[0] = "T0   P0 ; Empty pocket"
    save_tools_to_file(db_savefile)
    return

def make_tools(tfirst,tlast):
    global tools
    try:   f = open(db_savefile,"r")
    except IOError:
        init_tools(tfirst,tlast)
        return
    except Exception as e:
        sys.stderr.write("make_tools: e=%s\n"%e)

    # use existing opened file:
    while True:
        line = f.readline().strip()
        if not line: break
        D = toolline_to_dict(line)
        tno = int(D["T"].strip("T"))
        if not random_toolchanger:
            D["P"] = "P" + str(home_pocket(tno))
        tools[tno] = dict_to_toolline(D)
        if debug: sys.stderr.write("@@tno=%d line=%s\n"%(tno,line))
    f.close()
    save_tools_to_file(db_savefile)
    return

def update_tool(tno,update_line):
    current = toolline_to_dict(tools[tno])
    for item in toolline_to_list(update_line):
        # g10l1 reports radius but tool table uses diameter:
        if "R" in item.upper():
            item = "D" + str(float(item.upper().strip("R"))*2.0)
        for l in letters:
            if l in item: current[l]=item #supersede by update_line
    tools[tno] = dict_to_toolline(current)

#-----------------------------------------------------------
# functions to simulate db queries/transactions

#   user_get_tool: host requests tool data
def user_get_tool(toolno):
    # host requests tool data
    tno = int(toolno)
    if debug: sys.stderr.write("@@user_get_tool: %s\n"%tools[tno])
    return tools[tno]

#   user_put_tool_*: host updates tool data:
#                    1) offset change (G10 L1, G10 L2 ...)
#                    2) tool removed from spindle
def user_put_tool_ran_tc(toolno,params):
    tno = int(toolno)
    tools[tno] = params.upper() # sync to host
    if debug: sys.stderr.write("@@user_put_tool_ran_tc: %s\n"%tools[tno])
    save_tools_to_file(db_savefile)
    return

last_toolline = ""
def user_put_tool_nonran_tc(toolno,params):
    global last_toolline
    tno = int(toolno)
    if tno != 0: update_tool(tno,params.upper() ) # sync to host
    TMP = toolline_to_dict(params)
    pocket = TMP["P"]
    if pocket == "P0":
        if last_toolline != "": # need to restore home_pocket
            D = toolline_to_dict(last_toolline)
            last_tno = int(D["T"].strip("T"))
            D["P"] = "P" + str(home_pocket(last_tno))
            if last_tno == int(toolno):
                # guard for unexpected case of consecutive puts
                # to p0 with params: p tn p0 ... ; p tn p0 ...
                # (don't set home_pocket)
                pass
            else: # restore entry with home_pocket
                tools[last_tno] = dict_to_toolline(D)
        if tno != 0: last_toolline = tools[tno]
    if toolno == 0 and debug:
        sys.stderr.write("Spindle unloaded (T0 P0)\n")
    if debug: sys.stderr.write("@@user_put_tool_nonran_tc: %s\n"%tools[tno])
    save_tools_to_file(db_savefile)
#-----------------------------------------------------------
# begin
debug = 0 # debug var for command line testing
if (len(sys.argv) > 1 and sys.argv[1] == 'debug'): debug = 1

toolno_min = 10
toolno_max = 19
make_tools(toolno_min,toolno_max)

if random_toolchanger:
    tooldb_callbacks(user_get_tool,user_put_tool_ran_tc)
else:
    tooldb_callbacks(user_get_tool,user_put_tool_nonran_tc)

toollist = list(range(toolno_min,toolno_max+1))
if random_toolchanger: toollist.append(0) # T0 provision
tooldb_tools(toollist)

try:
    tooldb_loop()  # loop forever, use callbacks
except Exception as e:
    if sys.stdin.isatty():
        sys.stderr.write(("Exception=%s\n"%str(e)))
    else: pass # avoid messages at termination
