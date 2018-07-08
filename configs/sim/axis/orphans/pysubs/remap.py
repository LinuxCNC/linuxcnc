#   This is a component of LinuxCNC
#   Copyright 2011, 2013, 2014 Dewey Garrett <dgarrett@panix.com>,
#   Michael Haberler <git@mah.priv.at>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
import os
import signal

from interpreter import *
import emccanon

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
def prepare_prolog(self, userdata,**words):
	self.params[5599] = 1  # turn on DEBUG, output
	self._tool = int(words['t'])
	if self._tool:
		(status,self._pocket) = self.find_tool_pocket(self._tool)
		if status != INTERP_OK:
			self.set_errormsg("T%d: pocket not found" % (self._tool))
			return status
	else:
		self._pocket = -1 # this is an T0 - tool unload

	# these variables will be visible in the ngc oword sub
	# as #<tool> and #<pocket> as local variables
	self.params["tool"] = self._tool
	self.params["pocket"] = self._pocket
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
def prepare_epilog(self, userdata, **words):
	#print "prepare_epilog cl=",self.call_level, self._pocket
	retval = self.return_value
	if retval >= 0:
		self.selected_pocket = self._pocket
		self.selected_tool = self._tool
		emccanon.SELECT_POCKET( self._pocket, self._tool)
		return INTERP_OK
	else:
		self.set_errormsg("T%d: aborted (return code %.4f)" % (self._tool,retval))
		return INTERP_ERROR


#
# M6 remapped to a NGC handler, with Python prolog+epilog
#
# Incantation:
# REMAP=M6   modalgroup=6  argspec=-     prolog=change_prolog ngc=change epilog=change_epilog
#
def change_prolog(self, userdata,**words):
	if self.selected_pocket < 0:
		self.set_errormsg("Need tool prepared -Txx- for toolchange")
		return INTERP_ERROR
	if self.cutter_comp_side:
		self.set_errormsg("Cannot change tools with cutter radius compensation on")
		return INTERP_ERROR

	# bug in interp_convert.cc: WONT WORK - isnt valid anymore
	## 	    settings->selected_pocket);
	## 	    settings->tool_table[0].toolno, <--- BROKEN
	## 	    block->t_number,
	#self.params["prepared" ] = 2

	self.params["tool_in_spindle"] = self.current_tool
	self.params["selected_pocket"] = self.selected_pocket
	return INTERP_OK

def change_epilog(self, userdata,**words):
	retval = self.return_value
	print "change_epilog retval=% selected_pocket=%d" %(retval,self.selected_pocket)
	if retval > 0:
		# commit change
		#emccanon.CHANGE_TOOL(self.selected_pocket)
		emccanon.CHANGE_TOOL(self.selected_tool)
		self.current_pocket = self.selected_pocket
		# cause a sync()
		self.tool_change_flag = True
		self.set_tool_parameters()
		return INTERP_OK
	else:
		self.set_errormsg("M6 aborted (return code %.4f)" % (retval))
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
def set_tool_number(self, userdata,**words):
	toolno = int(words['q'])
	(status,pocket) = self.find_tool_pocket(toolno)
	if status != INTERP_OK:
		self.set_errormsg("M61 failed: requested tool %d not in table" % (toolno))
		return status
	if words['q'] > -TOLERANCE_EQUAL:
		self.current_pocket = pocket

		emccanon.CHANGE_TOOL_NUMBER(pocket)
		# test: self.tool_table[0].offset.tran.z = self.tool_table[pocket].offset.tran.z
		# cause a sync()
		self.tool_change_flag = True
		self.set_tool_parameters()
		return INTERP_OK
	else:
		self.set_errormsg("M61 failed: Q=%4" % (toolno))
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
def test_reschedule(self, userdata,**words):
	if userdata > 0:
		# we were called post-sync():
		pin_status = emccanon.GET_EXTERNAL_DIGITAL_INPUT(0,0);
		print "pin status=",pin_status
		return INTERP_OK # done
	else:
		# wait for digital-input 00 to go hi for 5secs
		emccanon.WAIT(0,1,2,5.0)
		# pls call again after sync() with new userdata value
		return (INTERP_EXECUTE_FINISH,userdata + 1)

#------ demonstrate task signal handlers --
def gen_backtrace(self, userdata,**words):
	if  'emctask' in sys.builtin_module_names:
		os.kill(os.getpid(), signal.SIGUSR2)
	return INTERP_OK

def gdb_window(self, userdata,**words):
	if  'emctask' in sys.builtin_module_names:
		os.kill(os.getpid(), signal.SIGUSR1)
	return INTERP_OK

#----------------  debugging fluff ----------
# named parameters table
def symbols(self, userdata, **words):
	self.print_named_params(words.has_key('p'))
	return INTERP_OK

# tool table access
def print_tool(self, userdata, **words):
	n = 0
	if words['p']:
		n = int(words['p'])
	print "tool %d:" % (n)
	print "tool number:", self.tool_table[n].toolno
	print "tool offset x:", self.tool_table[n].offset.tran.x
	print "tool offset y:", self.tool_table[n].offset.tran.y
	print "tool offset z:", self.tool_table[n].offset.tran.z
	return INTERP_OK

def set_tool_zoffset(self, userdata, **words):
	n = int(words['p'])
	self.tool_table[n].offset.tran.z = words['q']
	if n == 0:
		self.set_tool_parameters()
	return INTERP_OK


def printobj(b,header=""):
	print "object ",header,":"
	for a in dir(b):
		if not a.startswith('_'):
			if hasattr(b,a):
				print a,getattr(b,a)

def introspect(args,**kwargs):
	print "----- introspect:"
	r = self.remap_level
	print "call_level=",self.call_level, "remap_level=",self.remap_level
	print "selected_pocket=",self.selected_pocket
	print "blocks[r].comment=",self.blocks[r].comment
	print "blocks[r].seq=",self.blocks[r].line_number
	print "blocks[r].p_flag=",self.blocks[r].p_flag
	print "blocks[r].p_number=",self.blocks[r].p_number
	print "blocks[r].q_flag=",self.blocks[r].q_flag
	print "blocks[r].q_number=",self.blocks[r].q_number

	#printobj(interp,"interp")
	printobj(self.tool_offset,"tool_offset")
	callstack()
	for i in [5220,"_metric","_absolute","_tool_offset","_feed","_rpm"]:
		print "param",i,"=",self.params[i]
	print "blocks[r].executing_remap:",
	print "name=",self.blocks[r].executing_remap.name
	print "argspec=",self.blocks[r].executing_remap.argspec
	print "prolog=",self.blocks[r].executing_remap.prolog_func
	print "py=",self.blocks[r].executing_remap.remap_py
	print "ngc=",self.blocks[r].executing_remap.remap_ngc
	print "epilog=",self.blocks[r].executing_remap.epilog_func

	return INTERP_OK

def null(args,**kwargs):
	return INTERP_OK


def callstack():
	for i in range(len(self.sub_context)):
		print "-------- call_level: ",i
		print "position=",self.sub_context[i].position
		print "sequence_number=",self.sub_context[i].sequence_number
		print "filenameposition=",self.sub_context[i].filename
		print "subname=",self.sub_context[i].subname
		print "context_status=",self.sub_context[i].context_status
	return INTERP_OK

