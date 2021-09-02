#!/usr/bin/env python3

# Demonstrate LinuxCNC interface for a database of tools

#  Example command line testing:
#  $ ./db_demo.py
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

tools = dict() # global

def save_tools_to_file(fname):
    global tools
    f = open(fname,"w")
    for k in tools:
        if debug: sys.stderr.write("Save: key=%s %s\n"%(k,tools[k]))
        f.write(tools[k]+"\n")
    f.close()

def home_pocket(toolno):
    # simulation rule:
    return toolno+1 # make home pocket number differ from tool number

def init_tools(tfirst,tlast):
    # set tool parameters for all tools
    global tools
    for i in range(tfirst,tlast+1):
        tools["T%d"%i] = "T%-3d P%-3d Z0.%d D0.%d"%(i,home_pocket(i),i,i)
    if random_toolchanger:
        tools["T0"] = "T0   P0 ; Empty pocket"
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

    # use opened file:
    while True:
        line = f.readline().strip()
        if not line: break
        key = line[0:line.index(" P")].strip()
        tools[key] = line
        if debug: sys.stderr.write("key=%s line=%s\n"%(key,line))
    f.close()
    save_tools_to_file(db_savefile)
    return

#-----------------------------------------------------------
# functions to simulate db queries/transactions

#   user_get_tool: host requests tool data
def user_get_tool(toolno):
    # host requests tool data
    key = "T%d"%toolno
    if debug: sys.stderr.write("@@user_get_tool: %s\n"%tools[key])
    return tools[key]

#   user_put_tool_*: host updates tool data:
#                    1) offset change (G10 L1, G10 L2 ...)
#                    2) tool removed from spindle
def user_put_tool_ran_tc(toolno,params):
    key = "T%d"%toolno
    tools[key] = params.upper() # sync to host
    if debug: sys.stderr.write("@@user_put_tool_ran_tc: %s\n"%tools[key])
    save_tools_to_file(db_savefile)
    return

lastp0=""
def user_put_tool_nonran_tc(toolno,params):
    global lastp0
    key = "T%d"%toolno
    t = params.split()[0].strip().upper()
    p = params.split()[1].strip().upper()
    if p == "P0":
        if lastp0 != "":
            # restore prior p0 entry
            rkey = lastp0.split()[0].strip().upper()
            tools[rkey] = lastp0 # restore prior p0 settings
    if toolno == 0:
        if debug: sys.stderr.write("Spindle unloaded (T0 P0)\n")
        save_tools_to_file(db_savefile)
        return
    lastp0 = tools[key] # save p0 settings
    tools[key] = params.upper() # sync to host
    if debug: sys.stderr.write("@@user_put_tool_nonran_tc: %s\n"%tools[key])
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
        print(("exception=",e))
    else: pass # avoid messages at termination
