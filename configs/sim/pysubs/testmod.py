import sys

def pytdemo(args,**words):
	print >> sys.stderr, "executing Python function: %s.%s" % (globals()['__name__'],sys._getframe(0).f_code.co_name)
	for i in range(5):
		print >> sys.stderr, "args[%d] = %f" % (i,args[i])
	return args[1] # return pocket number (#2) to commit or -1 to fail


def pym6demo(args,**words):
	print >> sys.stderr, "executing Python function: %s.%s " % (globals()['__name__'],sys._getframe(0).f_code.co_name)
	for i in range(5):
		print >> sys.stderr, "args[%d] = %f" % (i,args[i])
	return -815.0  # commit change to prepped tool
	#return 1.0  # commit change to prepped tool

def pym61demo(args,**words):
	print >> sys.stderr, "executing Python function: %s.%s" % (globals()['__name__'],sys._getframe(0).f_code.co_name)
	for i in range(5):
		print >> sys.stderr, "args[%d] = %f" % (i,args[i])
	for key in words:
		print >> sys.stderr, "word '%s' = %f" % (key, words[key])
	return args[0] # set tool number from Q param to commit or negative value to fail

def g886(params,**words):
	# an absolute move
	CanonMod.STRAIGHT_TRAVERSE(InterpMod.interp.sequence_number(),
				   words['x'],words['y'],words['z'],
				   0,0,0,0,0,0)

# the oword params #1..#30 are not used in remapped codes
# any words as per argspec come through the words dict
def m310(params,**words):
	for key in words:
		CanonMod.MESSAGE("word '%s' = %s" % (key, str(words[key])))

	# access into Interp
	line = InterpMod.interp.sequence_number()
	CanonMod.MESSAGE("current line= " + str(line))

	# demo queued canon interface
	if (words.has_key('r')):
		if words['r'] > 0:
			CanonMod.FLOOD_ON()
		else:
			CanonMod.FLOOD_OFF()


print >> sys.stderr, "--- testmod imported"
#print "dir(InterpMod.interp)=",dir(InterpMod.interp)
#print "dir(CanonMod)=",dir(CanonMod)
