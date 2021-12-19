import sys

# A python interface for LinuxCNC tool database usage

#Notes
#  1) host (linuxCNC) pushes commands to this program
#  2) all writes to stdout go to host
#     (use stderr for debug prints)
#-----------------------------------------------------------
DB_VERSION = 'v2.0'

#-----------------------------------------------------------
# functions for import by user
# tooldb_callbacks
# tooldb_tools
# tooldb_loop
#-----------------------------------------------------------

def do_reply(msg):
    sys.stdout.write("%s\n"%msg)
    sys.stdout.flush()

def saveline(line):
    global theline
    theline = line

def currentline():
    global theline
    return theline

def nak_reply(msg):
    theline = currentline()
    sys.stdout.write("NAK %s <%s>\n"%(msg,theline))
    sys.stdout.flush()

def tool_cmd(cmd,params):
    # debug usage: return info for tool
    # parms is toolno
    if not len(params):
        nak_reply("no toolno")
        return
    if len(params.split()) != 1:
        nak_reply("too many parms")
        return
    try:
        toolno = int(params)
    except Exception as e:
        nak_reply("non-integer toolno")
        return
    if toolno not in tools:
        nak_reply("toolno out-of-range")
        return
    do_reply(get_tool(toolno))

def get_cmd(cmd,params):
    for tno in tools:
        try: msg = get_tool(tno)
        except Exception as e:
            nak_reply( "get_cmd():%s"%str(e))
            return
        try: do_reply(msg)
        except:
            if (tno==0):
                if debug: sys.stderr.write("no tool in spindle\n")
                pass
    do_reply("FINI (get_cmd)")

def put_cmds(cmd,params):
    # note: checks are mainly for commandline debugging,
    #       expect host to always provide a valid toolline
    uparams = params.upper()
    if len(uparams) == 0:
        raise Exception("cmd=%s: requires tool entry line"%cmd)
    try:
        toolno   = int(uparams[0:uparams.index(" ")].strip("T"))
        toolline = uparams
    except Exception as e:
        sys.stderr.write("cmd=%s: %s:\n"%(cmd,e))
        raise Exception("cmd=%s: failed to parse <%s>"%(cmd,params))
    # require at least "Tvalue Pvalue"
    try: toolline.index("T");toolline.index(" P")
    except Exception as e:
        sys.stderr.write("cmd=%s: %s:\n"%(cmd,e))
        raise Exception("cmd=%s: failed to parse <%s>"%(cmd,params))

    try:
        if   cmd == "p": put_tool(      toolno,toolline)
        elif cmd == "l": load_spindle(  toolno,toolline)
        elif cmd == "u": unload_spindle(toolno,toolline)
    except Exception as e:
        nak_reply( "cmd=%s: %s"%(cmd,str(e)))
        return
    do_reply("FINI (update recvd) %s"%params)

def unknown_cmd(cmd,params):
    nak_reply("unknown cmd")
    return
    
def do_cmd(line):
    global debug
    if debug: sys.stderr.write("!!line=<%s>\n"%line)
    linelist = line.strip().split()
    cmd = linelist[0]
    params = ""
    if len(linelist) > 1:
        # params is everything after first " ":0
        params = line.strip()[line.index(" "):].strip()
    switcher = {
               "g": get_cmd,   # get (all tools)
               "p": put_cmds,  # put (update one tool offsets)
               "u": put_cmds,  # unload spindle
               "l": put_cmds,  #   load spindle
               "t": tool_cmd, # debug usage
               }
    thecmd = switcher.get(cmd) or unknown_cmd
    thecmd(cmd,params)

def startup_ack():
    global debug
    debug = 0
    do_reply("%s"%DB_VERSION);
    if (len(sys.argv)>1 and sys.argv[1] == 'debug'): debug = 1

#-----------------------------------------------------------
# Begin functions that can be imported

def tooldb_callbacks(tool_getter,tool_putter,spindle_loader,spindle_unloader):
    """Specify callback functions"""
    global get_tool, put_tool, load_spindle, unload_spindle
    get_tool       = tool_getter
    put_tool       = tool_putter
    load_spindle   = spindle_loader
    unload_spindle = spindle_unloader

def tooldb_tools(tool_list):
    """Specify list = available toolnumers"""
    global tools
    tools = tool_list

def tooldb_loop():
    """Loop forever:
       1) send startup acknowlegment
       2) read line from stdino
       3) parse line
             execute command if valid
             nak command     if invalid
       4) repeat
    """
    startup_ack()
    while True:
        try:
            line=sys.stdin.readline().strip()

            saveline(line)
            if (line == ""): nak_reply("empty line")
            else:            do_cmd(line)
        except Exception as e:
            nak_reply("_exception=%s"%e)
