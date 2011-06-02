import sys
from InterpMod import *

# [RS274NGC]
# import Python module(s) from this directory
# PYDIR=pysubs
# import the following Python modules
# PYIMPORT=pythonsubs.py
# any callables in this module will be available as
# o<callable_name> call or handler names.
#
# to remap Tx (prepare) to an NGC file 'prepare.ngc', incantate like so:
#
# REMAP=T argspec=T-  prolog=prepare_prolog epilog=prepare_epilog ngc=prepare
# This means:
#
# argspec=T- :
#     for T, a mandatory parameter word is required, others are ignored
#
# ngc=prepare
#     your O-word procedure goes to prepare.ngc
#
# prolog=prepare_prolog
#     before calling prepare.ngc, execute the Python function 'prepare_prolog'
#
# epilog=prepare_epilog
#     after calling prepare.ngc, execute the Python function 'prepare_epilog'


def prepare_prolog(userdata,**words):
	toolno = words['t']
	(status,pocket) = interp.find_tool_pocket(toolno)
	if status != INTERP_OK:
		interp.set_errormsg("T%d: pocket not found" % (toolno))
		return (status,)

	# these variables will be visible in the following ngc oword sub
	# as #<tool> and #<pocket> as local variables
	interp.params["tool"] = toolno
	interp.params["pocket"] = pocket
	return (INTERP_OK,)

# The actual ngc procedure looks like so:
#
# o<r_prepare> sub
# (debug, r_prepare call_level= #<_call_level> remap_level=#<_remap_level> tool=#<tool> pocket=#<pocket>)
#
# 	; show aborting a prepare on tool 2 by returning a negative value
# 	o<testabort> if [#<tool> EQ 2]
# 		     o<testabort> return [-1]
# 	o<testabort> endif
#
# o<r_prepare> endsub [#<pocket>]
# m2
def prepare_epilog(userdata,**words):
	retval = interp.return_value
	if retval > 0:
		interp.selected_pocket = int(retval)
		CanonMod.SELECT_POCKET(int(retval))
	else:
		CanonMod.INTERP_ABORT(int(retval),"T%d: aborted (return code %.4f)" % (words['t'],retval))
	return (INTERP_OK,)


#
# M6 remapped to a NGC handler, with Python prolog+epilog
#
# Incantation:
# REMAP=M6   modalgroup=6  argspec=-     prolog=change_prolog ngc=change epilog=change_epilog
#
def change_prolog(userdata,**words):
	if interp.selected_pocket < 0:
		interp.set_errormsg("Need tool prepared -Txx- for toolchange")
		return (INTERP_ERROR,)
	if interp.cutter_comp_side:
		interp.set_errormsg("Cannot change tools with cutter radius compensation on")
		return (INTERP_ERROR,)
	interp.params["tool_in_spindle" ] = interp.current_tool

	# bug in interp_convert.cc: WONT WORK - isnt valid anymore
	## 	    settings->selected_pocket);
	## 	    settings->tool_table[0].toolno, <--- BROKEN
	## 	    block->t_number,
	#interp.params["prepared" ] = 2

	interp.params["pocket" ] = interp.selected_pocket
	return (INTERP_OK,)

def change_epilog(userdata,**words):
	retval = interp.return_value
	if retval > 0:
		# commit change
		CanonMod.CHANGE_TOOL(interp.selected_pocket)
		interp.current_pocket = interp.selected_pocket
		# cause a sync()
		interp.tool_change_flag = True
		interp.set_tool_parameters();
	else:
		# abort
		CanonMod.INTERP_ABORT(int(retval),"M6 aborted (return code %.4f)" % (retval))
	return (INTERP_OK,)


# M61 remapped to an all-Python handler
# demo - this really does the same thing as the builtin (non-remapped) M61
#
# Incantation:
#
# REMAP=M61  modalgroup=6  argspec=Q-  python=set_tool_number
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
		return (status,)
	if words['q'] > -TOLERANCE_EQUAL:
		interp.current_pocket = pocket
		CanonMod.CHANGE_TOOL_NUMBER(pocket)
		# cause a sync()
		interp.tool_change_flag = True
		return (INTERP_OK,)
	else:
		interp.set_errormsg("M61 failed: Q=%4" % (toolno))
		return (INTERP_ERROR,)

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
		return (INTERP_OK,) # done
	else:
		# wait for digital-input 00 to go hi for 5secs
		CanonMod.WAIT(0,1,2,5.0)
		# pls call again after sync() with new userdata value
		return (INTERP_EXECUTE_FINISH,userdata + 1)


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
	print "blocks[0].line_number=",interp.blocks[0].line_number
	print "blocks[0].p_flag=",interp.blocks[0].p_flag
	print "blocks[0].p_number=",interp.blocks[0].p_number
	print "blocks[0].q_flag=",interp.blocks[0].q_flag
	print "blocks[0].q_number=",interp.blocks[0].q_number
	print "blocks[0].comment=",interp.blocks[0].comment

	printobj(interp,"interp")
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

	return (INTERP_OK,)

def null(args,**kwargs):
	return (INTERP_OK,)

def callstack():
	for i in range(len(interp.sub_context)):
		print "-------- call_level: ",i
		print "position=",interp.sub_context[i].position
		print "sequence_number=",interp.sub_context[i].sequence_number
		print "filenameposition=",interp.sub_context[i].filename
		print "subname=",interp.sub_context[i].subname
		print "context_status=",interp.sub_context[i].context_status
	return (INTERP_OK,)
