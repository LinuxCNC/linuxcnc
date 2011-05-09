import sys
import traceback

#import bullshit

def pym6demo(args,**words):
	print >> sys.stderr, "executing Python function: %s.%s " % (globals()['__name__'],sys._getframe(0).f_code.co_name)
	for i in range(5):
		print >> sys.stderr, "args[%d] = %f" % (i,args[i])
	#return -815.0  # fail
	return 1.0  # commit change to prepped tool

def pym61demo(args,**words):
	print >> sys.stderr, "executing Python function: %s.%s" % (globals()['__name__'],sys._getframe(0).f_code.co_name)
	for i in range(5):
		print >> sys.stderr, "args[%d] = %f" % (i,args[i])
	for key in words:
		print >> sys.stderr, "word '%s' = %f" % (key, words[key])
	return args[0] # set tool number from Q param to commit or negative value to fail

def g886(callargs,**words):
	# an absolute move
	CanonMod.STRAIGHT_TRAVERSE(InterpMod.interp.sequence_number(),
				   words['x'],words['y'],words['z'],
				   0,0,0,0,0,0)

# the oword params #1..#30 are not used in remapped codes
# any words as per argspec come through the words dict
def m310(callargs,**words):
	print >> sys.stderr, "----- m310:"
#	try:
	for key in words:
		CanonMod.MESSAGE("word '%s' = %s" % (key, str(words[key])))

	for i in range(1,5400):
		if abs(InterpMod.params[i]) > 0.1:
			print "param ",i,"=",InterpMod.params[i]

	# access into Interp
	line = InterpMod.interp.sequence_number()
	CanonMod.MESSAGE("current line= " + str(line))
#	InterpMod.interp.params[123] =2.71828
#	CanonMod.MESSAGE("blocktext = " + InterpMod.interp.blocktext)
	CanonMod.MESSAGE("call_level  = " + str(InterpMod.interp.call_level))
	CanonMod.MESSAGE("remap_level  = " + str(InterpMod.interp.remap_level))

	# demo queued canon interface
	if (words.has_key('r')):
		if words['r'] > 0:
			CanonMod.FLOOD_ON()
		else:
			CanonMod.FLOOD_OFF()
			l = 4711 / 0
	# except Exception,err:
	# 	print >> sys.stderr, "----- m310 exception"
	# 	CanonMod.MESSAGE("exception: " + str(err) + ":" + traceback.format_exc())
	# 	raise

def m311(callargs,**words):
	print >> sys.stderr, "----- m311:"
	try:
		print "curp=",	InterpMod.interp.current_pocket
		print "sel=",InterpMod.interp.selected_pocket
		print "tc=",InterpMod.interp.toolchange_flag
		if (words.has_key('r')):
			if words['r'] > 0:
				InterpMod.interp.selected_pocket = int(words['r'])
				InterpMod.interp.toolchange_flag = True
			else:
				InterpMod.interp.current_pocket = int(words['r'])
				InterpMod.interp.toolchange_flag = False
	except Exception,err:
		print >> sys.stderr, "----- m311 exception"
		CanonMod.MESSAGE("exception: " + str(err) + ":" + traceback.format_exc())
		raise

def pytdemo(args,**words):
	print "pytdemo pocket=", args[0]
#	if True: #success
	if args[0] > 2:
		InterpMod.interp.selected_pocket = int(args[0])
		CanonMod.SELECT_POCKET(int(args[0]))
	else:
		CanonMod.INTERP_ABORT(-4711,"pytdemo abort")

	return args[1] # return pocket number (#2) to commit or -1 to fail


print >> sys.stderr, "--- testmod imported"
print "dir(InterpMod)=",dir(InterpMod)
#print "dir(InterpMod.interp)=",dir(InterpMod.interp)
#print "dir(InterpMod.interp.do)=",dir(InterpMod.interp.do)
#print "do = ",InterpMod.interp.do(123)

#print "dir(InterpMod.interp.foo4)=",dir(InterpMod.interp.foo4)
#print "dir(InterpMod.interp.ps)",dir(InterpMod.interp.ps)
#print "dir(InterpMod.interp.invite)",dir(InterpMod.interp.invite)
#print "str(InterpMod.interp.invite)",str(InterpMod.interp.invite)
#print "InterpMod.interp.foo4=",InterpMod.interp.foo4
#print "InterpMod.interp.invite()",InterpMod.interp.invite()
#print "InterpMod.interp.blocktext",InterpMod.interp.blocktext
#print "InterpMod.interp.call_level",InterpMod.interp.call_level
#print "InterpMod.interp.cl()=",InterpMod.interp.cl()

print "dir(InterpMod.params)=",dir(InterpMod.params)

InterpMod.params[123] = 3.14
InterpMod.params['fasfasdf'] = 2.718


#
#print "params[12]=",InterpMod.interp.params[12]
#print "params['foo']=",InterpMod.interp.params['foo']

#print "dir(InterpMod.do)=",dir(InterpMod.foo)
#print "dir(CanonMod)=",dir(CanonMod)


