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

def prepare_epilog(userdata,**words):
	i = InterpMod.interp
	retval = InterpMod.interp.return_value
	if retval > 0:
		InterpMod.interp.selected_pocket = int(retval)
		CanonMod.SELECT_POCKET(int(retval))
	else:
		CanonMod.INTERP_ABORT(int(retval),"T%d: aborted (return code %.1f)" % (words['t'],retval))
	return (INTERP_OK,)



# // int Interp::finish_m6_command(setup_pointer settings, block_pointer r_block)
# // {
# //     // if M6_COMMAND 'return'ed or 'endsub'ed a #<_value> > 0,
# //     // commit the tool change
# //     if (settings->return_value >= TOLERANCE_EQUAL) {
# // 	CHANGE_TOOL(settings->selected_pocket);
# // 	settings->current_pocket = settings->selected_pocket;
# // 	// this will cause execute to return INTERP_EXECUTE_FINISH
# // 	settings->toolchange_flag = true;
# //     } else {
# // 	char msg[LINELEN];
# // 	snprintf(msg, sizeof(msg), "M6 failed (%f)", settings->return_value);
# // 	INTERP_ABORT(round_to_int(settings->return_value),msg);
# // 	return INTERP_EXECUTE_FINISH;
# //     }
# //     return remap_finished(r_block->executing_remap->op);
# // }

# // int Interp::finish_m61_command(setup_pointer settings,  block_pointer r_block)
# // {
# //     // if M61_COMMAND 'return'ed or 'endsub'ed a #<_value> >= 0,
# //     // set that as the new tool' pocket number
# //     // a negative return value will leave it untouched

# //     if (settings->return_value > - TOLERANCE_EQUAL) {
# // 	settings->current_pocket = round_to_int(settings->return_value);
# // 	CHANGE_TOOL_NUMBER(settings->current_pocket);
# // 	// this will cause execute to return INTERP_EXECUTE_FINISH
# // 	settings->toolchange_flag = true;
# //     } else {
# // 	char msg[LINELEN];
# // 	snprintf(msg,sizeof(msg),"M61 failed (%f)", settings->return_value);
# // 	INTERP_ABORT(round_to_int(settings->return_value),msg);
# // 	return INTERP_EXECUTE_FINISH;
# //     }
# //     return remap_finished(r_block->executing_remap->op);
# // }


# // Tx epiplogue - executed past T_COMMAND
# // int Interp::finish_t_command(setup_pointer settings,   block_pointer r_block)
# // {
# //     // if T_COMMAND 'return'ed or 'endsub'ed a #<_value> >= 0,
# //     // commit the tool prepare to that value.

# //     if (settings->return_value > - TOLERANCE_EQUAL) {
# // 	settings->selected_pocket = round_to_int(settings->return_value);
# // 	SELECT_POCKET(settings->selected_pocket);
# //     } else {

# // 	char msg[LINELEN];
# // 	snprintf(msg, sizeof(msg), "T<tool> - prepare failed (%f)",
# // 		 settings->return_value);
# // 	INTERP_ABORT(round_to_int(settings->return_value),msg);
# // 	return INTERP_EXECUTE_FINISH;
# //     }
# //     return remap_finished(r_block->executing_remap->op);
# // }
