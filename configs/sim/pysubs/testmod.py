import sys
import traceback

#import bullshit  # test  import failure

#def xxx():        # test syntax failure

class MsgOut(object):
	def __init__(self, f):
		self.data = ''

	def write(self, txt=''):
		self.data += txt
		tmp = str(self.data)
		if '\x0a' in tmp or '\x0d' in tmp:
			tmp = tmp.rstrip('\x0a\x0d')
			CanonMod.MESSAGE(tmp)
			self.data = ''

	def flush(self):
		pass

# Replace stdout with text redirected to operator messages
sys.stdout = MsgOut(sys.__stdout__)

print "CanonMsg, here?"


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
		#	l = 4711 / 0
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

def m312(callargs,**words):
	print "p[4711]=",InterpMod.params[4711]
	InterpMod.params[4711] = 3.1415926

def m313(callargs,**words):
	print "assigning p['_foo']=4711"
	InterpMod.params['_foo'] = 4711.0


def m250(callargs,**words):
	i = InterpMod.interp
	print "py M250 call_level=",i.call_level,"remap_level=",i.remap_level
	#printobj(callargs,"callargs")
	#printobj(words,"words")

def fooprolog(*args,**words):
	print "py fooprolog args=",args
	i = InterpMod.interp
	print "call_level=",i.call_level,"remap_level=",i.remap_level
	for key in words:
		print  "word '%s' = %f" % (key, words[key])

def fooepilog(*args,**words):
	print "py fooepillog args=",args
	i = InterpMod.interp
	print "call_level=",i.call_level,"remap_level=",i.remap_level
	for key in words:
		print  "word '%s' = %f" % (key, words[key])


def whoami(*args,**words):
	print "executing Python function: %s.%s " % (globals()['__name__'],sys._getframe(0).f_code.co_name)
	i = InterpMod.interp
	print "call_level=",i.call_level,"remap_level=",i.remap_level

	print "args = ",args
#	for i in range(5):
#		print "args[%d] = " % (i), args[i]
	for key in words:
		print  "word '%s' = %f" % (key, words[key])

def m314(callargs,**words):
	i = InterpMod.interp
	print "m314:call_level=",i.call_level,"remap_level=",i.remap_level

def m315(callargs,**words):
	i = InterpMod.interp
	c = i.cblock
	e = i.eblock
	#r = i.remap
	print "---> in py.m315()"
	#printobj(i.remap,"Remap:")
#	printobj(c,"cblock")
#	printobj(e,"eblock")


def g887(callargs,**words):
	i = InterpMod.interp
	c = i.cblock
	e = i.eblock
	#print "dir(cblock) =",dir(c)
	print "remap_level =",i.remap_level,"call_level=",i.call_level
	#printobj(c,"cblock")
	#printobj(e,"eblock")



def printobj(b,header=""):
	print "object ",header,":"
	for a in dir(b):
		if not a.startswith('_'):
			if hasattr(b,a):
				print a,getattr(b,a)


def pytdemo(args,**words):
	for i in range(5):
		print >> sys.stderr, "args[%d] = %f" % (i,args[i])
	print >> sys.stderr, "pytdemo pocket=", args[0]
#	if True: #success
	if args[0] > 2:
		InterpMod.interp.selected_pocket = int(args[0])
		CanonMod.SELECT_POCKET(int(args[0]))
	else:
		CanonMod.INTERP_ABORT(-4711,"pytdemo abort")

	return args[1] # return pocket number (#2) to commit or -1 to fail


try:
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
	#print "InterpMod.interp.value=",InterpMod.interp.value
	#InterpMod.interp.value = True
	#print "set true: InterpMod.interp.value=",InterpMod.interp.value

	for i in [5220,"_metric","_absolute","_tool_offset","_feed","_rpm"]:
		print "param",i,"=",InterpMod.params[i]





except Exception,err:
	print "exception: " + str(err) + ":" + traceback.format_exc()




def add_args(code,):
	# access argspec -> need code
	# accesss cblock - ok
	# store into current callframe  dict - dict should be in place
	# report failure with message
	# incicate success to continue
	pass
