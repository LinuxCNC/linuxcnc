#!/usr/bin/python3
# Note: '#!/usr/bin/python3' is used instead of
#       '#!/usr/bin/env python3'
#      since it allows ps and pidof to show/find
#      using the script name

# Example [EMCIO]DB_PROGRAM:
# Demonstrate LinuxCNC interface for a database of tools
# with tool time usage monitoring
# ini file examples:
#   [EMCIO]DB_PROGRAM=./db_nonran.py [nonran_filename]
#   [EMCIO]DB_PROGRAM=./db_ran.py    [ran_filename]

# defaults for *_filename:
db_ran_savefile    = "/tmp/db_ran_file"     # db flatfile
db_nonran_savefile = "/tmp/db_nonran_file"  # db flatfile

#-----------------------------------------------------------
import os
import sys
import time
import signal
import threading
import getopt
import linuxcnc
#-----------------------------------------------------------
# LinuxCNC tooldb module for low-level interface:
from tooldb import tooldb_callbacks # functions (g,p,l,u)
from tooldb import tooldb_tools     # list of tool numbers
from tooldb import tooldb_loop      # main loop

#-----------------------------------------------------------
# Notes
#  1) Uses the LinuxCNC provided tooldb module: so all
#     writes to stdout are piped to LinuxCNC host.
#     Use stderr for debug prints in this file.
#  2) tool usage time is computed based on
#     loads/unloads.  Tool must be unloaded
#     when LinuxCNC is stopped.
#  3) a database savefile will be created at
#     startup if it does not exist applying
#     rules for a simulation setup.
#  4) startup/tooltimer
#   a) random toolchanger: both LinuxCNC and database
#      know if there is a loaded tool at startup so
#      if a tool is loaded, timer is started
#   b) nonrandom toolchanger: LinuxCNC is not
#      aware of loaded tool at startup so timer is
#      not started.  (use m61qn to specify tool
#      and start timer)
#   c) best practice: operator always unloads tool
#      when stopping LinuxCNC
#  5) The cmd_loop() function is executed in a thread
#     so that concurrent tasks can be demonstrated.
#  6) Worker tasks demonstrate changes to database
#     with synchronization to the LinuxCNC host:
#    a) tool_modify_task: change a tool parameter
#    b) tool_update_task add/remove tool no.s
#     The tasks are executed in threads concurrent
#     to the primary task that executes tooldb_loop().
#     to illustrate methods for db_program guis that
#     update and synchronize tooldata with LinuxCNC.
#  7) The periodic_task updates tool time usage.
#  8) A single mutex is used to protect all activity
#     than can read/write tools[] and db_*_savefile
#     The mutex is acquired and held by
#        a) each tooldb interface routine specified
#           by the call to tooldb_callbacks()
#        b) the periodic_task
#        c) each worker demonstration task:
#           tool_modify_task()
#           tool_update_task()
#  9) Monitor tool data updates by using shell watch
#     command in a terminal. Examples:
#     $ watch -d -n 1 cat /tmp/db_ran_file
#     $ watch -d -n 1 cat /tmp/db_nonran_file
# 10) export environmental variables DB_SHOW, DB_DEBUG
#     to print operational details from LinuxCNC
# 11) If LinuxCNC is not running, use is limited to basic
#     command line testing for commands g,p,l,u.
#     Use Ctrl-C to exit
#     Example (simple non random toolchanger):
#     $ ./db_nonran.py [debug]
#     g               (g: get all tools)
#     p t11 p111 d.3  (p: put t11 for received update)
#     g               (g: note changes from p cmd)
#     l t14 p0        (l: load   tool 14)
#     u t0  p0        (u: unload tool 14)
#     Example (more complicated random toolchanger):
#     $ ./db_ran.py [debug]
#     $ g             (g: get all tools)
#     $ u t0  p114    (u: first  of pair to load t14)
#     $ l t14 p0      (l: second of pair to load t14)
# 12) problem: unexpected termination (like a power
#     failure) is not detected by LinuxCNC so tool
#     time data and tool-in-spindle status may be
#     corrupted for such events.  A database application
#     may be able to detect and notify for some anomalies.

#-----------------------------------------------------------
# globals init
prog = sys.argv[0]         # this program
toollist = []              # list of tool numbers
tools = dict()             # tools[tno]: dict of text toollines
spindle_tool = -1          # no tool in spindle is indicated by:
                           # nonran:-1, ran:-1|0
nonran_from_pocket = dict()# use to restore pocket in db
start_time = -1            # neg means timer not active
elapsed_time = 0           # tool time usage
mfmt = "%.3f"              # format for M letter usage time
sync_fail_reason = ""      # why sync failed
cmdline_only = 0           # 1: for cmdline debugging
history = [];              # history logging
history_max_ct = 10        # history logging
period_minutes = 1         # default for periodic_task
mutex = threading.Lock()           # lock for all threads
disconnect_evt = threading.Event() # set at disconnect

# tletters: interface to LinuxCNC
tletters = ['T','P', 'D','X','Y','Z','A','B','C','U','V','W','I','J','Q']

# aux_letters: augment db usage
aux_letters = ['M'] # M for operating Minutes
all_letters = tletters + aux_letters

alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ"

#-----------------------------------------------------------
# messages to stderr (stdout is piped to LinuxCNC)
def  msg(txt): sys.stderr.write("%s: %s\n"%(prog,txt))
def umsg(txt): msg("UNEXPECTED: %s"%txt) # debug usage

try: # environ override default period_minutes for testing:
    period_minutes = float(os.environ['DB_PERIOD_MINUTES'])
    msg("period_minutes=%f"%period_minutes)
except Exception as e: pass
#-----------------------------------------------------------
# LinuxCNC interface for status & syncing
linuxcnc_stat      = linuxcnc.stat()
linuxcnc_tool_sync = linuxcnc.command().load_tool_table
try: linuxcnc_stat.poll() # test if LinuxCNC running
except Exception as e:
    msg("e=%s"%e)
    msg("LinuxCNC not active, cmdline debugging")
    cmdline_only = 1

#-----------------------------------------------------------
def ctrlc_handler(signo, frame):
    sys.stderr.write(" Caught signo=%d Ctrl-C bye\n"%signo)
    sys.exit(0)
if cmdline_only: signal.signal(signal.SIGINT, ctrlc_handler)

# defer handler until ready to avoid signal to io process
def notready_for_signal(signo, frame): umsg("notready_for_signal %d"%signo)

signal.signal(signal.SIGUSR1,notready_for_signal)
signal.signal(signal.SIGUSR2,notready_for_signal)

#-----------------------------------------------------------
# Simulation initial tool number and pocket number setups
toolno_min = 10
toolno_max = 19

pocket_offset      = 100
n_extra_pockets    =   5 # for add/remove demo
n_pockets          =  (toolno_max - toolno_min + 1) + n_extra_pockets
pockets_base       =  toolno_min + pocket_offset # start of pocket numbering
available_pockets  =  []
for i in range(n_pockets): available_pockets.append(pockets_base + i)

#-----------------------------------------------------------
def start_db_program():
    global db_savefile, period_minutes, random_toolchanger
    # Default: support random_toolchanger
    # Use symbolic link with "nonran" in its name to implement
    # a nonrandom_toolchanger
    if sys.argv[0].find("nonran")>=0:
        msg("starting(nonrandom_toolchanger)")
        random_toolchanger = 0
        db_savefile = db_nonran_savefile # default file
    else:
        msg("starting(random_toolchanger) n_pockets = %d"%n_pockets)
        random_toolchanger = 1
        db_savefile = db_ran_savefile # default file
    
    # handle options and args:
    try: opts,remainder = getopt.getopt(sys.argv[1:]
                                      ,shortopts='hp:'
                                      ,longopts=['help','period_minutes=']
                                      )
    except getopt.GetoptError as emsg:
        msg("GetoptError:%s"%emsg)
        sys.exit(1)
    for opt,arg in opts:
        if opt in ('-p','--period_minutes'):
            period_minutes=float(arg)
            msg("period_minutes=%f"%period_minutes)
        if opt in ('-h','--help'):
            msg("""
        Usage: %s [Options] [flat_filename]
        Default flat_filename = %s
        Options:
            -h|--help (and exit)
            -p|--period_minutes value (default=%f)
         
        bye!"""%(prog,db_savefile,period_minutes))
            sys.exit(1)
    if remainder:
        db_savefile = remainder[0] # superseded default
        msg("db_savefile = %s"%db_savefile)
    if len(remainder) >1:
        msg("Unknown arguments %s"%remainder[1:])
        sys.exit(1)

#-----------------------------------------------------------
# functions
def toolline_to_list(line): return line.upper().split()

def toolline_to_dict(line,letters):
    # line: "Tt Pp Dd Xx Yy Zz ..."
    # dict:  D["T"]=tno, D["P"]="p", D["D"]=d,  ...
    D = dict()
    # use leading ';' to denote comment line
    line_nocomment = line.upper().split(';')[0]
    for item in line_nocomment.split():
        for l in letters:
            if l in item: D[l]=item.lstrip(l).lstrip('+')
    return D

def dict_to_toolline(D,letters):
    toolline = str()
    for l in letters:
        if l in D: toolline = toolline + " " + l + D[l]
    toolline = toolline.strip()
    return toolline

def save_tools_to_file(fname,comment=""):
    f = open(fname,"w")
    for tno in tools:
        if debug: msg("@@Save: tno=%d %s"%(tno,tools[tno]))
        D = toolline_to_dict(tools[tno],all_letters)
        for l in all_letters:
           if l == "T" or     l == "P": continue # always include
           if l in D   and D[l] == "0": del D[l] # drop zero entries
        tools[tno] = dict_to_toolline(D,all_letters)
    for i in sorted(tools,key=tools.get): f.write(tools[i]+"\n")

    global history
    if comment: history.append(comment)
    if len(history) > history_max_ct: history=history[1:]
    if len(history):
        f.write("\n; Recent (last %d) db history:\n"%history_max_ct)
        for i in range(len(history)):
            f.write("; %s\n"%history[i])
    f.close()

def nonran_pno(tno): # simulation rule:
    return tno+pocket_offset # make startup pocket number different than tool number
                             # to avoid assumptions about tno and pno

def nonran_restore_pocket(tno):
    if not tno in nonran_from_pocket:
        umsg("nonran_restore_pocket tno=%d not in dict: %s"%(tno,nonran_from_pocket))
    D = toolline_to_dict(tools[tno],all_letters)
    D['P'] = nonran_from_pocket[tno]
    tools[tno] = dict_to_toolline(D,all_letters)
    #msg("Restore pocket tno:%d pno %s"%(tno,int(D['P'])))
    del nonran_from_pocket[tno]

def assign_pocket(tno):
    global available_pockets
    if not random_toolchanger:
        return nonran_pno(tno)
    if not len(available_pockets):
        umsg("assign_pocket: no pockets available") # expect error
    available_pockets.sort()
    pno = available_pockets[0]
    available_pockets.remove(pno)
    return pno

def release_pocket(pno):
    global available_pockets
    if not random_toolchanger: return # not applicable
    if pno in available_pockets: umsg("pno=%d was already available"%pno)
    available_pockets.append(pno)
    available_pockets.sort()
    return

def init_tools(tfirst,tlast):
    # set tool parameters for all tools
    global tools
    msg("init_tools(): tfirst=%d tlast=%d"%(tfirst,tlast))
    for tno in range(tfirst,tlast+1):
        if random_toolchanger:
            pno = assign_pocket(tno)
            tools[tno] = "T%-3d P%-3d Z0.%d D0.%d"%(tno,pno,tno,tno)
            tools[0]   = "T0   P0 ; Empty pocket"
        else:
            tools[tno] = "T%-3d P%-3d Z0.%d D0.%d"%(tno,nonran_pno(tno),tno,tno)

    # for testing tool time usage computations:
    # T11,T12 have same diameter as T10
    D = toolline_to_dict(tools[11],all_letters)
    D['D'] = "D0.10"
    tools[11] = dict_to_toolline(D,all_letters)
    D = toolline_to_dict(tools[12],all_letters)
    D['D'] = "D0.10"
    tools[12] = dict_to_toolline(D,all_letters)

    save_tools_to_file(db_savefile,"Created %s (init_tools)"%db_savefile)
    return

def make_tools(tfirst,tlast):
    global tools,spindle_tool
    try:   f = open(db_savefile,"r")
    except IOError:
        init_tools(tfirst,tlast)
        initialtoollist = list(range(toolno_min,toolno_max+1))
        if random_toolchanger: initialtoollist.append(0) # T0 provision
        return initialtoollist
    except Exception as e:
        msg("make_tools: e=%s"%e)

    # use preexisting file:
    savedtoollist = []
    while True:
        line = f.readline().strip()
        if not line: break
        if line[0] == ';': continue
        D = toolline_to_dict(line,all_letters)
        tno = int(D["T"])
        savedtoollist.append(tno)
        if random_toolchanger:
            pno = int(D["P"])
            if pno != 0: available_pockets.remove(pno)
        if not random_toolchanger:
            #Note: nonran toolchanger:
            #      database may indicate tool is loaded at startup
            #      but LinuxCNC does not have this info
            #      so: timer is not started, use nonran_pno
            if D["P"] == "0":
                msg("nonran toolchanger was stopped with loaded tool %d"%tno)
            D["P"] = str(nonran_pno(tno))
        tools[tno] = dict_to_toolline(D,all_letters)
        if debug: msg("@@tno=%d line=%s"%(tno,line))
        if random_toolchanger and D['P'] == "0":
            spindle_tool = tno
            start_tool_timer(spindle_tool)
    f.close()
    save_tools_to_file(db_savefile
                      ,"Using preexisting db file: %s (make_tools)"%db_savefile)
    return savedtoollist

def sync_allowed():
    global sync_fail_reason
    try:
        linuxcnc_stat.poll()
        if linuxcnc_stat.interp_state != linuxcnc.INTERP_IDLE:
            sync_fail_reason = "interp not idle"
            return 0 # fail
    except Exception as e:
        msg("sync_allowed fail:%s"%e)
        sync_fail_reason = e
        return 0 # fail

    sync_fail_reason = ""
    return 1 # success

def add_dbtool(tno,**kwargs):
    # kwargs specify parameters in tletters
    # example usage: add_dbtool(tno,Ppvalue Zzvalue Ddvalue Xxvalue ...)
    global toollist,tools
    for k in kwargs.keys():
        if k != k.upper():
            msg("!!!add_dbtool ERR: unexpected lower case param: %s"%k)
        if not k in tletters:
            msg("!!!add_dbtool ERR: unknown param letter: %s"%k)
            return 0 # fail

    if not sync_allowed():
        msg("add_dbtool: DISALLOWED tno=%d (%s)"%(tno,sync_fail_reason))
        return 0 #fail

    parms=""
    for l in tletters:
        if l in kwargs:
            if l == 'P': umsg("add_dbtool tno=%d P%s"%(tno,kwargs[l]))
            parms = parms + " %s%s"%(l,kwargs[l])
    pno = assign_pocket(tno)
    tools[tno] = "T%d"%tno + " P%d"%pno + parms
    toollist.append(tno)
    save_tools_to_file(db_savefile,"tno:%3d added   (add_dbtool)"%tno)
    tooldb_tools(toollist); linuxcnc_tool_sync()
    msg("add_dbtool: ADD tno=%d pno=%d"%(tno,pno))
    return 1 # success

def rm_dbtool(tno):
    if not tno in toollist:
        msg(" rm_dbtool: ERR tno=%d not in toollist"%tno)
        return 0 # fail
    if tno == spindle_tool:
        msg(" rm_dbtool: ERR tno=%d is loaded to   spindle"%tno)
        return 0 # fail

    D = toolline_to_dict(tools[tno],tletters)
    pno = int(D['P'])

    if not sync_allowed():
        msg(" rm_dbtool: DISALLOWED tno=%d (%s)"%(tno,sync_fail_reason))
        return 0 #fail

    del tools[tno]
    toollist.remove(tno)
    save_tools_to_file(db_savefile,"tno:%3d removed (rm_dbtool)"%tno)
    tooldb_tools(toollist); linuxcnc_tool_sync()
    msg(" rm_dbtool: RM  tno=%d pno=%d"%(tno,pno))
    return 1 # success

def handle_disconnect(fname):
    global disconnect_evt
    if spindle_tool > 0:
        stop_tool_timer(spindle_tool)
        msg("!!!Stopped with loaded tool: %d"%spindle_tool)
        if not random_toolchanger:
            nonran_restore_pocket(spindle_tool)
        save_tools_to_file(fname,"tno:%3d in spindle at termination"%spindle_tool)
    msg("disconnected")
    disconnect_evt.set()

def start_tool_timer(tno):
    global start_time, elapsed_time, spindle_tool
    if tno != spindle_tool:
        umsg("start_tool_timer tno %d != spindle_tool %d"%(tno,spindle_tool))
    if start_time >= 0:
        umsg("start_tool_timer: tno:%d spindle_tool:%d start_time=%f"%(tno,spindle_tool,start_time))
    start_time = time.time()

def update_tool_time(tno):
    global start_time, elapsed_time
    now = time.time()
    if tno == 0: umsg("update_tool_time with no tool")
    if start_time < 0 and spindle_tool > 0:
        umsg("update_tool_time spindle_tool=%d start_time=%f"%(spindle_tool,start_time))
        start_time = now
        return
    elapsed_time = now - start_time
    start_time = now
    D = toolline_to_dict(tools[tno],all_letters)
    if not 'M' in D:
        D['M'] = mfmt%(elapsed_time/60.0)
    else:
        tot_time_m = float(D['M'].strip('M')) + elapsed_time/60.0
        D['M'] = mfmt%(tot_time_m)
    tools[tno] = dict_to_toolline(D,all_letters)
    save_tools_to_file(db_savefile)

def stop_tool_timer(tno):
    global start_time, elapsed_time, spindle_tool
    if start_time < 0 and random_toolchanger and tno != 0:
        # ran_tc and starting with tool in spindle
        msg("NOTE: Tool %d was not unloaded in prior run"%tno)
        return
    if spindle_tool <= 0: umsg("stop_tool_timer with no loaded tool")

    update_tool_time(spindle_tool)
    start_time = -1 #  negative means timer not active

def update_tool_params(tno,update_line):
    D = toolline_to_dict(tools[tno],all_letters)
    for item in toolline_to_list(update_line):
        for l in tletters:
            if l in item: D[l]=item.lstrip(l) #supersede by update_line
    if not random_toolchanger and spindle_tool == tno:
        D['P'] = '0' # unload uses nonran_from_pocket[]
    tools[tno] = dict_to_toolline(D,all_letters)

def apply_db_rules():
    # Apply database rules
    #   typically:   when a tooldata reload requested
    #   alternately: ater a tool is unloaded
    # Rule: For tools having identical diameter (within diameter_epsilon),
    #       lowest tool number is assigned to the pocket/parameters for
    #       the tool with the lowest operating time (in minutes).
    #
    #       So if t10diameter==t11diameter==t12diameter,
    #       t10 will select tool with lowest operating minutes
    #       ...
    #       t12 will select tool with most   operating minutes
    #
    # NOTE: Tooldata must be reloaded for retrieval of updates.
    #       Use gui reload buttons or G10L0 in mdi or gcode programs

    #msg("APPLY_DB_RULES================================")
    diameter_epsilon = 0.001
    if spindle_tool > 0: # not allowed with G10L0 but gui may attempt
        msg("DISALLOW: apply_db_rules with tool %d loaded"%spindle_tool)
        return
    for tno in toollist:
        if tno == 0: continue
        CUR   = toolline_to_dict(tools[tno],all_letters)
        diam  = float(CUR["D"])
        if not 'M' in CUR: CUR['M'] = "0"

        # find tools after tno in toollist with same diameter:
        for tryno in toollist[toollist.index(tno)+1:]:
            if tryno == 0: continue
            TRY = toolline_to_dict(tools[tryno],all_letters)
            if abs(diam - float(TRY["D"])) > diameter_epsilon: continue
            # found equal diameters
            if not 'M' in TRY: TRY['M'] = "0"
            #msg("diameter match: tno=%d tryno=%d diam=%f trydiam=%s"
            #   %(tno,tryno,diam,TRY["D"]))
            if float(TRY['M']) < float(CUR['M']):
                # swap tool data so that tno has fewest minutes:
                CUR['T'] = str(tryno)
                TRY['T'] = str(tno)
                tools[tno]   = dict_to_toolline(TRY,all_letters)
                tools[tryno] = dict_to_toolline(CUR,all_letters)
                CUR = toolline_to_dict(tools[tno],all_letters)
                save_tools_to_file(db_savefile,
                   "tno:%3d diam=%f NEWpno=%s minutes=%s (apply_db_rules)"
                    %(tno,diam,CUR['P'],CUR['M']))

def check_params(tno,params):
    if (tno != 0) and (not tno in tools): # expect NAK
        umsg("Attempt to use tno=%d not in db"%tno)
    for l in params.upper():
        if l in alphabet and not l in tletters:
            umsg("unknown param letter:%s in %s"%(l,params))
#-----------------------------------------------------------
# Interface (callback) functions

#   'g' interface command (ran or nonran)
def user_get_tool(tno):
    mutex.acquire() # blocking
    # detect tool data reload (gui or with G10L0) and apply db rules:
    if tno == toollist[0]: apply_db_rules()
    if debug: msg("@@user_get_tool: %s"%tools[tno])
    ans = "" # line formatted for LinuxCNC db interface
    for item in toolline_to_list(tools[tno]):
        for l in tletters:
            if l in item: ans = ans + " " + item
    mutex.release()
    return ans.strip()

#   'p' interface command (ran or nonran toolchanger)
def user_put_tool(tno,params):
    mutex.acquire() # blocking
    check_params(tno,params)
    update_tool_params(tno,params.upper() )
    save_tools_to_file(db_savefile,"tno:%3d updated (user_put_tool)"%tno)
    mutex.release()

#   'l' interface command (nonran toolchanger)
def user_load_spindle_nonran_tc(tno,params):
    mutex.acquire() # blocking
    global spindle_tool
    #msg("user_load_spindle_nonran_tc 'l %s'"%params)
    check_params(tno,params)
    if tno == spindle_tool:
        # a tool reload may be requested (gui, worker task, etc.) using:
        # emcmodule.cc:load_tool_table()
        #              --> EMC_TOOL_LOAD_TOOL_TABLE
        #                  --> ioControl.cc:reload_tool_number()
        # Note: the redundant reload is done for nonrandom toolchanger only
        #msg("user_load_spindle_nonran_tc: redundant load for tno=%d"%tno)
        mutex.release()
        return
    TMP = toolline_to_dict(params,['T','P'])
    if TMP['P'] != "0": umsg("user_load_spindle_nonran_tc P=%s"%TMP['P'])
    if tno      ==  0:  umsg("user_load_spindle_nonran_tc tno=%d"%tno)

    D = toolline_to_dict(tools[tno],all_letters)

    # save nonran_from_pocket as pocket may have been altered by apply_db_rules()
    nonran_from_pocket[tno] = D['P']

    if spindle_tool > 0 and spindle_tool != tno:  # accrue time for prior tool:
        stop_tool_timer(spindle_tool)
        nonran_restore_pocket(spindle_tool)

    spindle_tool = tno
    D['T'] = str(tno)
    D['P'] = "0"
    start_tool_timer(spindle_tool)
    tools[tno] = dict_to_toolline(D,all_letters)
    save_tools_to_file(db_savefile,"tno:%3d (  load to   spindle)"%tno)
    mutex.release()

#   'u' interface command (nonran toolchanger)
def user_unload_spindle_nonran_tc(tno,params):
    mutex.acquire() # blocking
    global spindle_tool
    #msg("user_unload_spindle_nonran_tc spindle_tool=%d restore=%s'u %s'"%
    #    (spindle_tool,nonran_from_pocket,params))
    check_params(tno,params)

    if spindle_tool == -1: # no tool in spindle
        pass # ignore
    else:
        TMP = toolline_to_dict(params,['T','P'])
        if tno       !=  0:  umsg("user_unload_spindle_nonran_tc tno=%d"%tno)
        if TMP['P']  != "0": umsg("user_unload_spindle_nonran_tc P=%s"%TMP['P'])

        stop_tool_timer(spindle_tool)
        nonran_restore_pocket(spindle_tool)
        spindle_tool = -1 # indicate no tool in spindle

        save_tools_to_file(db_savefile,"tno:%3d (unload from spindle)(empty)"%tno)
    mutex.release()

#   'l' interface command (ran toolchanger)
def user_load_spindle_ran_tc(tno,params):
    mutex.acquire() # blocking
    global spindle_tool
    #msg("user_load_spindle_ran_tc   'l %s' (spindle_tool=%d)"%(params,spindle_tool))
    check_params(tno,params)
    D   = toolline_to_dict(tools[tno],all_letters)
    TMP = toolline_to_dict(params,['T','P'])
    D['T'] = TMP['T']
    D['P'] = TMP['P']

    if spindle_tool != -1: # unexpected: no tool in spindle
        stop_tool_timer(spindle_tool)
        umsg("user_load_spindle_ran_tc spindle_tool=%d"%spindle_tool)
    spindle_tool = tno
    if tno != 0: start_tool_timer(spindle_tool)
    tools[tno] = dict_to_toolline(D,all_letters)

    pno = int(D['P'])
    txt = "tno:%3d --> pno:%3d (  load to   spindle)"%(tno,pno)
    if tno==0 and pno==0:
        txt = "tno:%3d --> pno:%3d (  load t0   spindle)(empty)"%(tno,pno)
    save_tools_to_file(db_savefile,txt)
    mutex.release()

#   'u' interface command (ran toolchanger)
def user_unload_spindle_ran_tc(tno,params):
    mutex.acquire() # blocking
    global spindle_tool
    #msg("user_unload_spindle_ran_tc 'u %s' (spindle_tool=%d)"%(params,spindle_tool))
    check_params(tno,params)
    TMP = toolline_to_dict(params,['T','P'])
    if spindle_tool == -1: # no tool in spindle
        if tno != 0:
            umsg("user_unload_spindle_ran_tc spindle_tool=%d tno=%d"%(spindle_tool,tno))
        D = dict()
        D['T'] = TMP['T']
        D['P'] = TMP['P']
        tools[0] = dict_to_toolline(D,['T','P'])
    else:
        if spindle_tool != 0: stop_tool_timer(spindle_tool)
        D = toolline_to_dict(tools[spindle_tool],all_letters)
        D['T'] = TMP['T']
        D['P'] = TMP['P']
        tools[tno] = dict_to_toolline(D,all_letters)
        spindle_tool = -1 # indicate no tool in spindle

    pno = int(D['P'])
    txt="tno:%3d --> pno:%3d (unload from spindle)"%(tno,pno)
    if tno==0:
        txt="tno:%3d --> pno:%3d (unload from spindle)(notool)"%(tno,pno)
    save_tools_to_file(db_savefile,txt)
    mutex.release()

#-----------------------------------------------------------
start_db_program() # handle random/nonrandom toolchanger, args 

# begin interface to LinuxCNC
debug = 0 # debug var for command line testing
if (len(sys.argv) > 1 and sys.argv[1] == 'debug'): debug = 1

toollist = make_tools(toolno_min,toolno_max)

if random_toolchanger: tooldb_callbacks(user_get_tool,
                                        user_put_tool,
                                        user_load_spindle_ran_tc,
                                        user_unload_spindle_ran_tc)
else: tooldb_callbacks(user_get_tool,
                       user_put_tool,
                       user_load_spindle_nonran_tc,
                       user_unload_spindle_nonran_tc)

tooldb_tools(toollist)

#-----------------------------------------------------------
# start task threads

active_threads=[]
def start_thread(tname):
    name = tname.__name__
    if name in active_threads:
        umsg("start_thread: %s already running"%name)
        return
    active_threads.append(name)
    msg("**Start: %s"%name)
    msg("**Active threads:%s"%active_threads)
    thd = threading.Thread(target=tname,args=(disconnect_evt,),daemon=0)
    thd.start()
    return thd

def cmd_loop(*args):
    try:
        tooldb_loop()  # loop forever, use callbacks
    except BrokenPipeError:
        handle_disconnect(db_savefile)
    except Exception as e:
        if sys.stdin.isatty():
            msg(("Exception=%s"%str(e)))
        else: pass # avoid messages at termination

# start cmd_loop thread (primary interface to LinuxCNC):
threading.Thread(target=cmd_loop,args=(disconnect_evt,),daemon=1).start()

# task to demonstrate periodic updates to database
def periodic_task(fini):
    if fini.is_set(): return # end task
    global start_time, elapsed_time, spindle_tool
    while 1:
        time.sleep(period_minutes*60)
        mutex.acquire() # blocking
        if spindle_tool > 0: update_tool_time(spindle_tool)
        mutex.release()

threading.Thread(target=periodic_task,args=(disconnect_evt,),daemon=1).start()
#-----------------------------------------------------------
# begin worker demo tasks

def demo_add_or_rm_dbtool(tno):
    # remove tool if it is  in toollist
    # add tool if it is not in toollist
    if tno in toollist:
        D = toolline_to_dict(tools[tno],tletters)
        pno = int(D['P'])
        if rm_dbtool(tno):
            release_pocket(pno)
            return 1 # success
    else:
        # supply some dummy parameter values based on tno
        if add_dbtool(tno,Z=tno/100,D=tno/1000): return 1 # success
    return 0 # fail

# tool_update_task: demonstrate addition/removal of toolno.s
def tool_update_task(fini):
    ct_max = 2
    wait_secs = 5 # interval between adds/removes
    begin_tno = 40 # start of extra tool numbers

    tno = begin_tno; ct = 0; waited = 0 ; delta_secs = 1
    msg("**Begin tool_update_task begin_tno=%d ct_max=%d wait_secs=%d"%
        (begin_tno,ct_max,wait_secs))
    while ct < ct_max: # loop adding/removing some tool no.s
        if fini.is_set(): return # end task
        time.sleep(delta_secs); waited += delta_secs
        if waited < wait_secs: continue
        if not mutex.acquire(timeout=1):
            msg("tool_update_task ct=%d waiting for mutex"%ct)
            continue
        demo_add_or_rm_dbtool(tno)
        mutex.release()
        ct += 1; waited = 0
        tno = begin_tno + ct%n_extra_pockets
    active_threads.remove('tool_update_task')
    msg("**End tool_update_task ct=%d"%ct)

# tool_modify_task: demonstrate changing a tool parameter
def tool_modify_task(fini):
    ct_max = 2
    wait_secs = 5 # interval between tool modifications
    letter = 'A'
    modify_tnos = [15,16]

    if not hasattr(tool_modify_task,"ct"): tool_modify_task.ct=0
    tno = modify_tnos[tool_modify_task.ct]
    tool_modify_task.ct = (tool_modify_task.ct +1)%len(modify_tnos);

    ct = 0; waited = 0 ; delta_secs = 1
    msg("**Begin tool_modify_task tno=%d letter=%s ct_max=%d wait_secs=%d"%
       (tno,letter,ct_max,wait_secs))
    while ct < ct_max: # loop adding/removing some tool no.s
        if fini.is_set(): return # end task
        time.sleep(delta_secs); waited += delta_secs
        if waited < wait_secs: continue
        waited = 0
        if not sync_allowed():
            msg("tool_modify_task: DISALLOWED tno=%d (%s)"%(tno,sync_fail_reason))
            continue
        if not mutex.acquire(timeout=1):
            msg("tool_modify_task ct=%d waiting for mutex"%ct)
            continue
        D = toolline_to_dict(tools[tno],all_letters)
        if spindle_tool != tno:
            D[letter] = "0.1%02d"%ct # modify letter parameter
            tools[tno] = dict_to_toolline(D,all_letters)
            save_tools_to_file(db_savefile,"tno:%3d modified param %s=%s"%(tno,letter,D[letter]))
            msg("tool_modify_task: tno=%d %s=%s"%(tno,letter,D[letter]));
            linuxcnc_tool_sync()
            ct += 1
        else:
            msg("tool_modify_task: DISALLOWED tno=%d (%s)"%(tno,"tool is loaded"))
        mutex.release()
    active_threads.remove('tool_modify_task')
    msg("**End tool_modify_task ct=%d"%ct)

#-----------------------------------------------------------
# start worker tasks
time.sleep(1) # wait for startup messages from LinuxCNC and gui to pass
if cmdline_only: # for gui, start worker threads:
    msg("cmdline debug (CTRL-C to exit)")
else:
    msg("\n\nNOTE: Terminal command to monitor database file:\n     $ watch -c -n 1 cat %s\n"%db_savefile)
    thd1 = start_thread(tool_modify_task)
    thd1.join() # comment this to make tasks concurrent
    thd2 = start_thread(tool_update_task)

#-----------------------------------------------------------
# After all worker threads started, allow signals to start tasks.
# typ: kill -USR[12] $(pidof -x db_nonran.py)
def start_m_thread(*args):start_thread(tool_modify_task)
def start_u_thread(*args):start_thread(tool_update_task)
signal.signal(signal.SIGUSR1, start_m_thread)
signal.signal(signal.SIGUSR2, start_u_thread)
#-----------------------------------------------------------
disconnect_evt.wait()
msg("bye")
