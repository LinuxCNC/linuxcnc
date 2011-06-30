import sys
# InterpMod, CanonMod are automatically imported
from InterpMod import *
import pickle


# to remap Tx (prepare) to an NGC file 'prepare.ngc', incantate like so:
#
# REMAP=T  prolog=prepare_prolog epilog=prepare_epilog ngc=prepare
# This means:
#
# prolog=prepare_prolog
#     before calling prepare.ngc, execute the Python function 'prepare_prolog'
# ngc=prepare
#     your O-word procedure goes to prepare.ngc
# epilog=prepare_epilog
#     after calling prepare.ngc, execute the Python function 'prepare_epilog'
#
def prepare_prolog(userdata,**words):
	tool = int(words['t'])
	(status,pocket) = interp.find_tool_pocket(tool)
	if status != INTERP_OK:
		interp.set_errormsg("T%d: pocket not found" % (tool))
		return status
	# these variables will be visible in the following ngc oword sub
	# as #<tool> and #<pocket> as local variables
	interp.params["tool"] = tool
	interp.params["pocket"] = pocket
	return INTERP_OK

# The minimal ngc prepare procedure looks like so:
#
# o<r_prepare> sub
# (debug, r_prepare tool=#<tool> pocket=#<pocket>)
#
# returning a negative value fails the prepare command
# returning a positive value sets this value as the new pocket:
# o<r_prepare> endsub [#<pocket>]
# m2

# the prepare epilog looks at the return value from the NGC procedure
# and does the right thing:
def prepare_epilog(userdata,**words):
	retval = interp.return_value
	if retval > 0:
		interp.selected_pocket = int(retval)
		interp.selected_tool = int(words['t'])
		CanonMod.SELECT_POCKET(int(retval),int(words['t']))
		return INTERP_OK
	else:
		interp.set_errormsg("T%d: aborted (return code %.4f)" % (int(words['t']),retval))
		return INTERP_ERROR


#
# M6 remapped to a NGC handler, with Python prolog+epilog
#
# Incantation:
# REMAP=M6   modalgroup=6  argspec=-     prolog=change_prolog ngc=change epilog=change_epilog
#
def change_prolog(userdata,**words):
	if interp.selected_pocket < 0:
		interp.set_errormsg("Need tool prepared -Txx- for toolchange")
		return INTERP_ERROR
	if interp.cutter_comp_side:
		interp.set_errormsg("Cannot change tools with cutter radius compensation on")
		return INTERP_ERROR

	# bug in interp_convert.cc: WONT WORK - isnt valid anymore
	## 	    settings->selected_pocket);
	## 	    settings->tool_table[0].toolno, <--- BROKEN
	## 	    block->t_number,
	#interp.params["prepared" ] = 2

	interp.params["tool_in_spindle"] = interp.current_tool
	interp.params["selected_pocket"] = interp.selected_pocket
	return INTERP_OK

def change_epilog(userdata,**words):
	retval = interp.return_value
	print "change_epilog retval=",retval
	if retval > 0:
		# commit change
		CanonMod.CHANGE_TOOL(interp.selected_pocket)
		interp.current_pocket = interp.selected_pocket
		# cause a sync()
		interp.tool_change_flag = True
		interp.set_tool_parameters()
		return INTERP_OK
	else:
		interp.set_errormsg("M6 aborted (return code %.4f)" % (retval))
		return INTERP_ERROR


# M61 remapped to an all-Python handler
# demo - this really does the same thing as the builtin (non-remapped) M61
#
# Incantation:
#
# REMAP=M61    python=set_tool_number
#
# This means:
#
# argspec=Q- :
#     a mandatory Q parameter word is required, others are ignored
#
# python=set_tool_number
#     the following function is executed on M61:
#
def set_tool_number(userdata,**words):
	toolno = int(words['q'])
	(status,pocket) = interp.find_tool_pocket(toolno)
	if status != INTERP_OK:
		interp.set_errormsg("M61 failed: requested tool %d not in table" % (toolno))
		return status
	if words['q'] > -TOLERANCE_EQUAL:
		interp.current_pocket = pocket

		CanonMod.CHANGE_TOOL_NUMBER(pocket)
		# test: interp.tool_table[0].offset.tran.z = interp.tool_table[pocket].offset.tran.z
		# cause a sync()
		interp.tool_change_flag = True
		interp.set_tool_parameters()
		return INTERP_OK
	else:
		interp.set_errormsg("M61 failed: Q=%4" % (toolno))
		return INTERP_ERROR

#
# This demonstrates how queuebusters
# (toolchange, wait for input, probe) can be dealt with in Python handlers.
#
# on the initial call, userdata equals zero.
# if a queuebuster is executed, the function is expected to return
# (INTERP_EXECUTE_FINISH,<optional new userdata value>
#
# Post sync, the function is called again with the userdata value
# returned previously for continuation.
#
def test_reschedule(userdata,**words):
	if userdata > 0:
		# we were called post-sync():
		pin_status = CanonMod.GET_EXTERNAL_DIGITAL_INPUT(0,0);
		print "pin status=",pin_status
		return INTERP_OK # done
	else:
		# wait for digital-input 00 to go hi for 5secs
		CanonMod.WAIT(0,1,2,5.0)
		# pls call again after sync() with new userdata value
		return (INTERP_EXECUTE_FINISH,userdata + 1)


# Demo Python O-word subroutine - call as:
# o<square> [5]
# (debug, #<_value>)
#
# len(args) reflects the number of actual parameters passed
def square(args):
	return args[0]*args[0]


#----------------  exec-time plugin support ----------

def _plugin_call_(s):
    global _execute
    call = pickle.loads(s)
    func = getattr(_execute, call[0], None)
    if func:
	    return func(*call[1],**call[2])
    else:
        raise AttributeError, "no such method: " + call[0]

class EnqueueCall(object):
    def __init__(self,e):
        self.e = e

    def __encode(self,*args,**kwargs):
        if hasattr(self.e,self.name):# and callable(getattr(self.e,self.name)):
            p = pickle.dumps(args)
            k = pickle.dumps(kwargs)
            CanonMod.PLUGIN_CALL(pickle.dumps((self.name,args,kwargs)))
        else:
            raise AttributeError,"no such method: " + self.name

    def __getattr__(self, name):
        self.name = name
        return self.__encode

class Execute(object):

    def demo(self,s):
	    print "demo(%s)" % s
	    return 0 # return -1 to fail execution

_execute = Execute()
_callq = EnqueueCall(_execute)

#----------------  queue a call for task-time execution ------------

def task(args):
	print "python: issuing PLUGIN_CALL"
	_callq.demo(args)

#----------------  debugging fluff ----------

# tool table access
def print_tool(userdata, **words):
	n = 0
	if words['p']:
		n = int(words['p'])
	print "tool %d:" % (n)
	print "tool number:", interp.tool_table[n].toolno
	print "tool offset x:", interp.tool_table[n].offset.tran.x
	print "tool offset y:", interp.tool_table[n].offset.tran.y
	print "tool offset z:", interp.tool_table[n].offset.tran.z

	return INTERP_OK

def set_tool_zoffset(userdata, **words):
	n = int(words['p'])
	interp.tool_table[n].offset.tran.z = words['q']
	if n == 0:
		interp.set_tool_parameters()
	return INTERP_OK


def printobj(b,header=""):
	print "object ",header,":"
	for a in dir(b):
		if not a.startswith('_'):
			if hasattr(b,a):
				print a,getattr(b,a)

def introspect(args,**kwargs):
	print "----- introspect:"
	r = interp.remap_level
	print "call_level=",interp.call_level, "remap_level=",interp.remap_level
	print "selected_pocket=",interp.selected_pocket
	print "blocks[r].comment=",interp.blocks[r].comment
	print "blocks[r].seq=",interp.blocks[r].line_number
	print "blocks[r].p_flag=",interp.blocks[r].p_flag
	print "blocks[r].p_number=",interp.blocks[r].p_number
	print "blocks[r].q_flag=",interp.blocks[r].q_flag
	print "blocks[r].q_number=",interp.blocks[r].q_number

	#printobj(interp,"interp")
	printobj(interp.tool_offset,"tool_offset")
	callstack()
	for i in [5220,"_metric","_absolute","_tool_offset","_feed","_rpm"]:
		print "param",i,"=",interp.params[i]
	print "blocks[r].executing_remap:",
	print "name=",interp.blocks[r].executing_remap.name
	print "argspec=",interp.blocks[r].executing_remap.argspec
	print "prolog=",interp.blocks[r].executing_remap.prolog_func
	print "py=",interp.blocks[r].executing_remap.remap_py
	print "ngc=",interp.blocks[r].executing_remap.remap_ngc
	print "epilog=",interp.blocks[r].executing_remap.epilog_func

	return INTERP_OK

def null(args,**kwargs):
	return INTERP_OK


def callstack():
	for i in range(len(interp.sub_context)):
		print "-------- call_level: ",i
		print "position=",interp.sub_context[i].position
		print "sequence_number=",interp.sub_context[i].sequence_number
		print "filenameposition=",interp.sub_context[i].filename
		print "subname=",interp.sub_context[i].subname
		print "context_status=",interp.sub_context[i].context_status
	return INTERP_OK

