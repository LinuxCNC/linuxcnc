import sys

# A python interface for LinuxCNC tool database usage

#Notes
#  1) host (linuxCNC) pushes commands to this program
#  2) all writes to stdout go to host
#     (use stderr for debug prints)
#-----------------------------------------------------------
DB_VERSION = 'v1.0'

# Required data functions to be provided by user:
def tooldb_callbacks(tool_getter,tool_putter):
    global get_tool, put_tool
    get_tool = tool_getter
    put_tool = tool_putter

def tooldb_tools(tool_list):
    global tools
    tools = tool_list

#-----------------------------------------------------------
# Interface functions
def do_reply(msg):
    sys.stdout.write("%s\n"%msg)
    sys.stdout.flush()

def nak_reply(msg):
    global theline
    sys.stdout.write("NAK %s <%s>\n"%(msg,theline))
    sys.stdout.flush()

def tool_cmd(cmd,params):
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
        try: do_reply(get_tool(tno))
        except:
            if (tno==0):
                if debug: sys.stderr.write("no tool in spindle\n")
                pass
    do_reply("FINI (get_cmd)")

def put_cmd(cmd,params):
    # note: checks are mainly for commandline debugging,
    #       expect host to always provide a valid toolline
    uparams = params.upper()
    if len(uparams) == 0:
        raise Exception("put_cmd: requires tool entry line")
    try:
        toolno   = int(uparams[0:uparams.index(" ")].strip("T"))
        toolline = uparams
    except Exception as e:
        sys.stderr.write("put_cmd: %s:\n"%e)
        raise Exception("put_cmd: failed to parse <%s>"%params)
    # require at least "Tvalue Pvalue"
    try: toolline.index("T");toolline.index(" P")
    except Exception as e:
        sys.stderr.write("put_cmd: %s:\n"%e)
        raise Exception("put_cmd: failed to parse <%s>"%params)

    put_tool(toolno,toolline)
    do_reply("FINI (update recvd) %s"%params)

def unknown_cmd(cmd,params):
    global theline
    nak_reply("unknown cmd")
    return
    
def do_cmd(line):
    global debug
    if debug: sys.stderr.write("!!line=<%s>\n"%line)
    linelist = line.strip().split()
    cmd = linelist[0]
    params = ""
    if len(linelist) > 1:
        params = line.strip()[line.index(" "):].strip()
    switcher = {
               "g": get_cmd,  # get (all tools)
               "p": put_cmd,  # put (update one tool)
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
global get_tool
global tools
theline = ""

def tooldb_loop():
    startup_ack()
    global theline
    while True:
        try:
            theline=sys.stdin.readline().strip()
            if (theline == ""): nak_reply("empty line")
            else:               do_cmd(theline)
        except Exception as e:
            nak_reply("_exception=%s"%e)
