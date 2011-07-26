import os
import signal

from interpreter import *
import canon

#
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
	(status,pocket) = this.find_tool_pocket(tool)
	if status != INTERP_OK:
		this.set_errormsg("T%d: pocket not found" % (tool))
		return status
	# these variables will be visible in the following ngc oword sub
	# as #<tool> and #<pocket> as local variables
	this.params["tool"] = tool
	this.params["pocket"] = pocket
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
	retval = this.return_value
	if retval > 0:
		this.selected_pocket = int(retval)
		canon.SELECT_POCKET(int(retval),int(words['t']))
		return INTERP_OK
	else:
		this.set_errormsg("T%d: aborted (return code %.4f)" % (int(words['t']),retval))
		return INTERP_ERROR


#
# M6 remapped to a NGC handler, with Python prolog+epilog
#
# Incantation:
# REMAP=M6   modalgroup=6  argspec=-     prolog=change_prolog ngc=change epilog=change_epilog
#
def change_prolog(userdata,**words):
	if this.selected_pocket < 0:
		this.set_errormsg("Need tool prepared -Txx- for toolchange")
		return INTERP_ERROR
	if this.cutter_comp_side:
		this.set_errormsg("Cannot change tools with cutter radius compensation on")
		return INTERP_ERROR

	# bug in interp_convert.cc: WONT WORK - isnt valid anymore
	## 	    settings->selected_pocket);
	## 	    settings->tool_table[0].toolno, <--- BROKEN
	## 	    block->t_number,
	#this.params["prepared" ] = 2

	this.params["tool_in_spindle"] = this.current_tool
	this.params["selected_pocket"] = this.selected_pocket
	return INTERP_OK

def change_epilog(userdata,**words):
	retval = this.return_value
	print "change_epilog retval=",retval
	if retval > 0:
		# commit change
		canon.CHANGE_TOOL(this.selected_pocket)
		this.current_pocket = this.selected_pocket
		# cause a sync()
		this.tool_change_flag = True
		this.set_tool_parameters()
		return INTERP_OK
	else:
		this.set_errormsg("M6 aborted (return code %.4f)" % (retval))
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
	(status,pocket) = this.find_tool_pocket(toolno)
	if status != INTERP_OK:
		this.set_errormsg("M61 failed: requested tool %d not in table" % (toolno))
		return status
	if words['q'] > -TOLERANCE_EQUAL:
		this.current_pocket = pocket

		canon.CHANGE_TOOL_NUMBER(pocket)
		# test: this.tool_table[0].offset.tran.z = this.tool_table[pocket].offset.tran.z
		# cause a sync()
		this.tool_change_flag = True
		this.set_tool_parameters()
		return INTERP_OK
	else:
		this.set_errormsg("M61 failed: Q=%4" % (toolno))
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
		pin_status = canon.GET_EXTERNAL_DIGITAL_INPUT(0,0);
		print "pin status=",pin_status
		return INTERP_OK # done
	else:
		# wait for digital-input 00 to go hi for 5secs
		canon.WAIT(0,1,2,5.0)
		# pls call again after sync() with new userdata value
		return (INTERP_EXECUTE_FINISH,userdata + 1)

#------ demonstrate task signal handlers --
def gen_backtrace(userdata,**words):
	if  'emctask' in sys.builtin_module_names:
		os.kill(os.getpid(), signal.SIGUSR2)
	return INTERP_OK

def gdb_window(userdata,**words):
	if  'emctask' in sys.builtin_module_names:
		os.kill(os.getpid(), signal.SIGUSR1)
	return INTERP_OK

#----------------  debugging fluff ----------
# named parameters table
def symbols(userdata, **words):
	this.print_named_params(words.has_key('p'))
	return INTERP_OK

# tool table access
def print_tool(userdata, **words):
	n = 0
	if words['p']:
		n = int(words['p'])
	print "tool %d:" % (n)
	print "tool number:", this.tool_table[n].toolno
	print "tool offset x:", this.tool_table[n].offset.tran.x
	print "tool offset y:", this.tool_table[n].offset.tran.y
	print "tool offset z:", this.tool_table[n].offset.tran.z
	return INTERP_OK

def set_tool_zoffset(userdata, **words):
	n = int(words['p'])
	this.tool_table[n].offset.tran.z = words['q']
	if n == 0:
		this.set_tool_parameters()
	return INTERP_OK


def printobj(b,header=""):
	print "object ",header,":"
	for a in dir(b):
		if not a.startswith('_'):
			if hasattr(b,a):
				print a,getattr(b,a)

def introspect(args,**kwargs):
	print "----- introspect:"
	r = this.remap_level
	print "call_level=",this.call_level, "remap_level=",this.remap_level
	print "selected_pocket=",this.selected_pocket
	print "blocks[r].comment=",this.blocks[r].comment
	print "blocks[r].seq=",this.blocks[r].line_number
	print "blocks[r].p_flag=",this.blocks[r].p_flag
	print "blocks[r].p_number=",this.blocks[r].p_number
	print "blocks[r].q_flag=",this.blocks[r].q_flag
	print "blocks[r].q_number=",this.blocks[r].q_number

	#printobj(interp,"interp")
	printobj(this.tool_offset,"tool_offset")
	callstack()
	for i in [5220,"_metric","_absolute","_tool_offset","_feed","_rpm"]:
		print "param",i,"=",this.params[i]
	print "blocks[r].executing_remap:",
	print "name=",this.blocks[r].executing_remap.name
	print "argspec=",this.blocks[r].executing_remap.argspec
	print "prolog=",this.blocks[r].executing_remap.prolog_func
	print "py=",this.blocks[r].executing_remap.remap_py
	print "ngc=",this.blocks[r].executing_remap.remap_ngc
	print "epilog=",this.blocks[r].executing_remap.epilog_func

	return INTERP_OK

def null(args,**kwargs):
	return INTERP_OK


def callstack():
	for i in range(len(this.sub_context)):
		print "-------- call_level: ",i
		print "position=",this.sub_context[i].position
		print "sequence_number=",this.sub_context[i].sequence_number
		print "filenameposition=",this.sub_context[i].filename
		print "subname=",this.sub_context[i].subname
		print "context_status=",this.sub_context[i].context_status
	return INTERP_OK

