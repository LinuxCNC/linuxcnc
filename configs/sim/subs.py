import sys

# Python functions as o-word subroutines
#
# you can either call this function like so:
# o<m260> call [2] [3] [4]
# (debug, #<_value>) should display 24.0
#
# NB: when calling as oword sub, ONLY positional parameters are passed.
#
# or as a remapped code, to do so, add in ini:
'''
#------- add to ini file ------------
# define custom G and M codes
#
# syntax: GCODE=<number>,<modal group>,<argument spec>
#
# argument spec: 0-n characters of the class [A-KMNP-Za-kmnp-z]
#   an uppercase letter for each required parameter 'word'
#   a lowercase letter for each optional parameter 'word'
#   superfluous words present in the current block which are neither required nor optional cause an error message
#
# an 'S' requires speed > 0
# an 'F' requires feed > 0
#
# if calling a Python function, the actual words present in the block as
# required or optional are passed in the words dictionary; no positional
# parameters are passed in this case.
#
[CUSTOM]
# GCODE: currently supported modal group: 1
# this requires words x,y,z, and a feed > 0; optionally takes p,q,r words:
GCODE=88.5,1,XYZFpqr

# MCODE: currently supported modal groups: 5,6,7,8,9,10
MCODE=270,10,pqr
#------- end add to ini file ------------
'''
# thereafter you can execute
#
# G88.5X10Y20Z30F200
# G88.5X10Y20Z30F200p1q2r3
#
# M260
# M260p1
# M260q2
# M260r3
# M260p1q2r3
# a return value wll be stored in #<_value>:
# (debug,#<_value>)


# test a new Python subroutine like so -
# this is identical to how the call from EMC works:
#
# mah@ubuntu-10:~/emc2-tc/configs/sim$ python
# Python 2.6.5 (r265:79063, Apr 16 2010, 13:09:56)
# [GCC 4.4.3] on linux2
# Type "help", "copyright", "credits" or "license" for more information.
# >>> from subs import *
# >>> m270((1,2,3,4,5),a=10,b=20)
# executing Python function: subs.m270
# params[0] = 1.000000
# params[1] = 2.000000
# params[2] = 3.000000
# params[3] = 4.000000
# params[4] = 5.000000
# word 'a' = 10.000000
# word 'b' = 20.000000
# >>>
#
# to see python stderr output, start emc from a terminal

# positional parameters are available as params[0] .. params[29]
# and are available only on a call like 'o<pyfunctionname> call [...]
#
# if called as a remapped M- or G-code, the words passed from
# the block as filtered by the argspec are passed through the words dict
# and params is meaningless


def m270(params,**words):
	result = 1
	print >> sys.stderr, "executing Python function: %s.%s" % (globals()['__name__'],sys._getframe(0).f_code.co_name)
	for i in range(5):
		print >> sys.stderr, "params[%d] = %f" % (i,params[i])
		if params[i] > 0:
			result *= params[i]
	for key in words:
		print >> sys.stderr, "word '%s' = %f" % (key, words[key])
		if words[key] > 0:
			result *= words[key]
	return result

def g885(params,**words):
	result = 1
	print >> sys.stderr, "executing Python function: %s.%s" % (globals()['__name__'],sys._getframe(0).f_code.co_name)
	for i in range(5):
		print >> sys.stderr, "params[%d] = %f" % (i,params[i])
		if params[i] > 0:
			result *= params[i]
	for key in words:
		print >> sys.stderr, "word '%s' = %f" % (key, words[key])
		if words[key] > 0:
			result *= words[key]
	return result
