#!/usr/bin/env python3
# Minimal [EMCIO]DB_PROGRAM used by this test.
# Tool data is kept in a flat file (db_tools.txt) so that it survives a
# restart of LinuxCNC, and every received command is appended to db_cmds.log.
# Note: stdout is the pipe to LinuxCNC, so logging goes to a file.

import os

from tooldb import tooldb_callbacks
from tooldb import tooldb_tools
from tooldb import tooldb_loop

savefile = "db_tools.txt"
logfile  = "db_cmds.log"

initial_tools = {
    1: "T1 P1 D0.1 Z0.5",
    2: "T2 P2 D0.2 Z1.5",
    3: "T3 P3 D0.3 Z2.5",
}

tools = {}

def log(msg):
    with open(logfile,"a") as f:
        f.write("%s\n"%msg)

def save():
    with open(savefile,"w") as f:
        for tno in sorted(tools):
            f.write("%s\n"%tools[tno])

def load():
    if not os.path.exists(savefile):
        tools.update(initial_tools)
        save()
        return
    with open(savefile) as f:
        for line in f:
            line = line.strip()
            if not line: continue
            tools[toolno_of(line)] = line

def toolno_of(toolline):
    for item in toolline.upper().split():
        if item.startswith("T"): return int(item[1:])
    raise ValueError("no toolno in <%s>"%toolline)

def merge(toolline,params):
    # apply the letter parameters of params to toolline
    D = dict((item[0],item[1:]) for item in toolline.upper().split())
    for item in params.upper().split():
        if item.startswith(";"): break
        D[item[0]] = item[1:]
    letters = ["T","P"] + sorted(set(D) - set(["T","P"]))
    return " ".join("%s%s"%(letter,D[letter]) for letter in letters)

#   'g' interface command
def get_tool(tno):
    log("g %s"%tools[tno])
    return tools[tno]

#   'p' interface command
def put_tool(tno,params):
    log("p %s"%params)
    tools[tno] = merge(tools.get(tno,"T%d P%d"%(tno,tno)),params)
    save()

#   'l' interface command
def load_spindle(tno,params):
    log("l %s"%params)

#   'u' interface command
def unload_spindle(tno,params):
    log("u %s"%params)

load()
tooldb_callbacks(get_tool,put_tool,load_spindle,unload_spindle)
tooldb_tools(sorted(tools))
tooldb_loop()
