import sys

# see http://boost.2283326.n4.nabble.com/Boost-python-exception-translation-failure-on-BlueGene-P-td2702424.html
#import dl
#sys.setdlopenflags(dl.RTLD_NOW | dl.RTLD_GLOBAL)

#import bullshit  # test  import failure
#def xxx():        # test syntax failure

INTERP_OK = 0
INTERP_EXIT =  1
INTERP_EXECUTE_FINISH = 2
INTERP_ENDFILE = 3
INTERP_FILE_NOT_OPEN = 4
INTERP_ERROR =  5


class MsgOut(object):
	def __init__(self, f):
		self.data = ''

	def write(self, txt=''):
		self.data += txt
		tmp = str(self.data)
		if '\x0a' in tmp or '\x0d' in tmp:
			tmp = tmp.rstrip('\x0a\x0d')
			canon.MESSAGE(tmp)
			self.data = ''

	def flush(self):
		pass

# Replace stdout with text redirected to operator messages
#sys.stdout = MsgOut(sys.__stdout__)

#print dir(canon)
#canon.MESSAGE("foomsg")

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
	canon.STRAIGHT_TRAVERSE(interpreter.this.sequence_number(),
				   words['x'],words['y'],words['z'],
				   0,0,0,0,0,0)

# the oword params #1..#30 are not used in remapped codes
# any words as per argspec come through the words dict
def m310(callargs,**words):
	print >> sys.stderr, "----- m310:"
#	try:
	for key in words:
		canon.MESSAGE("word '%s' = %s" % (key, str(words[key])))

	for i in range(1,5400):
		if abs(interpreter.params[i]) > 0.1:
			print "param ",i,"=",interpreter.params[i]

	# access into Interp
	line = interpreter.this.sequence_number()
	canon.MESSAGE("current line= " + str(line))
#	interpreter.this.params[123] =2.71828
#	canon.MESSAGE("blocktext = " + interpreter.this.blocktext)
	canon.MESSAGE("call_level  = " + str(interpreter.this.call_level))
	canon.MESSAGE("remap_level  = " + str(interpreter.this.remap_level))

	# demo queued canon interface
	if (words.has_key('r')):
		if words['r'] > 0:
			canon.FLOOD_ON()
		else:
			canon.FLOOD_OFF()
		#	l = 4711 / 0
	# except Exception,err:
	# 	print >> sys.stderr, "----- m310 exception"
	# 	canon.MESSAGE("exception: " + str(err) + ":" + traceback.format_exc())
	# 	raise

def m311(callargs,**words):
	print >> sys.stderr, "----- m311:"
	try:
		print "curp=",	interpreter.this.current_pocket
		print "sel=",interpreter.this.selected_pocket
		print "tc=",interpreter.this.toolchange_flag
		if (words.has_key('r')):
			if words['r'] > 0:
				interpreter.this.selected_pocket = int(words['r'])
				interpreter.this.toolchange_flag = True
			else:
				interpreter.this.current_pocket = int(words['r'])
				interpreter.this.toolchange_flag = False
	except Exception,err:
		print >> sys.stderr, "----- m311 exception"
		canon.MESSAGE("exception: " + str(err) + ":" + traceback.format_exc())
		raise

def m312(callargs,**words):
	print "p[4711]=",interpreter.params[4711]
	interpreter.params[4711] = 3.1415926

def m313(callargs,**words):
	print "assigning p['_foo']=4711"
	interpreter.params['_foo'] = 4711.0


def m250(callargs,**words):
	i = interpreter.interp
	print "py M250 call_level=",i.call_level,"remap_level=",i.remap_level
	#printobj(callargs,"callargs")
	#printobj(words,"words")

def fooprolog(*args,**words):
	print "py fooprolog args=",args
	i = interpreter.interp
	print "call_level=",i.call_level,"remap_level=",i.remap_level
	for key in words:
		print  "word '%s' = %f" % (key, words[key])

def addlocals(args,**words):
	print "py addlocals args=",args
	i = interpreter.interp
	print "seqno=",i.sequence_number()
	print "call_level=",i.call_level,"remap_level=",i.remap_level
	for key in words:
		print  "word '%s' = %f" % (key, words[key])

	interpreter.params[123] =2.71828
	interpreter.params["fooparam"] = 4711.0


def fooepilog(*args,**words):
	print "py fooepillog args=",args
	i = interpreter.interp
	print "call_level=",i.call_level,"remap_level=",i.remap_level
	for key in words:
		print  "word '%s' = %f" % (key, words[key])

def plainsub(args,**words):
	print "executing Python function: %s.%s " % (globals()['__name__'],sys._getframe(0).f_code.co_name)
	i = interpreter.interp
	print "call_level=",i.call_level,"remap_level=",i.remap_level

	print "args = ",args
	for i in range(len(args)):
		print "args[%d] = " % (i), args[i]
	if words:
		for key in words:
			print  "word '%s' = %f" % (key, words[key])
	else:
		print "no words passed"
	return 47.11

# prolog for prepare NGC file
def p_prepare(userdata,**words):
	i = interpreter.interp
	e = i.eblock
	toolno = e.t_number
	p = interpreter.params
	(status,pocket) = i.find_tool_pocket(toolno)
	if status != INTERP_OK:
		i.push_errormsg("py: pocket not found")
		return (status,)
	canon.MESSAGE("p_prepare: call_level="+str(i.call_level)+" remap_level="+str(i.remap_level)+" t_number=" + str(toolno) + " pocket=" + str(pocket))

	p["tool"] = toolno
	p["pocket"] = pocket
	print "tool=",	p["tool"],"pocket=", p["pocket"]
	return (INTERP_OK,)

# epilog for prepare NGC file
def e_prepare(userdata,**words):
	i = interpreter.interp
	retval = interpreter.this.return_value

	canon.MESSAGE("p_prepare: call_level="+str(i.call_level)+" remap_level="+str(i.remap_level) + " retval=" + str(retval))

	if retval > 0:
		interpreter.this.selected_pocket = int(retval)
		canon.SELECT_POCKET(int(retval))
	else:
		canon.INTERP_ABORT(-4711,"e_prepare abort")
	return (INTERP_OK,)



def null_remap(userdata,**words):
	print "--null_remap"
	return (INTERP_OK,)

#REMAP=T argspec=tq-  python=prepare

# handle prepare completely in Python
def prepare(userdata,**words):
	print "executing Python function: %s.%s " % (globals()['__name__'],sys._getframe(0).f_code.co_name)
	i = interpreter.interp
	p = interpreter.params
	e = i.eblock

	print "call_level=",i.call_level,"remap_level=",i.remap_level
	for key in words:
		print  "word '%s' = %f" % (key, words[key])
	toolno = e.t_number
	print "t_number=",toolno
	(status,pocket) = i.find_tool_pocket(toolno)
	if status != INTERP_OK:
		i.push_errormsg("py: pocket not found")
		return (status,)
	interpreter.this.selected_pocket = pocket
	canon.SELECT_POCKET(pocket)
	return (INTERP_OK,)

def whoami(userdata,**words):
	print "executing Python function: %s.%s(%d,words)" % (globals()['__name__'],sys._getframe(0).f_code.co_name,userdata)
	i = interpreter.interp
	print "call_level=",i.call_level,"remap_level=",i.remap_level
	for key in words:
		print  "word '%s' = %f" % (key, words[key])

	# test OK return on first call
	if (words.has_key('p')):
		return (INTERP_OK,)

	if userdata > 0:
		# we were called post-sync():
		pin_status = canon.GET_EXTERNAL_DIGITAL_INPUT(0,0);
		print "pin status=",pin_status
		return (INTERP_OK,) # done
	else:
		# wait for digital-input 00 to go hi for 5secs
		canon.WAIT(0,1,2,5.0)
		# pls call again after sync() with new userdata value
		return (INTERP_EXECUTE_FINISH,userdata + 1)

#	i.push_errormsg("set error entry in whoami")


def m314(callargs,**words):
	i = interpreter.interp
	print "m314:call_level=",i.call_level,"remap_level=",i.remap_level

def m315(callargs,**words):
	i = interpreter.interp
	c = i.cblock
	e = i.eblock
	#r = i.remap
	print "---> in py.m315()"
	#printobj(i.remap,"Remap:")
#	printobj(c,"cblock")
#	printobj(e,"eblock")


def g887(callargs,**words):
	i = interpreter.interp
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
		interpreter.this.selected_pocket = int(args[0])
		canon.SELECT_POCKET(int(args[0]))
	else:
		canon.INTERP_ABORT(-4711,"pytdemo abort")

	return args[1] # return pocket number (#2) to commit or -1 to fail


#try:
print "dir(interpreter)=",dir(interpreter)
	#print "dir(interpreter.interp)=",dir(interpreter.interp)
	#print "dir(interpreter.this.do)=",dir(interpreter.this.do)
	#print "do = ",interpreter.this.do(123)

	#print "dir(interpreter.this.foo4)=",dir(interpreter.this.foo4)
	#print "dir(interpreter.this.ps)",dir(interpreter.this.ps)
	#print "dir(interpreter.this.invite)",dir(interpreter.this.invite)
	#print "str(interpreter.this.invite)",str(interpreter.this.invite)
	#print "interpreter.this.foo4=",interpreter.this.foo4
	#print "interpreter.this.invite()",interpreter.this.invite()
	#print "interpreter.this.blocktext",interpreter.this.blocktext
	#print "interpreter.this.call_level",interpreter.this.call_level
	#print "interpreter.this.cl()=",interpreter.this.cl()
	#print "interpreter.this.value=",interpreter.this.value
	#interpreter.this.value = True
	#print "set true: interpreter.this.value=",interpreter.this.value

for i in [5220,"_metric","_absolute","_tool_offset","_feed","_rpm"]:
	print "param",i,"=",interpreter.params[i]





#except Exception,err:
#	print "exception: " + str(err) + ":" + traceback.format_exc()




def add_args(code,):
	# access argspec -> need code
	# accesss cblock - ok
	# store into current callframe  dict - dict should be in place
	# report failure with message
	# incicate success to continue
	pass
