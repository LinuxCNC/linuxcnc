import sys

INTERP_OK = 0
INTERP_EXIT =  1
INTERP_EXECUTE_FINISH = 2
INTERP_ENDFILE = 3
INTERP_FILE_NOT_OPEN = 4
INTERP_ERROR =  5



# [RS274NGC]
# import Python module(s) from this directory
# PYDIR=pysubs
# import the following Python modules
# PYIMPORT=pythonsubs.py
# any callables in this module will be available as
# o<callable_name> call or handler names.
#
# to remap Tx (prepare) to an NGC file, incantate like so:
#
# REMAP=T argspec=T-  prolog=p_prepare epilog=e_prepare ngc=r_prepare

# prolog for prepare NGC file
def p_prepare(userdata,**words):
	i = InterpMod.interp
	toolno = words['t']
	p = InterpMod.params
	(status,pocket) = i.find_tool_pocket(toolno)
	if status != INTERP_OK:
		i.push_errormsg("T%d: pocket not found" % (toolno))
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

# epilog for prepare NGC file
def e_prepare(userdata,**words):
	i = InterpMod.interp
	retval = InterpMod.interp.return_value
	if retval > 0:
		InterpMod.interp.selected_pocket = int(retval)
		CanonMod.SELECT_POCKET(int(retval))
	else:
		CanonMod.INTERP_ABORT(int(retval),"e_prepare abort")
	return (INTERP_OK,)
