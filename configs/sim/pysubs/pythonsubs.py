import sys
from InterpMod import *

INTERP_OK = 0
INTERP_EXIT =  1
INTERP_EXECUTE_FINISH = 2
INTERP_ENDFILE = 3
INTERP_FILE_NOT_OPEN = 4
INTERP_ERROR =  5
TOLERANCE_EQUAL = 0.0001

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
	p = params
	(status,pocket) = interp.find_tool_pocket(toolno)
	if status != INTERP_OK:
		interp.push_errormsg("T%d: pocket not found" % (toolno))
		return (status,)

	# these variables will be visible in the following ngc oword sub
	# as #<tool> and #<pocket> as local variables
	p["tool"] = toolno
	p["pocket"] = pocket
	return (INTERP_OK,)

# The actual ngc procedure looks like so:
#
# o<r_prepare> sub
# (debug, r_prepare call_level= #<_call_level> remap_level=#<_remap_level>)
# (debug, n_args=#<n_args> tool=#<tool> pocket=#<pocket>)
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

# M61 remapped to a python handler
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
		return (status,)
	if words['q'] > -TOLERANCE_EQUAL:
		interp.current_pocket = toolno
		CanonMod.SELECT_POCKET(toolno)
		# cause a sync()
		interp.tool_change_flag = True
		return (INTERP_OK,)
	else:
		interp.push_errormsg("M61 failed: Q=%4" % (toolno))
		return (INTERP_ERROR,)


# REMAP=M6   modalgroup=6  argspec=-     prolog=change_prolog ngc=change epilog=change_epilog
# (DEBUG, executing M6 O-word sub, tool-in-spindle=#1 prepared=#2 pocket=#3)
def change_prolog(userdata,**words):

	p["tool_in_spindle" ] = 1
	p["prepared" ] = 2
	p["pocket" ] = 3
	return (INTERP_OK,)

def change_epilog(userdata,**words):
	retval = interp.return_value
	if retval > 0:
		# commit change
		CanonMod.CHANGE_TOOL(interp.selected_pocket)
		interp.current_pocket = interp.selected_pocket
		# cause a sync()
		interp.tool_change_flag = True
	else:
		# abort
		CanonMod.INTERP_ABORT(int(retval),"M6 aborted (return code %.4f)" % (retval))
	return (INTERP_OK,)


